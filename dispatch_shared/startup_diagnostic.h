#pragma once
#include "protocol.h"

namespace live {
// A rejected plugin has no callbacks/heartbeat. Its diagnostic remains valid
// only for the same still-running ATS process, after that process was created.
inline std::string startupDiagnostic(const Json& value,uint32_t pid,uint64_t created,uint64_t now) {
    try {
        if(!pid||!created||value.at("protocol")!=protocolVersion||value.at("game")!=dispatchGame::id||value.at("online")!=false||value.at("state")!="startup_rejected"||value.at("pid").get<uint32_t>()!=pid)return {};
        const auto stamp=value.at("updated_utc_ms").get<uint64_t>();
        if(stamp<created||stamp>now+5000)return {};
        const auto message=value.at("message").get<std::string>();
        return message.size()<=1024?message:std::string();
    }catch(...){return {};}
}
}
