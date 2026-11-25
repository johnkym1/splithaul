#include "history_page.h"
#include <iostream>
#include <fstream>

struct HistoryPageTest {
    static void run(const std::filesystem::path& root){
        unsigned checks=0;auto check=[&](bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);};
        auto create=[&](bool ets,const std::string& name,unsigned jobs){
            const auto folder=profileManager::store(root,ets)/ats::profileFolderId(name);std::filesystem::create_directories(folder);
            live::writeJson(folder/"settings.config",{{"profile_name",name},{"mode",ets?2:0}});
            auto records=live::Json::array();for(unsigned n=0;n<jobs;++n){const auto id=live::uniqueId(),session=live::uniqueId();const live::Route route={"wood",ets?"paris":"yakima","depot_a",ets?"berlin":"wenatchee","depot_b"};
                records.push_back({{"receipt",{{"protocol",4},{"request_id",id},{"session_id",session},{"state","delivered"},{"updated_utc_ms",1700000000000ULL+n},{"route",live::routeJson(route)},{"job",{{"payload_known",true},{"cargo_mass_kg",1200},{"cargo_units",4}}},{"details",{{"distance.km",123},{"revenue",456}}}}},{"plan_id",""},{"stop",0}});
            }live::writeJson(folder/"job_history.json",{{"version",1},{"jobs",records}});return folder;
        };
        const auto atsProfile=create(false,"Shared Name",2),etsProfile=create(true,"Shared Name",1),empty=create(true,"Empty",0);
        const auto atsBefore=live::readJson(atsProfile/"job_history.json"),etsBefore=live::readJson(etsProfile/"job_history.json");
        INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&common);
        const auto parent=CreateWindowW(L"STATIC",L"Offline history test",0,0,0,0,0,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);if(!parent)throw std::runtime_error("Cannot create hidden history test window");
        AppTheme theme;HistoryPage page(parent,GetModuleHandleW(nullptr),root,theme);dispatchGame::select(false);page.show(true);
        check(page.profileData.empty()&&!IsWindowEnabled(page.profileCombo)&&!IsWindowEnabled(page.exportButton),"Offline history required or invented a loaded game profile");
        SendMessageW(page.gameCombo,CB_SETCURSEL,1,0);page.command(613,CBN_SELCHANGE);
        check(page.profiles.size()==1&&page.profiles[0].name=="Shared Name"&&page.profileData.empty(),"ATS selection did not isolate its profiles");
        SendMessageW(page.profileCombo,CB_SETCURSEL,1,0);page.command(614,CBN_SELCHANGE);
        check(page.profileData==atsProfile&&page.entries.size()==2&&IsWindowEnabled(page.selectedExportButton),"ATS offline profile history did not load");
        auto caption=[](HWND control){const int length=GetWindowTextLengthW(control);std::wstring value(static_cast<size_t>(length)+1,L'\0');GetWindowTextW(control,value.data(),length+1);value.resize(static_cast<size_t>(length));return value;};
        check(caption(page.details).find(L"Pickup\r\nYakima")!=std::wstring::npos&&caption(page.resultDetails).find(L"Cargo units: 4")!=std::wstring::npos,"Readable detail sections lost route or payload fields");
        check((GetWindowLongW(page.details,GWL_STYLE)&ES_READONLY)&&(GetWindowLongW(page.resultDetails,GWL_STYLE)&WS_VSCROLL),"Job details are not read-only and scrollable");
        SendMessageW(page.details,EM_SETSEL,10,20);page.refresh();DWORD selectionStart=0,selectionEnd=0;SendMessageW(page.details,EM_GETSEL,reinterpret_cast<WPARAM>(&selectionStart),reinterpret_cast<LPARAM>(&selectionEnd));
        check(selectionStart==10&&selectionEnd==20,"History refresh reset the user's detail-text selection");
        const auto originalEntry=page.entries.front();page.entries.front().driverConfirmed=true;page.entries.front().revenue.reset();page.entries.front().distanceKm.reset();page.entries.front().damage.reset();page.describeSelection();
        check(caption(page.resultDetails).find(L"Driver confirmation time")!=std::wstring::npos&&caption(page.resultDetails).find(L"Game completion was not recorded.")!=std::wstring::npos&&caption(page.resultDetails).find(L"Revenue: Not recorded")!=std::wstring::npos,"Details confused driver confirmation with a measured game completion");
        page.entries.front()=originalEntry;ListView_SetItemState(page.list,-1,0,LVIS_SELECTED);page.describeSelection();
        check(caption(page.resultDetails).empty()&&!IsWindowEnabled(page.selectedExportButton),"Clearing the selected job retained stale result details");
        ListView_SetItemState(page.list,0,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);page.describeSelection();
        page.setProfile({});check(page.profileData==atsProfile&&page.entries.size()==2,"Empty live-profile tick erased manual history");
        SendMessageW(page.gameCombo,CB_SETCURSEL,2,0);page.command(613,CBN_SELCHANGE);
        check(page.profiles.size()==2&&page.entries.empty()&&!IsWindowEnabled(page.exportButton),"Game switch retained another game's jobs");
        const auto found=std::find_if(page.profiles.begin(),page.profiles.end(),[](const profileManager::Entry& e){return e.name=="Shared Name";});
        SendMessageW(page.profileCombo,CB_SETCURSEL,static_cast<WPARAM>(found-page.profiles.begin()+1),0);page.command(614,CBN_SELCHANGE);
        check(page.profileData==etsProfile&&page.entries.size()==1&&page.entries.front().pickupCity=="Paris","ETS offline history used ATS jobs or labels");
        check(!dispatchGame::isETS2(),"History browsing changed the live Dispatch game target");
        page.setProfile(atsProfile);check(page.profileData==etsProfile,"Live profile notifications interrupted manual history browsing");
        page.command(615,BN_CLICKED);check(page.profileData==atsProfile&&!page.manualBrowsing,"Loaded profile shortcut did not resume following");
        page.setProfile({});check(page.profileData==atsProfile&&page.entries.size()==2,"Game disconnect erased visible recorded history");
        const auto entry=page.entries.front();const auto selected=root/"Selected job.csv";ats::saveHistoryCsvSelection(atsProfile,selected,nullptr,entry.id,entry.session);
        std::ifstream input(selected,std::ios::binary);const std::string csv={std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
        check(csv.find(entry.id)!=std::string::npos&&csv.find(page.entries.back().id)==std::string::npos,"Selected export included another job");
        check(live::readJson(atsProfile/"job_history.json")==atsBefore&&live::readJson(etsProfile/"job_history.json")==etsBefore&&!std::filesystem::exists(atsProfile/"user.data"),"History browsing/export rewrote saved data or required generated CSV");
        const auto all=root/"All jobs.csv";ats::saveHistoryCsvSelection(atsProfile,all,nullptr);check(std::filesystem::file_size(all)>std::filesystem::file_size(selected),"Full export did not include the profile's other jobs");
        page.selectedGame=1;SendMessageW(page.gameCombo,CB_SETCURSEL,2,0);page.selectProfile(empty,"Empty");page.refresh();
        check(page.entries.empty()&&!IsWindowEnabled(page.exportButton)&&!IsWindowEnabled(page.selectedExportButton),"Empty profile enabled an export");
        bool rejected=false;try{ats::saveHistoryCsvSelection(empty,root/"Empty.csv",nullptr);}catch(const std::exception&){rejected=true;}check(rejected,"Empty history silently exported a blank CSV");
        rejected=false;try{ats::saveHistoryCsvSelection(atsProfile,root/"Wrong session.csv",nullptr,entry.id,live::uniqueId());}catch(const std::exception&){rejected=true;}check(rejected,"Selected export accepted a job from the wrong session");
        rejected=false;try{ats::saveHistoryCsvSelection(atsProfile,atsProfile/"job_history.json",nullptr,entry.id,entry.session);}catch(const std::exception&){rejected=true;}check(rejected&&live::readJson(atsProfile/"job_history.json")==atsBefore,"Export overwrote the archive");
        page.layout(700,980,96);RECT selector={},follow={},selectedButton={},refreshButton={};GetWindowRect(page.profileCombo,&selector);GetWindowRect(page.followButton,&follow);GetWindowRect(page.selectedExportButton,&selectedButton);GetWindowRect(page.refreshButton,&refreshButton);
        check(selector.right<follow.left&&selectedButton.right<refreshButton.left,"History controls overlap at minimum size");
        RECT grid={},detailBody={},resultBody={},heading={},exportButton={};GetWindowRect(page.list,&grid);GetWindowRect(page.details,&detailBody);GetWindowRect(page.resultDetails,&resultBody);GetWindowRect(page.detailHeading,&heading);GetWindowRect(page.exportButton,&exportButton);
        check(grid.bottom-grid.top<350&&detailBody.bottom-detailBody.top>=280&&grid.bottom<heading.top&&detailBody.right<resultBody.left&&resultBody.bottom<exportButton.top,"Smaller history grid and larger detail panels overlap or lack reading space");
        LOGFONTW font={};GetObjectW(page.detailFont,sizeof(font),&font);check(font.lfHeight==-16,"Detail text did not use the larger 12-point reading font");
        DestroyWindow(parent);std::cout<<"PASS: "<<checks<<" offline history checks: game/profile browsing, independent dispatch target, loaded shortcut and selected/all exports.\n";
    }
};
int wmain(int argc,wchar_t** argv){try{if(argc!=2)throw std::runtime_error("Expected isolated directory");const auto root=std::filesystem::absolute(argv[1])/live::uniqueId();std::filesystem::create_directories(root);HistoryPageTest::run(root);return 0;}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
