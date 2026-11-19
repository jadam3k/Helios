#pragma once
#include <Arduino.h>
#include "Control.h"
class PowerStage {
public:
    void begin() {
        __HAL_RCC_GPIOB_CLK_ENABLE();__HAL_RCC_TIM3_CLK_ENABLE();
        TIM3->CR1=0;TIM3->CCER=0;
        gpioOff();
        uint32_t clock=HAL_RCC_GetPCLK1Freq();
        if((RCC->CFGR&RCC_CFGR_PPRE1)!=0)clock*=2;
        arr_=clock/(2*cfg::pwmHz);
        gap_=uint32_t((uint64_t(clock)*cfg::deadTimeNs+999999999)/1000000000);
        if(arr_<100||gap_==0||gap_>=arr_/10){valid_=false;return;}
        TIM3->PSC=0;TIM3->ARR=arr_;
        TIM3->CCR3=0;TIM3->CCR4=arr_+1;
        // CH3 PWM1 and CH4 PWM2, both preloaded, single center-aligned counter.
        TIM3->CCMR2=(6UL<<4)|TIM_CCMR2_OC3PE|(7UL<<12)|TIM_CCMR2_OC4PE;
        TIM3->CR1=TIM_CR1_ARPE|TIM_CR1_CMS_0;
        TIM3->EGR=TIM_EGR_UG;TIM3->SR=0;
        valid_=true;
    }
    bool valid()const{return valid_;}
    bool active()const{return active_;}
    uint32_t top()const{return arr_;}
    uint32_t gap()const{return gap_;}
    void stop() {
        TIM3->CCER=0;TIM3->CR1&=~TIM_CR1_CEN;
        gpioOff();active_=false;
    }
    void apply(float duty,bool synchronous) {
        if(!valid_||!(duty>0)||!isfinite(duty)){stop();return;}
        auto c=helios::compareFor(duty,arr_,gap_);
        if(!active_) {
            // Short bootstrap refresh. With 47 uH and 12.6 V, 2 us implies
            // about 0.54 A ideal reverse-current change. Validate actual pulse.
            gpioOff();GPIOB->BSRR=GPIO_PIN_1;
            delayMicroseconds(2);GPIOB->BSRR=uint32_t(GPIO_PIN_1)<<16;
            TIM3->CCER=0;TIM3->CCR3=c.high;TIM3->CCR4=arr_+1;
            TIM3->CNT=arr_;TIM3->EGR=TIM_EGR_UG;TIM3->SR=0;
            GPIO_InitTypeDef g={};g.Pin=GPIO_PIN_0|GPIO_PIN_1;
            g.Mode=GPIO_MODE_AF_PP;g.Pull=GPIO_PULLDOWN;g.Speed=GPIO_SPEED_FREQ_HIGH;g.Alternate=GPIO_AF2_TIM3;
            HAL_GPIO_Init(GPIOB,&g);
            TIM3->CCER=TIM_CCER_CC3E|TIM_CCER_CC4E;
            TIM3->CR1|=TIM_CR1_CEN;active_=true;
        }
        const uint32_t irq=__get_PRIMASK();__disable_irq();
        // UDIS prevents an update from latching one compare before the other.
        TIM3->CR1|=TIM_CR1_UDIS;
        TIM3->CCR3=c.high;TIM3->CCR4=synchronous?c.low:arr_+1;
        TIM3->CR1&=~TIM_CR1_UDIS;
        if(!irq)__enable_irq();
    }
private:
    static void gpioOff() {
        GPIOB->BSRR=uint32_t(GPIO_PIN_0|GPIO_PIN_1)<<16;
        GPIO_InitTypeDef g={};g.Pin=GPIO_PIN_0|GPIO_PIN_1;
        g.Mode=GPIO_MODE_OUTPUT_PP;g.Pull=GPIO_PULLDOWN;g.Speed=GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOB,&g);
    }
    volatile bool active_=false;
    bool valid_=false;
    uint32_t arr_=0,gap_=0;
};
