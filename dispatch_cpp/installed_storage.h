#pragma once
#include "dispatch_core.h"

namespace installedStorage {
// Reuse established data in place so an upgrade cannot hide profiles/history
// or create a competing copy that later diverges.
inline dispatch::fs::path userDataRoot(const dispatch::fs::path& local) {
    const auto legacy=local/"Grumpyman"/"DispatchCompanion";
    return dispatch::fs::exists(legacy)?legacy:local/"Grumpyman"/"SplitHaul";
}
// Installed program files are read-only. Only known catalog assets are refreshed;
// profile settings, histories, receipts and dispatches are never copied/reset.
inline dispatch::fs::path prepare(const dispatch::fs::path& installation,const dispatch::fs::path& userData) {
    using namespace dispatch;
    if(readJson(installation/"installed-layout.json").at("format")!=1)
        throw std::runtime_error("Unsupported SplitHaul installation layout.");
    std::array<Json,2> catalogs;
    const std::array<const char*,2> games={"ats","ets2"};
    for(size_t i=0;i<games.size();++i){
        catalogs[i]=readJson(installation/"catalogs"/(std::string(games[i])+".json"));
        if(catalogs[i].at("format")!=1||catalogs[i].at("game")!=games[i])throw std::runtime_error("Installed game catalog does not match its game.");
    }
    fs::create_directories(userData/"catalogs");
    for(size_t i=0;i<games.size();++i){
        const auto destination=userData/"catalogs"/(std::string(games[i])+".json");
        bool matches=false;try{matches=readJson(destination)==catalogs[i];}catch(const std::exception&){}
        if(!matches)writeJsonAtomic(destination,catalogs[i]);
    }
    for(const auto filename:{"company_catalog.json","city_coordinates.json"})
        if(!fs::exists(userData/filename))writeJsonAtomic(userData/filename,Json::object());
    return userData;
}
}
