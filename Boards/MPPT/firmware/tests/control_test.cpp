#include "../HeliosMPPT/Control.h"
#include <assert.h>
#include <stdio.h>
#include <limits>
using namespace helios;
static Sample sample(uint32_t t) {
    Sample s;s.battery=11.8f;s.panel=18.8f;s.converter=1;s.load=.2f;
    s.temperature=25;s.at=t;s.valid=true;return s;
}
static Controller charging() {
    Controller c;c.start();c.update(sample(0),0,true);
    c.update(sample(cfg::qualificationMs),cfg::qualificationMs,true);return c;
}
int main() {
    unsigned checks=0;
    Controller c;c.start();
    assert(c.update(sample(0),0,false).state==State::Unconfigured);++checks;
    assert(c.update(sample(1),1,true).state==State::Qualifying);++checks;
    assert(c.update(sample(3001),3001,true).state==State::Charging);++checks;
    c.stop();assert(c.update(sample(3002),3002,true).duty==0);++checks;
    c=charging();auto s=sample(3001);s.panel=5;
    assert(c.update(s,3001,true).state==State::Waiting);++checks;
    c=charging();s=sample(3001);s.converter=3;
    assert(c.update(s,3001,true).fault==Fault::Current);++checks;
    c.start();assert(c.update(sample(3002),3002,true).state==State::Fault);++checks;
    c.clear();assert(c.update(sample(3003),3003,true).state==State::Stopped);++checks;
    c=charging();s=sample(3001);s.battery=13;
    assert(c.update(s,3001,true).fault==Fault::Battery);++checks;
    c=charging();s=sample(3001);s.panel=30;
    assert(c.update(s,3001,true).fault==Fault::Panel);++checks;
    c=charging();s=sample(3001);s.temperature=50;
    assert(c.update(s,3001,true).fault==Fault::Temperature);++checks;
    c=charging();s=sample(3001);s.converter=-.5f;
    assert(c.update(s,3001,true).fault==Fault::Reverse);++checks;
    c=charging();s=sample(3001);s.valid=false;
    assert(c.update(s,3001,true).fault==Fault::Sample);++checks;
    c=charging();assert(c.update(sample(3001),3022,true).fault==Fault::Sample);++checks;
    c=charging();s=sample(3001);s.load=std::numeric_limits<float>::quiet_NaN();
    assert(c.update(s,3001,true).fault==Fault::Sample);++checks;
    c=charging();s=sample(3001);s.battery=cfg::chargeVoltage;s.converter=.3f;s.load=.2f;
    c.update(s,3001,true);s.at+=cfg::terminationMs;
    assert(c.update(s,s.at,true).state==State::Complete);++checks;
    s=sample(s.at+1);s.battery=12.3f;
    assert(c.update(s,s.at,true).state==State::Complete);++checks;
    s=sample(s.at+1);assert(c.update(s,s.at,true).state==State::Qualifying);++checks;
    c=charging();s=sample(cfg::qualificationMs+cfg::chargeTimeoutMs);
    assert(c.update(s,s.at,true).fault==Fault::Timeout);++checks;
    c=charging();s=sample(3001);s.battery=cfg::chargeVoltage+.05f;
    assert(c.update(s,s.at,true).duty==0);++checks;
    s.at++;s.battery=cfg::chargeVoltage-.01f;assert(c.update(s,s.at,true).duty==0);++checks;
    s.at++;s.battery=cfg::chargeVoltage-.06f;assert(c.update(s,s.at,true).duty>0);++checks;
    c=charging();s=sample(3001);s.battery=10.7f;
    assert(!c.update(s,s.at,true).load);s.at++;s.battery=11.3f;
    assert(!c.update(s,s.at,true).load);s.at++;s.battery=11.8f;
    assert(c.update(s,s.at,true).load);++checks;
    // Discrete timer model: check both directions of the center-aligned counter.
    for(unsigned d=3;d<=90;d++) {
        auto compare=compareFor(d/100.0f,400,16);
        assert(compare.low>=compare.high+16);
        for(unsigned cnt=0;cnt<=400;cnt++)
            assert(!((cnt<compare.high)&&(cnt>=compare.low)));
        for(int cnt=400;cnt>=0;cnt--)
            assert(!((unsigned(cnt)<=compare.high)&&(unsigned(cnt)>compare.low)));
    }
    assert(compareFor(0,400,16).high==0);++checks;
    assert(elapsed(0x20,0xfffffff0,40));++checks;
    printf("Passed %u checks; 88 duty settings checked in both count directions.\n",checks);
}
