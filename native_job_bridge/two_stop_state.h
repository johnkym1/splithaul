#pragma once
#include "../dispatch_shared/protocol.h"
#include "../dispatch_shared/payload.h"

namespace live {
struct TwoStopRun {
    Request first;
    std::string state="none",message,currentId;
    uint32_t firstUnits=0,finalUnits=0;double unitMassKg=0;
    size_t dropIndex=0;
    size_t stopCount() const {return first.multiDrop()?first.dropRoutes.size():2;}
    const Route& currentRoute() const {return first.multiDrop()?first.dropRoutes.at(dropIndex):(dropIndex?first.finalRoute:first.route);}
    const Route& nextRoute() const {return first.multiDrop()?first.dropRoutes.at(dropIndex+1):first.finalRoute;}
    std::string nextId() const {return first.multiDrop()?first.dropIds.at(dropIndex+1):first.secondId;}
    Request handoffRequest(const std::string& session,uint64_t now) const {
        Request next;next.id=nextId();next.route=nextRoute();next.session=session;next.context=first.context;next.trailer=first.trailer;next.created=now;next.expires=now+120000;next.fullCapacity=first.fullCapacity;return next;
    }
    uint32_t carriedUnits() const {return first.multiDrop()?multiDropRemaining(firstUnits,static_cast<unsigned>(stopCount()),static_cast<unsigned>(dropIndex)):finalUnits;}
    bool enabled() const {return first.twoStop;}
    bool active() const {return enabled()&&state!="complete"&&state!="stopped"&&state!="uncertain";}
    bool second() const {return state=="second_assigning"||state=="second_loading"||state=="second_active";}
    void start(const Request& r) {first=r;state=r.twoStop?"first_active":"none";message.clear();currentId=r.id;firstUnits=finalUnits=0;unitMassKg=0;dropIndex=0;}
    bool delivered(const std::string& id) {
        if(!enabled())return false;
        if(first.multiDrop()) {
            if(id!=currentId||(state!="first_active"&&state!="second_active"))return false;
            if(dropIndex+1==stopCount()){state="complete";message="All deliveries completed.";}
            else {state="waiting_handoff";message="Delivery "+std::to_string(dropIndex+1)+" confirmed. Return to Drive stopped for the next loaded leg.";}
            return true;
        }
        if(state=="first_active"&&id==first.id){state="waiting_handoff";message="First delivery complete. Return to Drive stopped for the loaded final leg.";return true;}
        if(state=="second_active"&&id==first.secondId){state="complete";message="Both deliveries completed.";return true;}
        return false;
    }
    bool canHandoff(bool empty,bool sameContext,bool ready) {
        if(state!="waiting_handoff")return false;
        if(!sameContext){stop("The profile, vehicle or trailer changed before handover.");return false;}
        return empty&&ready;
    }
    void claimHandoff() {currentId=nextId();++dropIndex;state="second_assigning";message="Assigning delivery "+std::to_string(dropIndex+1)+" of "+std::to_string(stopCount())+".";}
    void stop(const std::string& why,bool uncertain=false) {state=uncertain?"uncertain":"stopped";message=why;}
    Json json(const std::string& session) const {
        Json value={{"protocol",protocolVersion},{"plan_id",first.id},{"first_request_id",first.id},{"second_request_id",first.secondId},{"active_request_id",currentId},{"session_id",session},{"state",state},{"message",message},{"first_route",routeJson(first.route)},{"final_route",routeJson(first.finalRoute)},{"remaining_percent",first.remainingPercent},{"initial_units",firstUnits},{"remaining_units",finalUnits},{"unit_mass_kg",unitMassKg},{"initial_mass_kg",firstUnits*unitMassKg},{"remaining_mass_kg",finalUnits*unitMassKg},{"updated_utc_ms",utcMs()}};
        if(first.multiDrop()) {
            value["multi_drop"]=true;value["stop_count"]=stopCount();value["current_stop"]=dropIndex+1;value["drops"]=Json::array();
            for(size_t i=0;i<stopCount();++i){const auto aboard=firstUnits?multiDropRemaining(firstUnits,static_cast<unsigned>(stopCount()),static_cast<unsigned>(i)):0;const auto after=firstUnits?multiDropRemaining(firstUnits,static_cast<unsigned>(stopCount()),static_cast<unsigned>(i+1)):0;value["drops"].push_back({{"request_id",first.dropIds[i]},{"route",routeJson(first.dropRoutes[i])},{"aboard_units",aboard},{"unload_units",aboard-after}});}
            value["remaining_units"]=firstUnits?carriedUnits():0;value["remaining_mass_kg"]=value["remaining_units"].get<uint32_t>()*unitMassKg;
        }
        return value;
    }
};
}
