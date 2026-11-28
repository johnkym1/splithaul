#include "update_download.h"
#include <iostream>
static unsigned launched=0;static std::wstring launchedFile,launchedArgs;
static BOOL WINAPI fakeSetup(SHELLEXECUTEINFOW* info){++launched;launchedFile=info->lpFile;launchedArgs=info->lpParameters;return TRUE;}
static BOOL WINAPI cancelledSetup(SHELLEXECUTEINFOW*){SetLastError(ERROR_CANCELLED);return FALSE;}
static bool closedGames(){return false;}
static bool openGame(){return true;}
int main(int argc,char** argv){try{
    unsigned count=0;auto check=[&](bool ok,const char* message){++count;if(!ok)throw std::runtime_error(message);};
    auto release=[](const std::string& number,bool test=false){const auto name="SplitHaul-Setup-"+number+"-win64.exe";return dispatch::Json{{"tag_name","v"+number},{"html_url",std::string(updates::releasePrefix)+"v"+number},{"published_at","2026-11-24T00:00:00Z"},{"draft",false},{"prerelease",test},{"assets",dispatch::Json::array({{{"name",name},{"state","uploaded"},{"size",100},{"browser_download_url","https://github.com/johnkym1/splithaul/releases/download/v"+number+"/"+name},{"digest","sha256:"+std::string(64,'a')}},{{"name",name+".sha256"},{"state","uploaded"},{"size",100},{"browser_download_url","https://github.com/johnkym1/splithaul/releases/download/v"+number+"/"+name+".sha256"}}})}};};
    check(updates::version("v0.2.10")>updates::version("0.2.9"),"Version comparison is not numeric");
    for(const auto* bad:{"", "v0.2", "v0.2.9-beta", "v0.2.9/path", "v0.02.9", "999999999999.1.1", "0.2.9.0"})check(!updates::version(bad),"Malformed version accepted");
    const auto newer=release("0.2.10");auto result=updates::evaluate(dispatch::Json::array({newer}),"0.2.9");
    check(result.state==updates::State::available&&result.latest=="0.2.10"&&result.url==std::string(updates::releasePrefix)+"v0.2.10","New release not detected");
    check(updates::evaluate(dispatch::Json::array({release("0.2.9")}),"0.2.9").state==updates::State::current,"Matching version did not report current");
    check(updates::evaluate(dispatch::Json::array({release("0.2.8")}),"0.2.9").state==updates::State::ahead,"Local newer build was offered a downgrade");
    result=updates::evaluate(dispatch::Json::array({release("0.2.9"),release("0.2.11"),newer}),"0.2.8");check(result.latest=="0.2.11","Release order overrode highest version");
    const auto test=release("0.3.0",true);check(updates::evaluate(dispatch::Json::array({test}),"0.2.9").prerelease,"Test releases were lost");
    check(updates::evaluate(dispatch::Json::array({test}),"0.2.9",false).state==updates::State::none,"Stable channel accepted a test release");
    auto invalid=newer;invalid["draft"]=true;check(updates::evaluate(dispatch::Json::array({invalid}),"0.2.9").state==updates::State::none,"Draft release advertised");
    invalid=newer;invalid["published_at"]=nullptr;check(updates::evaluate(dispatch::Json::array({invalid}),"0.2.9").state==updates::State::none,"Unpublished release advertised");
    invalid=newer;invalid["assets"].erase(1);check(updates::evaluate(dispatch::Json::array({invalid}),"0.2.9").state==updates::State::none,"Installer without checksum advertised");
    invalid=newer;invalid["assets"][0]["state"]="new";check(updates::evaluate(dispatch::Json::array({invalid}),"0.2.9").state==updates::State::none,"Incomplete upload advertised");
    invalid=newer;invalid["html_url"]="https://example.com/update";check(updates::evaluate(dispatch::Json::array({invalid}),"0.2.9").state==updates::State::none,"External release link accepted");
    invalid=newer;invalid["tag_name"]="v0.2.10/evil";check(updates::evaluate(dispatch::Json::array({invalid}),"0.2.9").state==updates::State::none,"Unexpected release tag accepted");
    invalid=newer;invalid["assets"]=42;check(updates::evaluate(dispatch::Json::array({invalid,newer}),"0.2.9").state==updates::State::available,"Malformed release broke valid release discovery");
    check(updates::evaluate(dispatch::Json::array(),"0.2.9").state==updates::State::none,"Empty release list claimed up-to-date");
    bool rejected=false;try{updates::evaluate(dispatch::Json::object(),"0.2.9");}catch(const std::exception&){rejected=true;}check(rejected,"Error payload claimed no updates");
    for(const DWORD status:{403u,429u,404u,500u}){rejected=false;try{updates::requireSuccess(status);}catch(const std::exception&){rejected=true;}check(rejected,"HTTP error was treated as a successful update check");}
    updates::requireSuccess(200);
    for(const char* field:{"browser_download_url","digest"}){invalid=newer;invalid["assets"][0][field]="https://example.com/untrusted.exe";check(updates::evaluate(dispatch::Json::array({invalid}),"0.2.9").state==updates::State::none,"Unsafe installer metadata accepted");}
    invalid=newer;invalid["assets"][1]["browser_download_url"]="https://example.com/checksum";check(updates::evaluate(dispatch::Json::array({invalid}),"0.2.9").state==updates::State::none,"External checksum accepted");
    invalid=newer;invalid["assets"][0]["size"]=updates::maximumInstallerBytes+1;check(updates::evaluate(dispatch::Json::array({invalid}),"0.2.9").state==updates::State::none,"Oversized installer accepted");
    auto rejects=[](auto action){try{action();return false;}catch(const std::exception&){return true;}};
    check(updates::sha256("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA-256 implementation incorrect");
    const std::string bytes="MZ isolated test data";auto verified=updates::evaluate(dispatch::Json::array({newer}),"0.2.9");verified.digest=updates::sha256(bytes);verified.installerBytes=bytes.size();const auto sum=verified.digest+"  "+updates::installerName(verified)+"\r\n";
    updates::verifyAsset(verified,sum,bytes);check(true,"Valid installer/checksum rejected");
    check(rejects([&]{updates::verifyAsset(verified,sum,bytes+"x");}),"Truncated or extra installer bytes accepted");
    check(rejects([&]{updates::verifyAsset(verified,sum,"MZ corrupted payload");}),"Corrupt installer accepted");
    check(rejects([&]{updates::verifyAsset(verified,verified.digest+"  other.exe",bytes);}),"Checksum for another file accepted");
    check(rejects([&]{updates::verifyAsset(verified,std::string(64,'a')+"  "+updates::installerName(verified),bytes);}),"Checksum disagrees with GitHub digest");
    for(const auto* url:{L"http://github.com/johnkym1/splithaul/releases/download/v0.3.4/x",L"https://github.com.evil.test/johnkym1/splithaul/releases/download/x",L"https://release-assets.githubusercontent.com.evil.test/x",L"https://github.com/another/project/releases/download/x",L"https://release-assets.githubusercontent.com/x\r\nInjected: header"})check(!updates::trustedDownload(url),"Untrusted redirect accepted");
    check(updates::trustedDownload(L"https://release-assets.githubusercontent.com/github-production-release-asset/x?token=y"),"GitHub asset redirect rejected");
    std::atomic<bool> cancelled{true};check(rejects([&]{updates::fetchAsset(L"https://github.com/johnkym1/splithaul/releases/download/v0.3.4/x",100,cancelled);}),"Cancelled download continued");
    const auto fixture=dispatch::fs::temp_directory_path()/("splithaul-update-test-"+std::to_string(GetCurrentProcessId()));dispatch::fs::create_directories(fixture);
    check(updates::setupArguments(fixture)==L"/NORESTART /NOCLOSEAPPLICATIONS","Development app forced a setup target");
    {std::ofstream marker(fixture/"installed-layout.json");marker<<"{}";}
    check(updates::setupArguments(fixture).find(L"/DIR=\""+fixture.wstring()+L"\"")!=std::wstring::npos,"Existing installation directory not retained");
    {updates::Download temporary;temporary.directory=fixture/"download";dispatch::fs::create_directory(temporary.directory);temporary.file=temporary.directory/"partial.exe";std::ofstream file(temporary.file);file<<"partial";}
    check(!dispatch::fs::exists(fixture/"download"),"Abandoned download not cleaned up");
    {updates::Download installer;installer.directory=fixture/"launch";dispatch::fs::create_directory(installer.directory);installer.file=installer.directory/"setup.exe";installer.bytes=bytes.size();installer.digest=verified.digest;{std::ofstream file(installer.file,std::ios::binary);file<<bytes;}
    check(rejects([&]{updates::launchInstaller(nullptr,installer,fakeSetup,openGame);})&&launched==0,"Running game did not block installation");
    check(!updates::launchInstaller(nullptr,installer,cancelledSetup,closedGames)&&dispatch::fs::exists(installer.file),"UAC cancellation lost downloaded installer or closed app");
    {std::ofstream file(installer.file,std::ios::binary);file<<"MZ corrupted payload";}
    check(rejects([&]{updates::launchInstaller(nullptr,installer,fakeSetup,closedGames);})&&launched==0,"Tampered cached installer was launched");
    {std::ofstream file(installer.file,std::ios::binary);file<<bytes;}
    const auto savedFile=installer.file,savedDir=installer.directory;
    check(updates::launchInstaller(nullptr,installer,fakeSetup,closedGames)&&launched==1&&launchedFile==savedFile.wstring()&&launchedArgs.find(L"/NORESTART")!=std::wstring::npos,"Verified installer was not handed to Windows correctly");
    check(installer.file.empty()&&dispatch::fs::exists(savedFile),"Installer removed before Setup could use it");dispatch::fs::remove(savedFile);dispatch::fs::remove(savedDir);}
    dispatch::fs::remove(fixture/"installed-layout.json");dispatch::fs::remove(fixture);
    std::cout<<"PASS: "<<count<<" update checks: numeric versions, current/newer/ahead, test releases, drafts, complete installers and trusted release links.\n";
    if(argc==2&&(std::string(argv[1])=="--live"||std::string(argv[1])=="--download")){const auto live=updates::check();if(live.state==updates::State::none||live.state==updates::State::error)throw std::runtime_error("Live release discovery did not find the existing Windows installer");std::cout<<"LIVE PASS: installed " DISPATCH_VERSION_TEXT "; published "<<live.latest<<"; "<<live.url<<'\n';if(std::string(argv[1])=="--download"){std::atomic<bool> stop{false};const auto downloaded=updates::download(live,stop);std::cout<<"DOWNLOAD PASS: "<<downloaded->bytes<<" bytes; SHA-256 "<<downloaded->digest<<"; verified without launching Setup.\n";}}
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
