#include "update_check.h"
#include <iostream>
int main(int argc,char** argv){try{
    unsigned count=0;auto check=[&](bool ok,const char* message){++count;if(!ok)throw std::runtime_error(message);};
    auto release=[](const std::string& number,bool test=false){const auto name="SplitHaul-Setup-"+number+"-win64.exe";return dispatch::Json{{"tag_name","v"+number},{"html_url",std::string(updates::releasePrefix)+"v"+number},{"published_at","2026-11-24T00:00:00Z"},{"draft",false},{"prerelease",test},{"assets",dispatch::Json::array({{{"name",name},{"state","uploaded"},{"size",100}},{{"name",name+".sha256"},{"state","uploaded"},{"size",100}}})}};};
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
    std::cout<<"PASS: "<<count<<" update checks: numeric versions, current/newer/ahead, test releases, drafts, complete installers and trusted release links.\n";
    if(argc==2&&std::string(argv[1])=="--live"){const auto live=updates::check();if(live.state==updates::State::none||live.state==updates::State::error)throw std::runtime_error("Live release discovery did not find the existing Windows installer");std::cout<<"LIVE PASS: installed " DISPATCH_VERSION_TEXT "; published "<<live.latest<<"; "<<live.url<<'\n';}
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
