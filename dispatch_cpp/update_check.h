#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "dispatch_core.h"
#include "app_version.h"
#include <winhttp.h>
#include <optional>
#include <array>
#include <cctype>
#include <algorithm>
#pragma comment(lib,"winhttp.lib")

namespace updates {
inline constexpr char releasesUrl[]="https://github.com/johnkym1/splithaul/releases";
inline constexpr char releasePrefix[]="https://github.com/johnkym1/splithaul/releases/tag/";
inline constexpr bool includePrereleases=true; // The app is currently a test release.
using Version=std::array<unsigned,3>;
inline std::optional<Version> version(std::string text){
    if(!text.empty()&&text[0]=='v')text.erase(0,1);Version result{};size_t pos=0;
    for(size_t part=0;part<3;++part){const size_t start=pos;unsigned number=0;
        while(pos<text.size()&&text[pos]>='0'&&text[pos]<='9'){if(number>99999)return {};number=number*10+static_cast<unsigned>(text[pos++]-'0');}
        if(pos==start||(pos-start>1&&text[start]=='0'))return {};result[part]=number;
        if(part<2){if(pos>=text.size()||text[pos++]!='.')return {};}else if(pos!=text.size())return {};
    }return result;
}
enum class State {available,current,ahead,none,error};
inline constexpr uint64_t maximumInstallerBytes=128*1024*1024;
inline bool validDigest(const std::string& value){return value.size()==64&&std::all_of(value.begin(),value.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});}
struct Result {State state=State::none;std::string latest,url,message;bool prerelease=false;std::string installerUrl,checksumUrl,digest;uint64_t installerBytes=0;};
inline Result evaluate(const dispatch::Json& releases,const std::string& installed,bool includeTests=includePrereleases){
    const auto current=version(installed);if(!current)throw std::runtime_error("The installed version could not be read.");
    if(!releases.is_array())throw std::runtime_error("GitHub returned an unreadable release list.");
    std::optional<Version> highest;Result result;
    for(const auto& release:releases)try{
        if(release.value("draft",true)||!release.contains("published_at")||!release["published_at"].is_string())continue;
        const bool test=release.value("prerelease",false);if(test&&!includeTests)continue;
        const auto tag=release.at("tag_name").get<std::string>();const auto parsed=version(tag);if(!parsed||tag!="v"+std::to_string((*parsed)[0])+"."+std::to_string((*parsed)[1])+"."+std::to_string((*parsed)[2]))continue;
        const auto number=tag.substr(1),url=release.at("html_url").get<std::string>();if(url!=std::string(releasePrefix)+tag)continue;
        const auto installer="SplitHaul-Setup-"+number+"-win64.exe";bool exe=false,checksum=false;Result candidate;
        const auto download="https://github.com/johnkym1/splithaul/releases/download/"+tag+"/"+installer;
        if(!release.at("assets").is_array())continue;
        for(const auto& asset:release["assets"])if(asset.value("state",std::string())=="uploaded"&&asset.value("size",uint64_t(0))>0){
            const auto name=asset.value("name",std::string()),assetUrl=asset.value("browser_download_url",std::string());
            if(name==installer&&assetUrl==download){const auto digest=asset.value("digest",std::string());const auto bytes=asset.at("size").get<uint64_t>();if(digest.rfind("sha256:",0)==0&&validDigest(digest.substr(7))&&bytes<=maximumInstallerBytes){exe=true;candidate.installerUrl=assetUrl;candidate.digest=digest.substr(7);candidate.installerBytes=bytes;}}
            if(name==installer+".sha256"&&assetUrl==download+".sha256"&&asset.at("size").get<uint64_t>()<=4096){checksum=true;candidate.checksumUrl=assetUrl;}
        }
        if(!exe||!checksum||(highest&&*parsed<=*highest))continue;
        highest=parsed;result=std::move(candidate);result.latest=number;result.url=url;result.prerelease=test;
    }catch(const dispatch::Json::exception&){continue;}
    if(highest)result.state=*highest>*current?State::available:*highest==*current?State::current:State::ahead;
    return result;
}
struct Internet {
    HINTERNET value=nullptr;
    explicit Internet(HINTERNET handle):value(handle){if(!value)throw std::runtime_error("Could not connect to GitHub. Check your internet connection and try again.");}
    ~Internet(){if(value)WinHttpCloseHandle(value);}
    Internet(const Internet&)=delete;Internet& operator=(const Internet&)=delete;
};
inline void requireSuccess(DWORD status){
    if(status==403||status==429)throw std::runtime_error("GitHub has temporarily limited update checks. Please try again later.");
    if(status!=200)throw std::runtime_error("GitHub update check failed (HTTP "+std::to_string(status)+"). Please try again later.");
}
inline Result check(){
    Internet session(WinHttpOpen(L"SplitHaul/" DISPATCH_VERSION_WIDE,WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    WinHttpSetTimeouts(session.value,5000,5000,5000,10000);
    Internet connection(WinHttpConnect(session.value,L"api.github.com",INTERNET_DEFAULT_HTTPS_PORT,0));
    Internet request(WinHttpOpenRequest(connection.value,L"GET",L"/repos/johnkym1/splithaul/releases?per_page=100",nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE|WINHTTP_FLAG_REFRESH));
    DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;WinHttpSetOption(request.value,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy));
    DWORD disabled=WINHTTP_DISABLE_COOKIES;WinHttpSetOption(request.value,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof(disabled));
    const wchar_t* headers=L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2026-03-10\r\n";
    if(!WinHttpSendRequest(request.value,headers,static_cast<DWORD>(-1),WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request.value,nullptr))throw std::runtime_error("GitHub could not be reached. Check your connection and try again.");
    DWORD status=0,size=sizeof(status);if(!WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX))throw std::runtime_error("GitHub returned an unreadable response.");
    requireSuccess(status);
    std::string body;char buffer[8192];const auto started=GetTickCount64();
    for(;;){DWORD read=0;if(GetTickCount64()-started>20000)throw std::runtime_error("The GitHub update check timed out.");if(!WinHttpReadData(request.value,buffer,sizeof(buffer),&read))throw std::runtime_error("The GitHub response was interrupted. Please try again.");if(!read)break;if(body.size()+read>2*1024*1024)throw std::runtime_error("GitHub returned an unexpectedly large release list.");body.append(buffer,read);}
    dispatch::Json data;try{data=dispatch::Json::parse(body);}catch(const dispatch::Json::exception&){throw std::runtime_error("GitHub returned an unreadable release list.");}
    return evaluate(data,DISPATCH_VERSION_TEXT);
}
}
