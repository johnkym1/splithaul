#pragma once
#include "../dispatch_shared/protocol.h"

namespace live {
struct TwoStopRun {
    Request first;
    std::string state="none",message,currentId;
    uint32_t firstUnits=0,finalUnits=0;double unitMassKg=0;
    bool enabled() const {return first.twoStop;}
    bool active() const {return enabled()&&state!="complete"&&state!="stopped"&&state!="uncertain";}
    bool second() const {return state=="second_assigning"||state=="second_loading"||state=="second_active";}
    void start(const Request& r) {first=r;state=r.twoStop?"first_active":"none";message.clear();currentId=r.id;firstUnits=finalUnits=0;unitMassKg=0;}
    bool delivered(const std::string& id) {
        if(!enabled())return false;
        if(state=="first_active"&&id==first.id){state="waiting_handoff";message="First delivery complete. Return to Drive stopped for the loaded final leg.";return true;}
        if(state=="second_active"&&id==first.secondId){state="complete";message="Both deliveries completed.";return true;}
        return false;
    }
    bool canHandoff(bool empty,bool sameContext,bool ready) {
        if(state!="waiting_handoff")return false;
        if(!sameContext){stop("The profile, vehicle or trailer changed before handover.");return false;}
        return empty&&ready;
    }
    void claimHandoff() {state="second_assigning";message="Assigning the loaded final leg.";currentId=first.secondId;}
    void stop(const std::string& why,bool uncertain=false) {state=uncertain?"uncertain":"stopped";message=why;}
    Json json(const std::string& session) const {
        return {{"protocol",protocolVersion},{"plan_id",first.id},{"first_request_id",first.id},{"second_request_id",first.secondId},{"active_request_id",currentId},{"session_id",session},{"state",state},{"message",message},{"first_route",routeJson(first.route)},{"final_route",routeJson(first.finalRoute)},{"remaining_percent",first.remainingPercent},{"initial_units",firstUnits},{"remaining_units",finalUnits},{"unit_mass_kg",unitMassKg},{"initial_mass_kg",firstUnits*unitMassKg},{"remaining_mass_kg",finalUnits*unitMassKg},{"updated_utc_ms",utcMs()}};
    }
};
}
