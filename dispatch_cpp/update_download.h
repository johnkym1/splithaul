#pragma once
#include "update_check.h"
#include "win_text.h"
#include <bcrypt.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <atomic>
#include <fstream>
#include <functional>
#include <memory>
#include <shellapi.h>
#pragma comment(lib,"bcrypt.lib")
#pragma comment(lib,"shell32.lib")
#pragma comment(lib,"ole32.lib")
#pragma comment(lib,"uuid.lib")
namespace updates {
inline bool trustedDownload(const std::wstring& url){
    if(url.find_first_of(L"\r\n\\")!=std::wstring::npos)return false;
    return url.rfind(L"https://github.com/johnkym1/splithaul/releases/download/",0)==0||url.rfind(L"https://release-assets.githubusercontent.com/",0)==0;
}
using Progress=std::function<void(uint64_t)>;
inline std::string fetchAsset(std::wstring url,uint64_t limit,const std::atomic<bool>& cancelled,const Progress& progress={}){
    Internet session(WinHttpOpen(L"SplitHaul/" DISPATCH_VERSION_WIDE,WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    WinHttpSetTimeouts(session.value,5000,5000,10000,10000);const auto started=GetTickCount64();
    for(unsigned hop=0;hop<6;++hop){
        if(cancelled)throw std::runtime_error("Download cancelled.");
        if(!trustedDownload(url))throw std::runtime_error("The download redirected outside the trusted release service.");
        URL_COMPONENTS parts={};parts.dwStructSize=sizeof(parts);parts.dwHostNameLength=parts.dwUrlPathLength=parts.dwExtraInfoLength=static_cast<DWORD>(-1);
        if(!WinHttpCrackUrl(url.c_str(),0,0,&parts)||parts.nScheme!=INTERNET_SCHEME_HTTPS||parts.nPort!=443)throw std::runtime_error("Invalid secure download address.");
        const std::wstring host(parts.lpszHostName,parts.dwHostNameLength);std::wstring path(parts.lpszUrlPath,parts.dwUrlPathLength);if(parts.dwExtraInfoLength)path.append(parts.lpszExtraInfo,parts.dwExtraInfoLength);
        Internet connection(WinHttpConnect(session.value,host.c_str(),parts.nPort,0));
        Internet request(WinHttpOpenRequest(connection.value,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE|WINHTTP_FLAG_REFRESH));
        DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_NEVER,disabled=WINHTTP_DISABLE_COOKIES,auth=WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
        if(!WinHttpSetOption(request.value,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects))||!WinHttpSetOption(request.value,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof(disabled))||!WinHttpSetOption(request.value,WINHTTP_OPTION_AUTOLOGON_POLICY,&auth,sizeof(auth)))throw std::runtime_error("Could not secure the download connection.");
        if(!WinHttpSendRequest(request.value,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request.value,nullptr))throw std::runtime_error("Download interrupted. Please retry.");
        DWORD status=0,size=sizeof(status);if(!WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX))throw std::runtime_error("Unreadable download response.");
        if(status==301||status==302||status==303||status==307||status==308){
            wchar_t location[16384];size=sizeof(location);if(!WinHttpQueryHeaders(request.value,WINHTTP_QUERY_LOCATION,WINHTTP_HEADER_NAME_BY_INDEX,location,&size,WINHTTP_NO_HEADER_INDEX))throw std::runtime_error("Invalid download redirect.");url=location;continue;
        }
        requireSuccess(status);std::string data;char buffer[65536];
        for(;;){if(cancelled)throw std::runtime_error("Download cancelled.");if(GetTickCount64()-started>300000)throw std::runtime_error("Download timed out. Please retry.");DWORD read=0;if(!WinHttpReadData(request.value,buffer,sizeof(buffer),&read))throw std::runtime_error("Download interrupted. Please retry.");if(!read)break;if(data.size()+read>limit)throw std::runtime_error("Downloaded file exceeds its expected size.");data.append(buffer,read);if(progress)progress(data.size());}
        return data;
    }throw std::runtime_error("Too many download redirects.");
}
inline std::string sha256(const std::string& data){
    BCRYPT_ALG_HANDLE alg=nullptr;if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("Could not verify update checksum.");
    unsigned char digest[32];const auto status=BCryptHash(alg,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())),static_cast<ULONG>(data.size()),digest,sizeof(digest));BCryptCloseAlgorithmProvider(alg,0);if(status<0)throw std::runtime_error("Could not verify update checksum.");
    constexpr char hex[]="0123456789abcdef";std::string result;for(auto b:digest){result+=hex[b>>4];result+=hex[b&15];}return result;
}
inline std::string installerName(const Result& release){return "SplitHaul-Setup-"+release.latest+"-win64.exe";}
inline void verifyAsset(const Result& release,const std::string& checksum,const std::string& bytes){
    auto line=checksum;while(!line.empty()&&(line.back()=='\r'||line.back()=='\n'))line.pop_back();
    if(!validDigest(release.digest)||line!=release.digest+"  "+installerName(release))throw std::runtime_error("Release checksum does not match the installer. Nothing was installed.");
    if(bytes.size()!=release.installerBytes||bytes.size()<2||bytes[0]!='M'||bytes[1]!='Z'||sha256(bytes)!=release.digest)throw std::runtime_error("Installer verification failed. Nothing was installed; retry the download.");
}
struct Download {
    dispatch::fs::path directory,file;std::string digest;uint64_t bytes=0;
    Download()=default;Download(const Download&)=delete;Download& operator=(const Download&)=delete;
    ~Download(){std::error_code ignored;if(!file.empty())dispatch::fs::remove(file,ignored);if(!directory.empty())dispatch::fs::remove(directory,ignored);}
};
inline std::shared_ptr<Download> download(const Result& release,const std::atomic<bool>& cancelled,const Progress& progress={},dispatch::fs::path cache={}){
    const auto canonical="https://github.com/johnkym1/splithaul/releases/download/v"+release.latest+"/"+installerName(release);
    if(!version(release.latest)||release.installerUrl!=canonical||release.checksumUrl!=canonical+".sha256"||!release.installerBytes||release.installerBytes>maximumInstallerBytes||!validDigest(release.digest))throw std::runtime_error("Invalid update metadata.");
    const auto checksum=fetchAsset(winText::toWide(release.checksumUrl),4096,cancelled);
    const auto data=fetchAsset(winText::toWide(release.installerUrl),release.installerBytes,cancelled,progress);verifyAsset(release,checksum,data);
    if(cancelled)throw std::runtime_error("Download cancelled.");
    if(cache.empty()){PWSTR local=nullptr;if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&local)))throw std::runtime_error("Cannot locate the update cache.");cache=dispatch::fs::path(local)/"Grumpyman"/"SplitHaul"/"updates";CoTaskMemFree(local);}
    GUID guid;if(FAILED(CoCreateGuid(&guid)))throw std::runtime_error("Cannot create a download folder.");wchar_t name[40];StringFromGUID2(guid,name,40);
    dispatch::fs::create_directories(cache);auto ready=std::make_shared<Download>();ready->directory=cache/name;if(!dispatch::fs::create_directory(ready->directory))throw std::runtime_error("Download folder already exists.");
    ready->file=ready->directory/installerName(release);ready->digest=release.digest;ready->bytes=release.installerBytes;
    std::ofstream output(ready->file,std::ios::binary);output.write(data.data(),static_cast<std::streamsize>(data.size()));output.close();if(!output)throw std::runtime_error("Could not save the downloaded installer.");return ready;
}
inline bool gamesRunning(){
    const auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snapshot==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot check whether games are closed.");
    PROCESSENTRY32W entry={};entry.dwSize=sizeof(entry);if(!Process32FirstW(snapshot,&entry)){CloseHandle(snapshot);throw std::runtime_error("Cannot read the running game list.");}
    bool found=false;do{if(!_wcsicmp(entry.szExeFile,L"amtrucks.exe")||!_wcsicmp(entry.szExeFile,L"eurotrucks2.exe")){found=true;break;}}while(Process32NextW(snapshot,&entry));const auto error=GetLastError();CloseHandle(snapshot);if(!found&&error!=ERROR_NO_MORE_FILES)throw std::runtime_error("Cannot finish checking the running game list.");return found;
}
inline std::wstring setupArguments(const dispatch::fs::path& directory){
    std::wstring args=L"/NORESTART /NOCLOSEAPPLICATIONS";
    if(dispatch::fs::exists(directory/"installed-layout.json"))args+=L" /DIR=\""+directory.wstring()+L"\"";
    return args;
}
using SetupLauncher=BOOL(WINAPI*)(SHELLEXECUTEINFOW*);
inline bool launchInstaller(HWND owner,Download& ready,SetupLauncher startSetup=ShellExecuteExW,bool(*running)()=gamesRunning){
    if(running())throw std::runtime_error("Close ATS and ETS2 normally, then press Install update. Your download is ready.");
    // Deny writes/deletion while rechecking and launching the downloaded file.
    HANDLE file=CreateFileW(ready.file.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Could not open the verified installer.");
    try{
        LARGE_INTEGER size;if(!GetFileSizeEx(file,&size)||size.QuadPart!=static_cast<LONGLONG>(ready.bytes)||ready.bytes>maximumInstallerBytes)throw std::runtime_error("Installer changed after download. Download it again.");
        std::string data(static_cast<size_t>(ready.bytes),'\0');DWORD read=0;if(!ReadFile(file,data.data(),static_cast<DWORD>(data.size()),&read,nullptr)||read!=data.size()||sha256(data)!=ready.digest)throw std::runtime_error("Installer checksum changed. Nothing was installed.");
        wchar_t exe[32768];const auto length=GetModuleFileNameW(nullptr,exe,32768);if(!length||length>=32768)throw std::runtime_error("Cannot locate this installation.");const auto args=setupArguments(dispatch::fs::path(exe).parent_path());
        SHELLEXECUTEINFOW launch={};launch.cbSize=sizeof(launch);launch.fMask=SEE_MASK_NOCLOSEPROCESS;launch.hwnd=owner;launch.lpVerb=L"open";launch.lpFile=ready.file.c_str();launch.lpParameters=args.c_str();launch.nShow=SW_SHOWNORMAL;
        if(!startSetup(&launch)){const auto error=GetLastError();CloseHandle(file);file=INVALID_HANDLE_VALUE;if(error==ERROR_CANCELLED)return false;throw std::runtime_error("Windows could not start Setup. Please retry Install update.");}
        if(launch.hProcess)CloseHandle(launch.hProcess);CloseHandle(file);file=INVALID_HANDLE_VALUE;
        // Setup needs the file after this app exits. Keep it in the update cache.
        ready.file.clear();ready.directory.clear();return true;
    }catch(...){if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);throw;}
}
}
