#pragma once
#include "../dispatch_shared/protocol.h"

namespace ats {
// ATS encodes the UTF-8 profile name as hexadecimal for its profile folder.
// Encoding every byte also prevents names from becoming filesystem paths.
inline std::string profileFolderId(const std::string& name) {
    if(name.empty()||name.size()>100)throw std::runtime_error("Invalid game profile name.");
    constexpr char hex[]="0123456789ABCDEF";std::string id;
    for(unsigned char c:name){if(c<32||c==127)throw std::runtime_error("Invalid game profile name.");id+=hex[c>>4];id+=hex[c&15];}
    return id;
}
inline void createProfileFile(const live::fs::path& path,const std::string& content) {
    HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE){if(GetLastError()==ERROR_FILE_EXISTS||GetLastError()==ERROR_ALREADY_EXISTS)return;throw std::runtime_error("Cannot create profile data file.");}
    DWORD written=0;const bool ok=WriteFile(file,content.data(),static_cast<DWORD>(content.size()),&written,nullptr)&&written==content.size()&&FlushFileBuffers(file);
    CloseHandle(file);if(!ok){DeleteFileW(path.c_str());throw std::runtime_error("Cannot finish profile data file.");}
}
inline live::fs::path ensureProfileFiles(const live::fs::path& root,const std::string& name) {
    const auto dir=root/dispatchGame::profiles/profileFolderId(name);live::fs::create_directories(dir);
    const live::Json settings={{"version",1},{"profile_name",name},{"mode",dispatchGame::isETS2()?2:0}};
    createProfileFile(dir/"settings.config",settings.dump(2)+"\n");
    createProfileFile(dir/"user.data","request_id,status\r\n");
    return dir;
}
}
