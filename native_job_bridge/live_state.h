#pragma once
#include "../dispatch_shared/protocol.h"
#include "../dispatch_shared/payload.h"
#include "../dispatch_shared/trailer_combination.h"
#include "scssdk_telemetry.h"

namespace live {
inline std::string attribute(const scs_named_value_t* a,const char* name) {
    if(a)for(;a->name;++a)if(strcmp(a->name,name)==0&&a->value.type==SCS_VALUE_TYPE_string&&a->value.value_string.value)return a->value.value_string.value;
    return {};
}
struct Job {
    bool active=false,loadedKnown=false,loaded=false;
    unsigned revision=0;Route route;std::string market;uint64_t income=0;
    uint32_t units=0;float massKg=0,unitKg=0;bool payloadKnown=false;
    void update(const scs_named_value_t* a) {
        ++revision;active=a&&a->name;loadedKnown=loaded=false;income=0;units=0;massKg=unitKg=0;payloadKnown=false;unsigned payloadFields=0;
        route={attribute(a,"cargo.id"),attribute(a,"source.city.id"),attribute(a,"source.company.id"),attribute(a,"destination.city.id"),attribute(a,"destination.company.id")};market=attribute(a,"job.market");
        if(a)for(;a->name;++a){if(strcmp(a->name,"cargo.loaded")==0&&a->value.type==SCS_VALUE_TYPE_bool){loadedKnown=true;loaded=a->value.value_bool.value!=0;}if(strcmp(a->name,"income")==0&&a->value.type==SCS_VALUE_TYPE_u64)income=a->value.value_u64.value;
            if(strcmp(a->name,"cargo.mass")==0&&a->value.type==SCS_VALUE_TYPE_float){massKg=a->value.value_float.value;payloadFields|=1;}
            if(strcmp(a->name,"cargo.unit.mass")==0&&a->value.type==SCS_VALUE_TYPE_float){unitKg=a->value.value_float.value;payloadFields|=2;}
            if(strcmp(a->name,"cargo.unit.count")==0&&a->value.type==SCS_VALUE_TYPE_u32){units=a->value.value_u32.value;payloadFields|=4;}}
        payloadKnown=payloadFields==7;
    }
    bool matches(const Route& r) const {return active&&market=="cargo_market"&&routeJson(route)==routeJson(r);}
    bool assigned(const Route& r,unsigned before) const {return revision>before&&matches(r)&&loadedKnown&&!loaded;}
    bool confirmedLoaded(const Route& r,unsigned before) const {return revision>before&&matches(r)&&loadedKnown&&loaded;}
    bool payloadMatches(uint32_t expected,double expectedUnitKg) const {return payloadKnown&&matchingPayload(units,massKg,expected,expectedUnitKg);}
    Json json() const {return {{"active",active},{"loaded_known",loadedKnown},{"cargo_loaded",loaded},{"route",routeJson(route)},{"market",market},{"income",income},{"payload_known",payloadKnown},{"cargo_units",units},{"cargo_mass_kg",massKg},{"cargo_unit_mass_kg",unitKg}};}
};
struct Trailer {
    bool configured=false,connected=false;std::string id,body,chain;
    TrailerCombination combination;
    bool supported() const {return configured&&combination.supported();}
    void update(const scs_named_value_t* a) {configured=a&&a->name;id=attribute(a,"id");body=attribute(a,"body.type");chain=attribute(a,"chain.type");}
    std::string signature() const {
        const auto shape=combination.signature();if(!configured||shape.empty()||id.empty()||body.empty()||chain.empty())return {};
        const auto base=id+"|"+body+"|"+chain;
        // Preserve recovery of existing single-trailer jobs across this update.
        return combination.parts.size()==1?base:base+"|"+shape;
    }
    Json json() const {return {{"configured",configured},{"connected",connected},{"id",id},{"body_type",body},{"chain_type",chain},{"signature",signature()},{"combination_known",combination.known},{"trailer_count",combination.trailers()},{"dolly_count",combination.dollies()},{"supported",supported()},{"trailer_limit",trailerLimit()}};}
};
}
