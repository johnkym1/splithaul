#pragma once
#include <cstdint>
#include <string>

namespace live {
// A newly confirmed unloaded job gets at most one route reset. Never re-route
// after loading, a context change, timeout or a previous attempt.
struct PickupNavigation {
    bool pending=false,attempted=false;
    uint64_t deadline=0;
    std::string state="not_requested";
    void arm(uint64_t now) {pending=true;attempted=false;deadline=now+30000;state="waiting_stationary";}
    bool ready(bool matchingUnloaded,bool sameContext,bool driving,bool stationary,bool attached,uint64_t now) {
        if(!pending||attempted)return false;
        if(!matchingUnloaded||!sameContext){pending=false;state="skipped_job_changed";return false;}
        if(now>=deadline){pending=false;state="unavailable";return false;}
        return driving&&stationary&&attached;
    }
    void claim() {pending=false;attempted=true;state="attempting";}
    void finish(bool requested) {state=requested?"requested":"unavailable";}
};
}
