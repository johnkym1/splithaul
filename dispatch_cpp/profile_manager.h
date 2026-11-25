#pragma once
#include "profile_files.h"
#include "ats_profile_welcome.h"
#include <tlhelp32.h>
#include <shlobj.h>
#include <array>
#include <algorithm>

namespace profileManager {
namespace fs=std::filesystem;
struct Entry {std::string id,name;};
struct Protection {bool running=false;std::string name;};
inline fs::path store(const fs::path& root,bool ets){return root/(ets?"profiles_ets2":"profiles");}
inline bool ordinaryDirectory(const fs::path& path){const auto flags=GetFileAttributesW(path.c_str());return flags!=INVALID_FILE_ATTRIBUTES&&(flags&FILE_ATTRIBUTE_DIRECTORY)&&!(flags&FILE_ATTRIBUTE_REPARSE_POINT);}
inline Entry inspect(const fs::path& path){
    if(!ordinaryDirectory(path))throw std::runtime_error("Profile folder is unavailable or redirected.");
    const auto settings=path/"settings.config";const auto flags=GetFileAttributesW(settings.c_str());
    if(flags==INVALID_FILE_ATTRIBUTES||(flags&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))throw std::runtime_error("Profile settings are unavailable or redirected.");
    const auto data=live::readJson(settings);const auto name=data.at("profile_name").get<std::string>();
    const auto id=ats::profileFolderId(name);if(path.filename().u8string()!=id)throw std::runtime_error("Profile name and folder do not match.");
    return {id,name};
}
inline std::vector<Entry> list(const fs::path& root,bool ets){
    std::vector<Entry> result;const auto folder=store(root,ets);if(!ordinaryDirectory(folder))return result;
    for(const auto& path:fs::directory_iterator(folder))try{result.push_back(inspect(path.path()));}catch(const std::exception&){}
    std::sort(result.begin(),result.end(),[](const Entry& a,const Entry& b){return a.name<b.name;});return result;
}
inline bool protectedEntry(const Entry& entry,const Protection& live,const fs::path& path,const fs::path& active){
    return (!active.empty()&&fs::absolute(path).lexically_normal()==fs::absolute(active).lexically_normal())||(live.running&&(live.name.empty()||live.name==entry.name));
}
// Read each game's selected profile independently of the app's selected mode.
// A running game with unknown identity protects all its app profiles.
inline std::array<Protection,2> currentProtection(){
    std::array<Protection,2> result{};const auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot==INVALID_HANDLE_VALUE){for(auto& p:result)p.running=true;return result;}
    std::array<DWORD,2> pids{};std::array<unsigned,2> counts{};PROCESSENTRY32W process={};process.dwSize=sizeof(process);
    if(Process32FirstW(snapshot,&process))do{for(size_t i=0;i<2;++i)if(_wcsicmp(process.szExeFile,i?L"eurotrucks2.exe":L"amtrucks.exe")==0){result[i].running=true;pids[i]=process.th32ProcessID;++counts[i];}}while(Process32NextW(snapshot,&process));
    if(GetLastError()!=ERROR_NO_MORE_FILES)for(auto& p:result)p.running=true;
    CloseHandle(snapshot);
    PWSTR documents=nullptr;if(FAILED(SHGetKnownFolderPath(FOLDERID_Documents,0,nullptr,&documents)))return result;
    const fs::path base=documents;CoTaskMemFree(documents);
    for(size_t i=0;i<2;++i)if(result[i].running&&counts[i]==1)try{
        const auto handle=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pids[i]);if(!handle)continue;
        FILETIME birth={},exit={},kernel={},user={};const bool known=GetProcessTimes(handle,&birth,&exit,&kernel,&user)!=FALSE;CloseHandle(handle);if(!known)continue;
        const auto log=base/(i?L"Euro Truck Simulator 2":L"American Truck Simulator")/L"game.log.txt";
        WIN32_FILE_ATTRIBUTE_DATA info={};if(!GetFileAttributesExW(log.c_str(),GetFileExInfoStandard,&info)||CompareFileTime(&info.ftLastWriteTime,&birth)<0)continue;
        std::ifstream input(log,std::ios::binary|std::ios::ate);if(!input)continue;const auto size=input.tellg();if(size<=0||size>std::streamoff(32*1024*1024))continue;
        std::string text(static_cast<size_t>(size),'\0');input.seekg(0);if(input.read(text.data(),size))result[i].name=ats::selectedProfileFromLog(text);
    }catch(const std::exception&){}
    return result;
}
inline fs::path archive(const fs::path& root,bool ets,const Entry& entry,const Protection& live,const fs::path& active){
    if(entry.id!=ats::profileFolderId(entry.name))throw std::runtime_error("Invalid app profile selection.");
    const auto folder=store(root,ets);if(!ordinaryDirectory(folder))throw std::runtime_error("App profile storage is unavailable or redirected.");
    const auto source=folder/entry.id;const auto checked=inspect(source);
    if(checked.name!=entry.name)throw std::runtime_error("Profile changed. Select it again.");
    if(protectedEntry(entry,live,source,active))throw std::runtime_error("This profile is loaded or its game profile could not be verified. It cannot be deleted.");
    const auto backups=root/"deleted_profiles";if(fs::exists(backups)&&!ordinaryDirectory(backups))throw std::runtime_error("Deleted-profile backup folder is redirected.");
    fs::create_directories(backups);const auto game=backups/(ets?"ets2":"ats");if(fs::exists(game)&&!ordinaryDirectory(game))throw std::runtime_error("Deleted-profile game folder is redirected.");
    fs::create_directories(game);const auto destination=game/(entry.id+"-"+live::uniqueId());
    fs::rename(source,destination);return destination;
}
}
