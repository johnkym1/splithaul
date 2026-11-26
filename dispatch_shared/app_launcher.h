#pragma once
#include "protocol.h"
#include <filesystem>
#include <string>
#include <cwctype>
#include <cstdint>

namespace dispatchLauncher {
inline constexpr wchar_t windowProperty[]=L"GTDispatchProject";
inline constexpr UINT showMessage=WM_APP+73;
inline constexpr UINT shortcutModifiers=MOD_SHIFT|MOD_NOREPEAT;
inline constexpr UINT shortcutKey=VK_F9;
inline constexpr char shortcutLabel[]="Shift+F9";
inline constexpr wchar_t shortcutAtom[]=L"GT_Telemetry.Dispatch.ShiftF9";
// Require the exact modifier combination; Ctrl+F9 must remain game teleport.
inline bool shortcutChord(bool keyDown,unsigned modifiers) {
    return keyDown&&modifiers==(shortcutModifiers&~MOD_NOREPEAT);
}
inline uintptr_t projectKey(const std::filesystem::path& root) {
    uint64_t hash=14695981039346656037ULL;
    for(auto c:std::filesystem::absolute(root).lexically_normal().wstring()){hash^=static_cast<uint64_t>(std::towlower(c));hash*=1099511628211ULL;}
    return static_cast<uintptr_t>(hash?hash:1);
}
struct Paths {std::filesystem::path exe,project;};
inline live::Json registrationJson(const Paths& paths) {
    return {{"version",1},{"executable",paths.exe.u8string()},{"project",paths.project.u8string()}};
}
inline Paths parseRegistration(const live::Json& j) {
    if(j.at("version")!=1)throw std::runtime_error("Unsupported app launcher registration.");
    const auto exe=j.at("executable").get<std::string>(),project=j.at("project").get<std::string>();
    if(exe.size()>32760||project.size()>32760||exe.find('\0')!=std::string::npos||project.find('\0')!=std::string::npos)throw std::runtime_error("Invalid app launcher path length or embedded null.");
    Paths paths={std::filesystem::u8path(exe),std::filesystem::u8path(project)};
    // Existing registrations remain valid while the app and DLLs are updated.
    const bool knownName=_wcsicmp(paths.exe.filename().c_str(),L"SplitHaul.exe")==0||_wcsicmp(paths.exe.filename().c_str(),L"DispatchPlanner.exe")==0;
    if(!paths.exe.is_absolute()||!paths.project.is_absolute()||!knownName)throw std::runtime_error("Invalid app launcher paths.");
    return paths;
}
inline bool registerApp(const std::filesystem::path& project) {
    try {
        wchar_t exe[32768];const auto n=GetModuleFileNameW(nullptr,exe,32768);if(!n||n>=32768)return false;
        const auto folder=live::ipcFolder();std::filesystem::create_directories(folder);
        const Paths paths={std::filesystem::path(exe),std::filesystem::absolute(project).lexically_normal()};
        live::writeJson(folder/"launcher.json",registrationJson(parseRegistration(registrationJson(paths))));return true;
    }catch(...){return false;}
}
inline bool registered(Paths& paths,const std::filesystem::path& folder=live::ipcFolder()) {
    paths={};
    try{paths=parseRegistration(live::readJson(folder/"launcher.json",128*1024));return true;}
    catch(...){return false;}
}
}
