#pragma once
#include <string>
// Separate build targets deliberately isolate game requests and saved state.
namespace dispatchGame {
#if defined(GT_MULTI_GAME_APP)
// Only the app's UI thread changes the selected target. DLL targets stay fixed.
inline const char* id="ats";
inline const char* sdkId="ats";
inline const wchar_t* executable=L"amtrucks.exe";
inline const wchar_t* documents=L"American Truck Simulator";
inline const wchar_t* ipc=L"MSDCLiveDispatch";
inline const char* profiles="profiles";
inline const char* savedDispatch="ats_dispatch.json";
inline const char* build="ats-splithaul-20261128-roadtrains";
inline void select(bool ets) {
    id=ets?"ets2":"ats";sdkId=ets?"eut2":"ats";
    executable=ets?L"eurotrucks2.exe":L"amtrucks.exe";
    documents=ets?L"Euro Truck Simulator 2":L"American Truck Simulator";
    ipc=ets?L"MSDCLiveDispatchETS2":L"MSDCLiveDispatch";
    profiles=ets?"profiles_ets2":"profiles";savedDispatch=ets?"ets2_dispatch.json":"ats_dispatch.json";
    build=ets?"ets2-splithaul-20261128-roadtrains":"ats-splithaul-20261128-roadtrains";
}
#elif defined(GT_TARGET_ETS2)
inline constexpr char id[]="ets2";
inline constexpr char sdkId[]="eut2";
inline constexpr wchar_t executable[]=L"eurotrucks2.exe";
inline constexpr wchar_t documents[]=L"Euro Truck Simulator 2";
inline constexpr wchar_t ipc[]=L"MSDCLiveDispatchETS2";
inline constexpr char profiles[]="profiles_ets2";
inline constexpr char savedDispatch[]="ets2_dispatch.json";
inline constexpr char build[]="ets2-splithaul-20261128-roadtrains";
#else
inline constexpr char id[]="ats";
inline constexpr char sdkId[]="ats";
inline constexpr wchar_t executable[]=L"amtrucks.exe";
inline constexpr wchar_t documents[]=L"American Truck Simulator";
inline constexpr wchar_t ipc[]=L"MSDCLiveDispatch";
inline constexpr char profiles[]="profiles";
inline constexpr char savedDispatch[]="ats_dispatch.json";
inline constexpr char build[]="ats-splithaul-20261128-roadtrains";
#endif
inline bool isETS2(){return std::string(id)=="ets2";}
inline std::string displayText(std::string text) {
    if(isETS2()){size_t at=0;while((at=text.find("ATS",at))!=std::string::npos){text.replace(at,3,"ETS2");at+=4;}}
    return text;
}
}
