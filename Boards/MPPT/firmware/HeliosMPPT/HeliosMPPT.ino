/* Helios MPPT / STM32L072CZT6 / original PCB / Arduino STM32 core.
 * All four source files in this folder belong to this sketch.
 * Configuration and calibration are in Config.h. No RJ45/8P8C link is used.
 */
#include <Arduino.h>
#include <HardwareTimer.h>
#include <stm32l0xx_ll_adc.h>
#include <math.h>
#include "Config.h"
#include "Control.h"
#include "PowerStage.h"
#if !defined(STM32L072xx)
#error "Select Generic STM32L072CZ, LQFP48, 192 KB Flash."
#endif

PowerStage stage;
helios::Controller controller;
HardwareTimer heartbeatTimer(TIM21);
Uart console(PA3,PA2); // STM32 core 3.1 UART2. No fitted debug connector.

struct Telemetry {
    float vdda,batteryV,panelV,converterA,loadA,batteryA,temperatureC,ntcOhm;
    float outputW,requestedDuty,appliedDuty;
    uint32_t sampleUs,timerTop,deadTimeTicks;
    uint16_t rawBattery,rawPanel,rawConverter,rawLoad,rawNtc;
    uint8_t state,fault;
    bool configured,powerGood,synchronous,loadEnabled;
};
volatile Telemetry telemetry={};
// SWD commands: 1 START, 2 STOP, 3 CLEAR. Also available on optional UART2.
volatile uint32_t command=0;
volatile uint32_t heartbeatMs=0,lastHealthyMs=0;
volatile bool heartbeatFault=false;
uint32_t lastSampleUs=0,lastReportMs=0,lastPgFailMs=0;
uint32_t pgGoodAtMs=0;
bool pgWasGood=false;
bool configured=false;
float filteredBattery=0,filteredPanel=0,filteredConverter=0,filteredLoad=0;
bool filtersReady=false;

