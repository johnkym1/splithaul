#pragma once
#include "dispatch_core.h"
namespace dispatch {
struct AppOptions {int mode=0;bool dark=false;bool random=false;};
enum class RunningGames {none,ats,ets2,both};
inline int modeForRunningGame(int currentMode,RunningGames running) {
    if(currentMode<0||currentMode>3)throw std::runtime_error("Unknown game/job option.");
    if(running==RunningGames::ats&&currentMode>=2)return 0;
    if(running==RunningGames::ets2&&currentMode<2)return 2;
    return currentMode;
}
inline AppOptions decodeOptions(const Json& data) {
    if(!data.is_object())throw std::runtime_error("App options must be an object.");
    AppOptions result;result.mode=data.value("mode",0);result.dark=data.value("dark_theme",false);result.random=data.value("random",false);
    if(result.mode<0||result.mode>3)throw std::runtime_error("Unknown game/job option.");return result;
}
inline AppOptions loadOptions(const fs::path& root,int fallbackMode=0) {
    if(fs::exists(root/"app_options.json"))return decodeOptions(readJson(root/"app_options.json"));
    return {fallbackMode,false};
}
inline void saveOptions(const fs::path& root,const AppOptions& options) {
    const Json data={{"mode",options.mode},{"dark_theme",options.dark},{"random",options.random}};decodeOptions(data);writeJsonAtomic(root/"app_options.json",data);
}
inline constexpr std::array<const wchar_t*,4> modeLabels={L"ATS single",L"ATS multiple",L"ETS single",L"ETS multiple"};
}
