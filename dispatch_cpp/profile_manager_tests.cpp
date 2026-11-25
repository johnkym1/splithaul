#include "profile_manager.h"
#include "profile_controls.h"
#include <iostream>

struct ProfileControlsTest {
    template<class Check>static void run(const std::filesystem::path& root,Check check){
        const auto parent=CreateWindowW(L"STATIC",L"Isolated profile controls",0,0,0,0,0,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        if(!parent)throw std::runtime_error("Cannot create profile control test window");
        std::array<profileManager::Protection,2> protection{};
        ProfileControls controls(parent,root,[]{return std::filesystem::path();},[&]{return protection;});
        check(SendMessageW(controls.combos[0],CB_GETCURSEL,0,0)==0&&!IsWindowEnabled(controls.buttons[0])&&!IsWindowEnabled(controls.buttons[1]),"Delete was enabled before profile selection");
        SendMessageW(controls.combos[0],CB_SETCURSEL,1,0);controls.command(320,CBN_SELCHANGE);
        check(IsWindowEnabled(controls.buttons[0])&&!IsWindowEnabled(controls.buttons[1]),"Profile selection did not enable only its own Delete button");
        protection[0]={true,"Same Name"};controls.refresh();
        check(!IsWindowEnabled(controls.buttons[0])&&SendMessageW(controls.combos[0],CB_GETITEMDATA,1,0)==0x47545052,"Loaded ATS profile was not disabled/marked");
        SendMessageW(controls.combos[1],CB_SETCURSEL,1,0);controls.command(322,CBN_SELCHANGE);
        check(IsWindowEnabled(controls.buttons[1]),"Loaded ATS profile blocked same-named ETS profile");
        protection[1]={true,""};controls.refresh();check(!IsWindowEnabled(controls.buttons[1]),"Unknown ETS identity did not disable Delete");
        protection={};controls.refresh();check(IsWindowEnabled(controls.buttons[0])&&IsWindowEnabled(controls.buttons[1]),"Delete did not recover after games closed");
        SendMessageW(controls.combos[0],CB_SETCURSEL,0,0);controls.command(320,CBN_SELCHANGE);check(!IsWindowEnabled(controls.buttons[0]),"Select one did not disable Delete");
        controls.layout(700,96);RECT combo={},button={};GetWindowRect(controls.combos[0],&combo);GetWindowRect(controls.buttons[0],&button);
        check(combo.right<button.left&&combo.right>combo.left,"Profile selector overlaps its button at minimum width");
        DestroyWindow(parent);
    }
};

int wmain(int argc,wchar_t** argv){try{
    if(argc!=2)throw std::runtime_error("Expected isolated test directory");
    const auto root=std::filesystem::absolute(argv[1])/live::uniqueId();unsigned checks=0;
    auto check=[&](bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);};
    auto rejected=[&](auto action){try{action();return false;}catch(const std::exception&){return true;}};
    auto create=[&](bool ets,const std::string& name){const auto path=profileManager::store(root,ets)/ats::profileFolderId(name);std::filesystem::create_directories(path);live::writeJson(path/"settings.config",{{"profile_name",name},{"mode",ets?2:0}});live::writeJson(path/"job_history.json",{{"keep","history"}});return path;};
    check(profileManager::list(root,false).empty(),"Missing profile store did not list as empty");
    const auto atsSame=create(false,"Same Name"),etsSame=create(true,"Same Name"),unused=create(false,"Unused");
    check(profileManager::list(root,false).size()==2&&profileManager::list(root,true).size()==1,"Game profile lists leaked or lost entries");
    const profileManager::Entry same={ats::profileFolderId("Same Name"),"Same Name"},other={ats::profileFolderId("Unused"),"Unused"};
    const profileManager::Protection none{},loaded={true,"Same Name"},unknown={true,""};
    check(rejected([&]{profileManager::archive(root,false,same,loaded,{});}),"Loaded profile was removed");
    check(rejected([&]{profileManager::archive(root,false,other,unknown,{});}),"Unknown running-game identity allowed deletion");
    check(rejected([&]{profileManager::archive(root,false,other,none,unused);}),"Active app profile was removed");
    check(profileManager::protectedEntry(same,loaded,etsSame,{}),"Loaded profile protection failed");
    check(!profileManager::protectedEntry(same,none,etsSame,atsSame),"ATS active profile blocked same-named ETS profile");
    check(rejected([&]{profileManager::archive(root,false,{"..","Unused"},none,{});}),"Traversal profile id was accepted");
    check(rejected([&]{profileManager::archive(root,false,{other.id,"Changed"},none,{});}),"Changed profile metadata was accepted");
    std::filesystem::create_directories(root/"profiles"/"not-a-profile");live::writeJson(root/"profiles"/"not-a-profile"/"settings.config",{{"profile_name","Unused"}});
    check(profileManager::list(root,false).size()==2,"Unrecognised folder was offered for deletion");
    const auto before=live::readJson(unused/"settings.config");const auto archived=profileManager::archive(root,false,other,loaded,{});
    check(!std::filesystem::exists(unused)&&std::filesystem::exists(archived),"Inactive profile was not moved to backup");
    check(live::readJson(archived/"settings.config")==before&&live::readJson(archived/"job_history.json").at("keep")=="history","Archived profile data changed");
    check(std::filesystem::exists(atsSame)&&std::filesystem::exists(etsSame),"Deleting one profile affected another");
    check(profileManager::list(root,false).size()==1,"Archived profile remained in active list");
    check(rejected([&]{profileManager::archive(root,false,other,none,{});}),"Stale selection was accepted twice");
    const auto etsArchived=profileManager::archive(root,true,same,none,atsSame);
    check(std::filesystem::exists(atsSame)&&!std::filesystem::exists(etsSame)&&std::filesystem::exists(etsArchived),"ETS deletion changed ATS data");
    std::filesystem::rename(etsArchived,etsSame);check(profileManager::list(root,true).size()==1,"Archived profile could not be restored");
    ProfileControlsTest::run(root,check);
    std::cout<<"PASS: "<<checks<<" app profile checks: game isolation, loaded/unknown/active protection, validation, archive integrity and recovery.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