static void outputLow(uint32_t pin) {
    digitalWrite(pin,LOW);pinMode(pin,OUTPUT);
}
static void safetyTick() {
    ++heartbeatMs;
    if(stage.active()&&uint32_t(heartbeatMs-lastHealthyMs)>20) {
        stage.stop();GPIOC->BSRR=uint32_t(GPIO_PIN_13)<<16;
        heartbeatFault=true;
    }
}
static bool startWatchdog() {
    RCC->CSR|=RCC_CSR_LSION;
    const uint32_t start=millis();
    while(!(RCC->CSR&RCC_CSR_LSIRDY))if(helios::elapsed(millis(),start,100))return false;
    IWDG->KR=0x5555;IWDG->PR=4;IWDG->RLR=249; // ~500 ms at nominal 32 kHz /64.
    IWDG->KR=0xCCCC;IWDG->KR=0xAAAA;
    return true;
}
static uint16_t readAdc(uint32_t pin,unsigned count=4) {
    analogRead(pin);
    uint32_t sum=0;
    for(unsigned i=0;i<count;++i)sum+=analogRead(pin);
    return uint16_t((sum+count/2)/count);
}
static float adcVolts(uint16_t raw,float vdda) {return raw*vdda/4095.0f;}
static helios::Sample measure(uint32_t nowMs) {
    const uint32_t started=micros();
    const uint16_t ref=readAdc(AVREF);
    const float supply=ref?__LL_ADC_CALC_VREFANALOG_VOLTAGE(ref,LL_ADC_RESOLUTION_12B)/1000.0f*cfg::vddaCalibration:NAN;
    const uint16_t bat=readAdc(PA0),pv=readAdc(PA1),conv=readAdc(PA6),load=readAdc(PA5),ntc=readAdc(PA7);
    helios::Sample s;s.at=nowMs;
    s.battery=adcVolts(bat,supply)*cfg::voltageRatio*cfg::batteryVoltageCalibration;
    s.panel=adcVolts(pv,supply)*cfg::voltageRatio*cfg::panelVoltageCalibration;
    const float offset=supply*2048.0f/4095.0f;
    s.converter=(adcVolts(conv,supply)-offset-cfg::converterZeroCorrectionV)/(cfg::shuntOhm*cfg::currentGain);
    s.load=(adcVolts(load,supply)-offset-cfg::loadZeroCorrectionV)/(cfg::shuntOhm*cfg::currentGain);
    float resistance=NAN;s.temperature=NAN;
    if(ntc>2&&ntc<4093) {
        resistance=cfg::ntcPullup*ntc/(4095.0f-ntc);
        s.temperature=1.0f/(1.0f/298.15f+logf(resistance/cfg::ntcR25)/cfg::ntcBeta)-273.15f;
    }
    s.valid=supply>2.9f&&supply<3.6f&&bat>2&&bat<4093&&pv<4093&&
        conv>2&&conv<4093&&load>2&&load<4093&&ntc>2&&ntc<4093;
    telemetry.vdda=supply;telemetry.rawBattery=bat;telemetry.rawPanel=pv;
    telemetry.rawConverter=conv;telemetry.rawLoad=load;telemetry.rawNtc=ntc;
    telemetry.ntcOhm=resistance;telemetry.temperatureC=s.temperature;
    telemetry.sampleUs=micros()-started;
    // Use raw extremes for shutdown before applying the slow telemetry filter.
    if(configured&&s.valid) {
        if(s.battery>=cfg::batteryTripVoltage)controller.trip(helios::Fault::Battery);
        if(s.panel>=cfg::panelMaximum)controller.trip(helios::Fault::Panel);
        if(s.converter>cfg::tripConverterA||s.load>cfg::tripLoadA)controller.trip(helios::Fault::Current);
        if(s.converter<-.35f)controller.trip(helios::Fault::Reverse);
    }
    if(!filtersReady) {
        filteredBattery=s.battery;filteredPanel=s.panel;
        filteredConverter=s.converter;filteredLoad=s.load;filtersReady=true;
    } else {
        filteredBattery+=.25f*(s.battery-filteredBattery);filteredPanel+=.25f*(s.panel-filteredPanel);
        filteredConverter+=.25f*(s.converter-filteredConverter);filteredLoad+=.25f*(s.load-filteredLoad);
    }
    if(!s.valid||!isfinite(s.battery)||!isfinite(s.panel))filtersReady=false;
    s.battery=filteredBattery;s.panel=filteredPanel;s.converter=filteredConverter;s.load=filteredLoad;
    telemetry.batteryV=s.battery;telemetry.panelV=s.panel;
    telemetry.converterA=s.converter;telemetry.loadA=s.load;
    return s;
}
static void executeCommand() {
    if(cfg::uartTelemetry&&console.available()) {
        const char c=console.read();
        if(c=='s'||c=='S')command=1;
        else if(c=='x'||c=='X')command=2;
        else if(c=='c'||c=='C')command=3;
    }
    const uint32_t requested=command;command=0;
    if(requested==1&&configured)controller.start();
    if(requested==2){controller.stop();stage.stop();digitalWrite(PC13,LOW);}
    if(requested==3) {
        stage.stop();controller.clear();heartbeatFault=false;
        lastHealthyMs=heartbeatMs;
    }
}
void setup() {
    outputLow(PB0);outputLow(PB1);outputLow(PC13);
    outputLow(PB10);outputLow(PC14);outputLow(PA8);
    pinMode(PC15,INPUT);
    analogReadResolution(12);analogWriteResolution(12);
    analogWrite(PA4,2048); // INA2181A1 reference; PA4 is a hardware DAC pin.
    digitalWrite(PC14,HIGH); // PV DIVIDER enable, not power-stage enable.
    digitalWrite(PB10,HIGH); // 5 V buck enable.
    delay(50);
    stage.begin();
    telemetry.timerTop=stage.top();telemetry.deadTimeTicks=stage.gap();
    configured=cfg::batteryConfirmed&&cfg::calibrationConfirmed&&stage.valid();
    telemetry.configured=configured;
    if(cfg::uartTelemetry)console.begin(115200);
    heartbeatTimer.setOverflow(1000,HERTZ_FORMAT);
    heartbeatTimer.attachInterrupt(safetyTick);heartbeatTimer.resume();
    if(!startWatchdog()){configured=false;controller.trip(helios::Fault::Heartbeat);}
    if(cfg::autoStart&&configured)controller.start();
    lastSampleUs=micros();lastReportMs=millis();pgGoodAtMs=millis();
}
void loop() {
    executeCommand();
    if(heartbeatFault)controller.trip(helios::Fault::Heartbeat);
    const uint32_t nowUs=micros(),nowMs=millis();
    if(uint32_t(nowUs-lastSampleUs)>=1000) {
        lastSampleUs=nowUs;
        auto s=measure(nowMs);
        const bool good=digitalRead(PC15)==LOW; // Q8 inverts U8 power-good.
        telemetry.powerGood=good;
        if(good) {lastPgFailMs=nowMs;if(!pgWasGood)pgGoodAtMs=nowMs;}
        else {pgGoodAtMs=nowMs;if(pgWasGood)lastPgFailMs=nowMs;}
        pgWasGood=good;
        if(!good&&helios::elapsed(nowMs,lastPgFailMs,1000)&&configured)
            controller.trip(helios::Fault::PowerGood);
        auto result=controller.update(s,nowMs,configured);
        // 100 ms of stable 5 V required before connecting loads or switching.
        const bool allowed=good&&helios::elapsed(nowMs,pgGoodAtMs,100)&&!heartbeatFault;
        bool synchronous=false;
        if(cfg::synchronous&&result.duty>0) {
            const float ripple=fmaxf(0,(s.panel-s.battery))*result.duty/(cfg::inductanceH*cfg::pwmHz);
            synchronous=s.converter>ripple*.5f+.5f;
        }
        const uint32_t irq=__get_PRIMASK();__disable_irq();
        if(!s.valid||result.state==helios::State::Fault||!allowed||heartbeatFault)stage.stop();
        else {
            lastHealthyMs=heartbeatMs;
            stage.apply(result.duty,synchronous);
        }
        digitalWrite(PC13,result.load&&allowed?HIGH:LOW);
        if(!irq)__enable_irq();
        telemetry.state=uint8_t(result.state);telemetry.fault=uint8_t(result.fault);
        telemetry.outputW=result.power;telemetry.batteryA=result.batteryCurrent;
        telemetry.requestedDuty=result.duty;telemetry.appliedDuty=stage.active()?result.duty:0;
        telemetry.synchronous=synchronous&&stage.active();telemetry.loadEnabled=result.load&&allowed;
        // Both safety mechanisms are fed only after returning from a complete sample/control pass.
        IWDG->KR=0xAAAA;
    }
    // PB10 remains enabled: switching this rail off would return the LDO to battery input.
    if(cfg::uartTelemetry&&helios::elapsed(nowMs,lastReportMs,1000)) {
        lastReportMs=nowMs;
        console.print("state=");console.print(telemetry.state);
        console.print(" fault=");console.print(telemetry.fault);
        console.print(" battery=");console.print(telemetry.batteryV,3);
        console.print(" panel=");console.print(telemetry.panelV,3);
        console.print(" converterA=");console.print(telemetry.converterA,3);
        console.print(" batteryA=");console.print(telemetry.batteryA,3);
        console.print(" C=");console.print(telemetry.temperatureC,1);
        console.print(" duty=");console.println(telemetry.appliedDuty,4);
    }
}
