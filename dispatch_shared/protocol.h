#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "game_target.h"
#include <bcrypt.h>
#include <json.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <set>

namespace live {
using Json=nlohmann::json;
namespace fs=std::filesystem;
inline constexpr int protocolVersion=5;
inline constexpr unsigned maximumStops=8;
inline uint64_t utcMs() {
    FILETIME t;GetSystemTimeAsFileTime(&t);ULARGE_INTEGER n;n.LowPart=t.dwLowDateTime;n.HighPart=t.dwHighDateTime;
    return (n.QuadPart-116444736000000000ULL)/10000;
}
inline std::string uniqueId() {
    unsigned char bytes[16];if(BCryptGenRandom(nullptr,bytes,sizeof(bytes),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)throw std::runtime_error("Cannot generate a dispatch identifier.");
    constexpr char hex[]="0123456789abcdef";std::string s;for(auto b:bytes){s+=hex[b>>4];s+=hex[b&15];}return s;
}
inline bool identifier(const std::string& s,size_t max=48) {
    return !s.empty()&&s.size()<=max&&std::all_of(s.begin(),s.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_';});
}
inline bool requestId(const std::string& s) {
    return s.size()==32&&std::all_of(s.begin(),s.end(),[](unsigned char c){return (c>='a'&&c<='f')||(c>='0'&&c<='9');});
}
struct Route {
    std::string cargo,sourceCity,sourceCompany,targetCity,targetCompany;
    bool valid() const {return identifier(cargo)&&identifier(sourceCity)&&identifier(sourceCompany)&&identifier(targetCity)&&identifier(targetCompany)&&(sourceCity!=targetCity||sourceCompany!=targetCompany);}
    std::string source() const {return sourceCompany+"."+sourceCity;}
    std::string target() const {return targetCompany+"."+targetCity;}
};
inline Json routeJson(const Route& r) {return {{"cargo_id",r.cargo},{"source_city_id",r.sourceCity},{"source_company_id",r.sourceCompany},{"destination_city_id",r.targetCity},{"destination_company_id",r.targetCompany}};}
inline Route parseRoute(const Json& j) {return {j.at("cargo_id").get<std::string>(),j.at("source_city_id").get<std::string>(),j.at("source_company_id").get<std::string>(),j.at("destination_city_id").get<std::string>(),j.at("destination_company_id").get<std::string>()};}
struct Request {
    Route route;
    std::string id,session,context,trailer;
    uint64_t created=0,expires=0;
    bool twoStop=false;
    Route finalRoute;
    std::string secondId;
    unsigned remainingPercent=50;
    // Multi-drop keeps the original pickup/cargo and all trailers attached.
    std::vector<Route> dropRoutes;
    std::vector<std::string> dropIds;
    bool multiDrop() const {return !dropRoutes.empty();}
};
inline Json requestJson(const Request& r) {
    Json j={{"protocol",protocolVersion},{"game",dispatchGame::id},{"request_id",r.id},{"session_id",r.session},{"context_id",r.context},{"trailer_signature",r.trailer},{"created_utc_ms",r.created},{"expires_utc_ms",r.expires},{"route",routeJson(r.route)}};
    j["load_policy"]="maximum_whole_units";
    if(r.twoStop)j["continuation"]={{"request_id",r.secondId},{"route",routeJson(r.finalRoute)},{"remaining_percent",r.remainingPercent}};
    if(r.multiDrop()) {
        j.erase("continuation");j["drops"]=Json::array();j["split_policy"]="equal_whole_units";
        for(size_t i=0;i<r.dropRoutes.size();++i)j["drops"].push_back({{"request_id",r.dropIds.at(i)},{"route",routeJson(r.dropRoutes[i])}});
    }
    return j;
}
inline Request parseRequest(const Json& j) {
    if(j.at("protocol")!=protocolVersion||j.at("game")!=dispatchGame::id)throw std::runtime_error("Unsupported dispatch protocol or game.");
    if(j.at("load_policy")!="maximum_whole_units")throw std::runtime_error("Unsupported cargo load policy.");
    Request r;r.route=parseRoute(j.at("route"));r.id=j.at("request_id").get<std::string>();r.session=j.at("session_id").get<std::string>();r.context=j.at("context_id").get<std::string>();r.trailer=j.at("trailer_signature").get<std::string>();r.created=j.at("created_utc_ms").get<uint64_t>();r.expires=j.at("expires_utc_ms").get<uint64_t>();
    if(!r.route.valid()||!requestId(r.id)||!requestId(r.session)||r.context.empty()||r.context.size()>160||r.trailer.empty()||r.trailer.size()>512||r.expires<=r.created||r.expires-r.created>120000)throw std::runtime_error("Invalid dispatch request fields.");
    if(j.contains("continuation")) {
        const auto& next=j.at("continuation");r.twoStop=true;r.secondId=next.at("request_id").get<std::string>();r.finalRoute=parseRoute(next.at("route"));
        if(!next.at("remaining_percent").is_number_integer())throw std::runtime_error("Remaining percentage must be a whole number.");
        const auto percent=next.at("remaining_percent").get<int64_t>();if(percent<1||percent>99)throw std::runtime_error("Remaining load must be between 1 and 99 percent.");r.remainingPercent=static_cast<unsigned>(percent);
        if(!requestId(r.secondId)||r.secondId==r.id||!r.finalRoute.valid()||r.finalRoute.source()!=r.route.source()||r.finalRoute.cargo!=r.route.cargo||r.finalRoute.target()==r.route.target())throw std::runtime_error("A two-stop plan needs the same pickup/cargo, distinct request IDs and two different deliveries.");
    }
    if(j.contains("drops")) {
        const auto& drops=j.at("drops");
        if(j.contains("continuation")||j.value("split_policy",std::string())!="equal_whole_units"||!drops.is_array()||drops.size()<3||drops.size()>maximumStops)throw std::runtime_error("Multi-drop requires 3 to 8 deliveries and an equal whole-unit split.");
        std::set<std::string> ids,cities;
        for(const auto& drop:drops) {
            const auto id=drop.at("request_id").get<std::string>();const auto route=parseRoute(drop.at("route"));
            const bool finalReturn=route.targetCity==r.route.sourceCity&&r.dropRoutes.size()+1==drops.size();
            if(!requestId(id)||!ids.insert(id).second||!route.valid()||route.source()!=r.route.source()||route.cargo!=r.route.cargo||(route.targetCity==r.route.sourceCity&&!finalReturn)||!cities.insert(route.targetCity).second)throw std::runtime_error("Multi-drop needs unique delivery cities; only the final delivery may return to the pickup city, at a different depot.");
            r.dropIds.push_back(id);r.dropRoutes.push_back(route);
        }
        if(r.dropIds.front()!=r.id||routeJson(r.dropRoutes.front())!=routeJson(r.route))throw std::runtime_error("The first delivery must match the dispatch request.");
        r.twoStop=true;r.secondId=r.dropIds.back();r.finalRoute=r.dropRoutes.back();
    }
    return r;
}
inline bool currentRequest(const Request& r,const std::string& session,const std::string& context,const std::string& trailer,uint64_t now) {
    return r.session==session&&r.context==context&&r.trailer==trailer&&r.created<=now+5000&&r.expires>now;
}
inline bool terminal(const std::string& state) {return state=="delivered"||state=="cancelled"||state=="ended_unconfirmed"||state=="rejected"||state=="failed"||state=="uncertain";}
inline fs::path ipcFolder() {
    wchar_t value[32768];const auto n=GetEnvironmentVariableW(L"LOCALAPPDATA",value,32768);if(!n||n>=32768)throw std::runtime_error("Local AppData is unavailable.");
    return fs::path(value)/dispatchGame::ipc;
}
// MSIX can redirect apparent AppData paths. Check the opened handle's physical
// path before sending anything, so a request cannot disappear into LocalCache.
inline void physicalPath(HANDLE file,const fs::path& expected) {
    wchar_t name[32768];const auto n=GetFinalPathNameByHandleW(file,name,32768,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
    if(!n||n>=32768)throw std::runtime_error("Cannot verify the dispatch file location.");
    std::wstring actual=name;if(actual.rfind(L"\\\\?\\",0)==0)actual.erase(0,4);
    if(_wcsicmp(fs::absolute(expected).lexically_normal().c_str(),fs::path(actual).lexically_normal().c_str())!=0)throw std::runtime_error("Windows redirected the dispatch file. Launch this app directly from File Explorer.");
}
inline Json readJson(const fs::path& path,size_t limit=1024*1024) {
    HANDLE f=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(f==INVALID_HANDLE_VALUE)throw std::runtime_error("Dispatch data is unavailable.");
    try {
        physicalPath(f,path);LARGE_INTEGER size;if(!GetFileSizeEx(f,&size)||size.QuadPart<=0||size.QuadPart>static_cast<LONGLONG>(limit))throw std::runtime_error("Invalid dispatch data size.");
        std::string data(static_cast<size_t>(size.QuadPart),'\0');DWORD count=0;
        if(!ReadFile(f,data.data(),static_cast<DWORD>(data.size()),&count,nullptr)||count!=data.size())throw std::runtime_error("Cannot read dispatch data.");
        CloseHandle(f);f=INVALID_HANDLE_VALUE;return Json::parse(data);
    }catch(...){if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);throw;}
}
inline void writeJson(const fs::path& path,const Json& j,bool replace=true) {
    // The random temporary name permits concurrent readers/writers without
    // overwriting another client's temporary request.
    const auto id=uniqueId();const auto temp=fs::path(path.wstring()+L"."+std::wstring(id.begin(),id.end())+L".tmp");
    HANDLE f=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(f==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create dispatch data.");
    try {
        physicalPath(f,temp);const auto data=j.dump(2);DWORD written=0;
        if(!WriteFile(f,data.data(),static_cast<DWORD>(data.size()),&written,nullptr)||written!=data.size()||!FlushFileBuffers(f))throw std::runtime_error("Cannot finish writing dispatch data.");
        CloseHandle(f);f=INVALID_HANDLE_VALUE;
        if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH|(replace?MOVEFILE_REPLACE_EXISTING:0)))throw std::runtime_error("A request is already waiting, or the dispatch file is unavailable.");
    }catch(...){if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);DeleteFileW(temp.c_str());throw;}
}
inline bool freshStatus(const Json& status,uint64_t now) {
    try {const auto stamp=status.at("updated_utc_ms").get<uint64_t>();return status.at("protocol")==protocolVersion&&status.at("game")==dispatchGame::id&&requestId(status.at("session_id").get<std::string>())&&stamp<=now+5000&&now<=stamp+6000&&status.at("online").get<bool>();}catch(...){return false;}
}
inline bool claimOnce(const fs::path& incoming,const fs::path& folder,const std::string& id) {
    if(!requestId(id))throw std::runtime_error("Invalid request ledger identifier.");
    const auto destination=folder/"claims"/(id+".json");
    if(MoveFileExW(incoming.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH))return true;
    if(fs::exists(destination)) {
        const auto duplicate=folder/"duplicates"/(id+"-"+uniqueId()+".json");
        if(!MoveFileExW(incoming.c_str(),duplicate.c_str(),MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot archive a duplicate request.");
        return false;
    }
    throw std::runtime_error("Cannot claim a dispatch request.");
}
}
