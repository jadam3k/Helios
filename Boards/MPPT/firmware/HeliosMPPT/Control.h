#pragma once
#include <stdint.h>
#include <math.h>
#include "Config.h"
namespace helios {
inline float clamp(float x,float a,float b) { return x<a?a:(x>b?b:x); }
inline bool elapsed(uint32_t n,uint32_t t,uint32_t d) { return uint32_t(n-t)>=d; }
enum class State:uint8_t { Unconfigured, Stopped, Waiting, Qualifying, Charging, Complete, Fault };
enum class Fault:uint8_t { None, Sample, Battery, Panel, Current, Reverse, Temperature, Timeout, Heartbeat, PowerGood };
struct Sample {
    float battery=0,panel=0,converter=0,load=0,temperature=0;
    uint32_t at=0;
    bool valid=false;
};
struct Output {
    State state=State::Stopped;
    Fault fault=Fault::None;
    float duty=0,power=0,batteryCurrent=0;
    bool load=false;
};
class Controller {
public:
    void start() { requested_=true; }
    void stop() { requested_=false;out_.duty=0;out_.load=false; }
    void trip(Fault f) { out_.fault=f;out_.state=State::Fault;out_.duty=0;out_.load=false;requested_=false; }
    // CLEAR never starts charging: START is a separate action.
    void clear() { if(out_.state==State::Fault) {out_=Output();requested_=false;} }
    Output output()const {return out_;}
    Output update(const Sample&s,uint32_t now,bool configured) {
        if(out_.state==State::Fault)return out_;
        if(!configured) {out_=Output();out_.state=State::Unconfigured;return out_;}
        if(!requested_) {out_=Output();return out_;}
        if(!s.valid||elapsed(now,s.at,21)||!isfinite(s.battery)||!isfinite(s.panel)||
            !isfinite(s.converter)||!isfinite(s.load)||!isfinite(s.temperature))return fail(Fault::Sample);
        if(s.battery<cfg::batteryMinimum||s.battery>=cfg::batteryTripVoltage)return fail(Fault::Battery);
        if(s.panel<0||s.panel>=cfg::panelMaximum)return fail(Fault::Panel);
        if(s.converter>cfg::tripConverterA||s.load>cfg::tripLoadA)return fail(Fault::Current);
        if(s.converter<-.35f||s.load<-.35f)return fail(Fault::Reverse);
        if(s.temperature<cfg::minimumC||s.temperature>cfg::maximumC)return fail(Fault::Temperature);
        out_.power=s.battery*s.converter;out_.batteryCurrent=s.converter-s.load;
        if(s.battery<=cfg::loadOffVoltage)out_.load=false;
        else if(s.battery>=cfg::loadOnVoltage)out_.load=true;
        if(out_.state==State::Complete) {
            out_.duty=0;
            if(s.battery>cfg::rechargeVoltage)return out_;
            out_.state=State::Waiting;
        }
        const bool running=out_.state==State::Charging;
        const float headroom=running?cfg::stopHeadroom:cfg::startHeadroom;
        if(s.panel<s.battery+headroom) {
            out_.state=State::Waiting;out_.duty=0;tail_=false;return out_;
        }
        if(out_.state==State::Stopped||out_.state==State::Unconfigured||out_.state==State::Waiting) {
            out_.state=State::Qualifying;sunAt_=now;out_.duty=0;
        }
        if(out_.state==State::Qualifying) {
            if(!elapsed(now,sunAt_,cfg::qualificationMs))return out_;
            out_.state=State::Charging;chargeAt_=now;tail_=false;
            // Diode mode allows a gradual rise without actively sinking current.
            out_.duty=clamp(s.battery/s.panel-.08f,cfg::dutyMinimum,cfg::dutyMaximum);
            ceiling_=clamp(s.battery/s.panel+.06f,cfg::dutyMinimum,cfg::dutyMaximum);
            previousPower_=out_.power;perturbAt_=now;lastStep_=now;direction_=1;
        }
        if(elapsed(now,chargeAt_,cfg::chargeTimeoutMs))return fail(Fault::Timeout);
        if(s.battery>=cfg::chargeVoltage-.04f&&out_.batteryCurrent>=0&&out_.batteryCurrent<=cfg::terminationA) {
            if(!tail_){tail_=true;tailAt_=now;}
            if(elapsed(now,tailAt_,cfg::terminationMs)) {
                out_.state=State::Complete;out_.duty=0;return out_;
            }
        }else tail_=false;
        // Caller supplies a fresh sample each millisecond. Regulator is time-gated.
        if(!elapsed(now,lastStep_,1))return out_;
        lastStep_=now;
        const float vError=cfg::chargeVoltage-s.battery;
        const float iError=cfg::maxConverterA-s.converter;
        if(vError<-.03f)voltageHold_=true;
        if(voltageHold_) {
            out_.duty=0;
            if(vError<=.03f)return out_;
            voltageHold_=false;
            out_.duty=clamp(s.battery/s.panel-.08f,cfg::dutyMinimum,cfg::dutyMaximum);
        }
        if(vError<0||iError<0) {
            out_.duty-=clamp(-vError*.008f+.0002f, .0002f,.004f);
            if(iError<0)out_.duty-=clamp(-iError*.003f,0,.004f);
            // Rebase P&O after limiting so a load change is not mistaken for a perturbation.
            previousPower_=out_.power;perturbAt_=now;
        }else {
            if(elapsed(now,perturbAt_,250)) {
                if(out_.power<previousPower_-.1f)direction_=-direction_;
                ceiling_=clamp(ceiling_+direction_*.002f,cfg::dutyMinimum,cfg::dutyMaximum);
                previousPower_=out_.power;perturbAt_=now;
            }
            if(vError>.02f&&iError>.05f)out_.duty+=.0005f;
        }
        // P&O varies a duty ceiling; CC and CV can always reduce actual duty.
        out_.duty=clamp(out_.duty,cfg::dutyMinimum,ceiling_);
        return out_;
    }
private:
    Output fail(Fault f){trip(f);return out_;}
    Output out_;
    bool requested_=false,tail_=false,voltageHold_=false;
    uint32_t sunAt_=0,chargeAt_=0,tailAt_=0,lastStep_=0,perturbAt_=0;
    float ceiling_=0,previousPower_=0;
    int direction_=1;
};
struct Compare {uint32_t high,low;};
inline Compare compareFor(float duty,uint32_t arr,uint32_t gap) {
    if(!(duty>0)||!isfinite(duty))return {0,arr+1};
    const uint32_t high=uint32_t(clamp(duty,cfg::dutyMinimum,cfg::dutyMaximum)*arr);
    return {high,high+gap<arr?high+gap:arr+1};
}
}
