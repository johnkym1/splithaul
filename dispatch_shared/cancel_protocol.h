#pragma once
#include "protocol.h"

namespace live {
struct CancelRequest {
    std::string id,session,context,profileId,target;
    uint64_t created=0,expires=0;
};
inline Json cancelRequestJson(const CancelRequest& r) {
    return {{"protocol",protocolVersion},{"game",dispatchGame::id},{"request_id",r.id},{"session_id",r.session},{"context_id",r.context},{"profile_id",r.profileId},{"target_token",r.target},{"created_utc_ms",r.created},{"expires_utc_ms",r.expires}};
}
inline CancelRequest parseCancelRequest(const Json& j) {
    if(j.at("protocol")!=protocolVersion||j.at("game")!=dispatchGame::id)throw std::runtime_error("Unsupported cancellation request.");
    CancelRequest r;r.id=j.at("request_id").get<std::string>();r.session=j.at("session_id").get<std::string>();r.context=j.at("context_id").get<std::string>();r.profileId=j.at("profile_id").get<std::string>();r.target=j.at("target_token").get<std::string>();r.created=j.at("created_utc_ms").get<uint64_t>();r.expires=j.at("expires_utc_ms").get<uint64_t>();
    const bool validProfile=!r.profileId.empty()&&r.profileId.size()<=200&&r.profileId.size()%2==0&&std::all_of(r.profileId.begin(),r.profileId.end(),[](unsigned char c){return (c>='0'&&c<='9')||(c>='A'&&c<='F');});
    if(!requestId(r.id)||!requestId(r.session)||r.context.empty()||r.context.size()>160||r.target.empty()||r.target.size()>200||!validProfile||r.expires<=r.created||r.expires-r.created>30000)throw std::runtime_error("Invalid cancellation request.");
    return r;
}
inline bool currentCancelRequest(const CancelRequest& r,const std::string& currentSession,uint64_t now) {
    return r.session==currentSession&&r.created<=now+5000&&now<r.expires;
}
inline Json applyCancelCleanup(const Json& saved,const Json& pending,const Json& reply,uint64_t now) {
    const auto request=pending.at("request_id").get<std::string>(),id=pending.at("dispatch_id").get<std::string>();
    const auto gameSession=pending.at("session_id").get<std::string>(),dispatchSession=pending.at("dispatch_session_id").get<std::string>();
    if(!requestId(request)||!requestId(id)||!requestId(gameSession)||!requestId(dispatchSession)||reply.at("protocol")!=protocolVersion||reply.at("game")!=dispatchGame::id||reply.at("request_id")!=request||reply.at("session_id")!=gameSession||saved.at("last_request_id")!=id||saved.at("session_id")!=dispatchSession)throw std::runtime_error("Cancellation response or saved dispatch identity changed.");
    const auto outcome=reply.at("outcome").get<std::string>();
    if(outcome!="game_confirmed"&&outcome!="game_requested"&&outcome!="no_game_job"&&outcome!="game_unavailable"&&outcome!="game_unconfirmed"&&outcome!="stop_failed"&&outcome!="contact_lost")throw std::runtime_error("Cancellation was not accepted; dispatch was not cleared.");
    auto result=saved;result["form_cleared_for"]=id;result["cancel_cleanup"]={{"request_id",request},{"outcome",outcome},{"updated_utc_ms",now}};return result;
}
// Claim queued work rather than deleting it: another worker can no longer
// consume it, and the durable claim prevents a later replay of the same ID.
inline void stopQueuedWork(const fs::path& folder) {
    const auto queued=folder/"request.json";
    if(fs::exists(queued)) {
        const auto request=parseRequest(readJson(queued,8192));
        if(!claimOnce(queued,folder,request.id))throw std::runtime_error("Could not stop queued dispatch.");
    }
    const auto resume=folder/"resume.json";
    if(fs::exists(resume)) {
        const auto archived=folder/"duplicates"/(uniqueId()+".stopped-resume.json");
        if(!MoveFileExW(resume.c_str(),archived.c_str(),MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Could not stop queued recovery.");
    }
}
}
