#pragma once
#include <cmath>
#include <cstdint>

namespace live {
// The SDK stops updating most telemetry while paused. Capture only samples
// received just before the pause; clear them on every return to Drive.
struct MenuSnapshot {
    bool captured=false,speedKnown=false,ignitionKnown=false,ignitionOn=false;
    float speedMps=0;
    void beginDriving(){*this={};}
    void pause(uint64_t now,uint64_t speedStamp,float speed,uint64_t ignitionStamp,bool ignition) {
        *this={};captured=true;
        speedKnown=speedStamp&&now>=speedStamp&&now-speedStamp<2000&&std::isfinite(speed);
        ignitionKnown=ignitionStamp&&now>=ignitionStamp&&now-ignitionStamp<2000;
        if(speedKnown)speedMps=speed;if(ignitionKnown)ignitionOn=ignition;
    }
    bool stopped() const {return captured&&speedKnown&&std::fabs(speedMps)<=0.1f;}
};
}
