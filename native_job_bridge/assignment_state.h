#pragma once
#include <cstring>
#include "scssdk_telemetry.h"

// Telemetry is the independent acceptance check, not the native function's AL.
struct AssignmentTelemetry {
    bool active=false, matches=false, loadedKnown=false, loaded=false;
    unsigned revision=0;
    void update(const scs_named_value_t* attributes) {
        ++revision; active=attributes && attributes[0].name;
        matches=false; loadedKnown=false; loaded=false;
        if(!active)return;
        unsigned fields=0;
        for(auto a=attributes;a->name;++a) {
            if(a->value.type==SCS_VALUE_TYPE_bool && strcmp(a->name,"cargo.loaded")==0){loadedKnown=true;loaded=a->value.value_bool.value!=0;}
            if(a->value.type!=SCS_VALUE_TYPE_string || !a->value.value_string.value)continue;
            const char* expected=nullptr;unsigned bit=0;
            if(strcmp(a->name,"cargo.id")==0){expected="empty_palet";bit=1;}
            if(strcmp(a->name,"source.city.id")==0){expected="yakima";bit=2;}
            if(strcmp(a->name,"source.company.id")==0){expected="bn_farm";bit=4;}
            if(strcmp(a->name,"destination.city.id")==0){expected="wenatchee";bit=8;}
            if(strcmp(a->name,"destination.company.id")==0){expected="sg_whs";bit=16;}
            if(strcmp(a->name,"job.market")==0){expected="cargo_market";bit=32;}
            if(expected && strcmp(a->value.value_string.value,expected)==0)fields|=bit;
        }
        matches=fields==63;
    }
    bool confirmedUnloaded(unsigned before) const {
        return revision>before && active && matches && loadedKnown && !loaded;
    }
};
