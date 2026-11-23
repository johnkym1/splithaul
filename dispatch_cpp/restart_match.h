#pragma once
#include "../dispatch_shared/protocol.h"

namespace ats {
// A positive result is a candidate, not a unique identity: ATS can contain
// different jobs with identical route and cargo. Never mutate the game here.
inline bool restartJobCandidate(const live::Json& receipt,const live::Json& status,const std::string& expectedId,const std::string& originalSession,uint64_t now) {
    try {
        if(!live::requestId(expectedId)||!live::requestId(originalSession)||!live::freshStatus(status,now)||status.at("session_id")==originalSession)return false;
        if(receipt.at("request_id")!=expectedId||receipt.at("session_id")!=originalSession)return false;
        const auto phase=receipt.at("state").get<std::string>();if(phase!="assigned"&&phase!="loaded")return false;
        const auto& old=receipt.at("job");const auto& current=status.at("job");
        if(!old.at("active").get<bool>()||!current.at("active").get<bool>()||old.at("market")!="cargo_market"||current.at("market")!="cargo_market")return false;
        const auto route=live::parseRoute(receipt.at("route"));if(!route.valid()||old.at("route")!=receipt.at("route")||current.at("route")!=receipt.at("route"))return false;
        if(!old.at("payload_known").get<bool>()||!current.at("payload_known").get<bool>())return false;
        if(old.at("cargo_units")!=current.at("cargo_units")||old.at("cargo_units").get<unsigned>()==0)return false;
        if(old.at("cargo_mass_kg")!=current.at("cargo_mass_kg")||old.at("cargo_unit_mass_kg")!=current.at("cargo_unit_mass_kg"))return false;
        if(old.at("loaded_known").get<bool>()&&old.at("cargo_loaded").get<bool>()&&(!current.at("loaded_known").get<bool>()||!current.at("cargo_loaded").get<bool>()))return false;
        return true;
    }catch(...){return false;}
}
}
