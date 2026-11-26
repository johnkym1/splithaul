#include "dispatch_core.h"
#include "app_options.h"
#include "win_text.h"
#include <chrono>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace dispatch;
static unsigned checks=0;
static void check(bool value,const char* message) {++checks;if(!value)throw std::runtime_error(message);}
static void file(const fs::path& path,const std::string& content) {
    fs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary|std::ios::trunc);out<<content;if(!out)throw std::runtime_error("Fixture write failed");
}
static State sample() {
    State s;s.values={"Berlin","NBFC","Diesel","London","POSPED","Amsterdam","PRIMEL"};return s;
}
static void fixture(const fs::path& root) {
    file(root/"game_data/base/def/company/nbfc/editor/berlin.sii","SiiNunit\n{\n city : berlin\n}\n");
    file(root/"game_data/base/def/company/primel/editor/amsterdam.sii"," city: amsterdam\n");
    file(root/"game_data/base/def/company/posped/editor/london.sii","city: london\n");
    file(root/"game_data/base/def/company/remote/editor/madrid.sii","city: madrid\n");
    file(root/"game_data/base/def/company/nbfc/out/diesel.sii","cargo: cargo.diesel\ncargo: \"cargo.diesel\"\n");
    file(root/"game_data/base/def/company/nbfc/out/acid.sii","cargo: cargo.acid\n");
    file(root/"game_data/base/def/company/primel/in/diesel.sii","cargo : cargo.diesel\n");
    file(root/"game_data/base/def/company/posped/in/diesel.sii","cargo: cargo.diesel\n");
    file(root/"game_data/base/def/company/remote/in/diesel.sii","cargo: cargo.diesel\n");
    const Coordinates coords={{"Berlin",{0,0}},{"Amsterdam",{0,5}},{"London",{0,10}},{"Madrid",{50,50}}};
    writeJsonAtomic(root/"city_coordinates.json",Json(coords));
}
int wmain(int argc,wchar_t** argv) {
    try {
        if(argc!=3)throw std::runtime_error("Expected project and isolated test directory");
        check(winText::toWide("").empty()&&winText::toUtf8(L"").empty(),"Empty Windows text conversion failed");
        const std::string international=u8"Crème / Å / Łódź / 東京 / \U0001F69A";
        check(winText::toUtf8(winText::toWide(international))==international,"International text and supplementary characters did not round-trip");
        const std::string embeddedNull("A\0B",3);
        check(winText::toUtf8(winText::toWide(embeddedNull))==embeddedNull,"Text conversion truncated an embedded null");
        bool invalidTextRejected=false;
        try{winText::toWide(std::string("\xC3",1));}catch(const std::runtime_error&){invalidTextRejected=true;}
        check(invalidTextRejected,"Strict conversion accepted incomplete UTF-8");
        check(winText::toWide(std::string("\xC3",1),0)==L"\uFFFD","Live display replacement of malformed UTF-8 changed");
        check(winText::toUtf8(std::wstring(1,static_cast<wchar_t>(0xD800)))==u8"\uFFFD","Windows replacement of an unpaired surrogate changed");
        bool oversizedTextRejected=false;
        try{winText::length(static_cast<size_t>((std::numeric_limits<int>::max)())+1);}catch(const std::length_error&){oversizedTextRejected=true;}
        check(oversizedTextRejected,"Oversized Windows text length was truncated");
        const fs::path actual=fs::absolute(argv[1]),isolated=fs::absolute(argv[2]);fixture(isolated);
        Model m(isolated);m.refresh();
        check(m.catalog.size()==4&&m.catalog.at("Amsterdam")==std::vector<std::string>{"PRIMEL"},"Native catalog did not match definition files");
        check(m.cargoes("NBFC").size()==2&&m.cargoes("NBFC").count("Diesel"),"Outgoing cargo parsing/deduplication failed");
        auto s=sample();check(m.validate(s).empty(),"Valid route rejected");
        auto candidates=m.choices(MiddleCity,s);check(candidates==std::vector<std::string>{"Amsterdam"},"Cargo/corridor filtering failed");
        auto bad=s;bad.values[Cargo]="Acid";check(!m.validate(bad).empty(),"Incompatible cargo accepted");
        bad=s;bad.values[PickupCompany]="NOT_IN_BERLIN";check(!m.validate(bad).empty(),"Company/city mismatch accepted");
        bad=s;bad.values[MiddleCity]="Madrid";bad.values[MiddleCompany]="REMOTE";check(!m.validate(bad).empty(),"Off-route middle stop accepted");
        bad=s;bad.values[FinalCompany].clear();check(!m.validate(bad).empty(),"Incomplete route accepted");
        Json legacy={{"j1","Berlin"},{"d2","Amsterdam"},{"d1","London"}};
        const auto converted=decodeState(legacy);check(converted.values[StartCity]=="Berlin"&&converted.values[MiddleCity]=="Amsterdam"&&converted.values[FinalCity]=="London","Legacy settings migration failed");
        auto unicode=s;unicode.values[Cargo]=u8"Crème / Å";m.save(unicode);check(loadState(isolated).values==unicode.values,"UTF-8 settings round-trip failed");
        const auto request=jobRequest(s);check(request.at("destination_city")=="Amsterdam"&&request.at("destination_company")=="PRIMEL"&&request.at("status")=="pending_game_adapter","Wrong first-leg request");
        m.start(s);check(s.stage=="to_d2"&&loadState(isolated).stage=="to_d2","Plan start was not persisted");
        check(readJson(isolated/"game_job_request.json")==request,"Persisted pending request changed format");
        check(advance(s)&&s.stage=="to_d1","First-drop transition failed");check(advance(s)&&s.stage=="done","Final transition failed");check(!advance(s),"Completed plan advanced again");
        file(isolated/"dispatch.json","{malformed");bool rejected=false;try{loadState(isolated);}catch(const std::exception&){rejected=true;}check(rejected,"Malformed state silently accepted");
        rejected=false;try{decodeState(Json{{"stage","unknown"}});}catch(const std::exception&){rejected=true;}check(rejected,"Unknown stage silently accepted");
        m.save(sample());
        const auto optionsCase=isolated/("options-"+std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()));fs::create_directories(optionsCase);
        const auto defaultOptions=loadOptions(optionsCase);check(defaultOptions.mode==0&&!defaultOptions.dark,"Default app options changed unexpectedly");
        check(!defaultOptions.fullCapacity&&!decodeOptions(Json{{"mode",2}}).fullCapacity,"Existing settings unexpectedly bypassed route limits");
        saveOptions(optionsCase,{2,false,false,true});check(loadOptions(optionsCase).fullCapacity,"Full trailer load preference was not saved");
        rejected=false;try{decodeOptions(Json{{"full_trailer_loads","yes"}});}catch(const std::exception&){rejected=true;}check(rejected,"Malformed load preference accepted");
        saveOptions(optionsCase,{1,true});const auto restoredOptions=loadOptions(optionsCase);check(restoredOptions.mode==1&&restoredOptions.dark,"Theme and dispatch mode did not survive reopening");
        check(modeForRunningGame(0,RunningGames::ets2)==2&&modeForRunningGame(1,RunningGames::ets2)==2,"ETS2 did not replace an ATS selection with ETS single");
        check(modeForRunningGame(2,RunningGames::ets2)==2&&modeForRunningGame(3,RunningGames::ets2)==3,"ETS2 did not keep its selected job type");
        check(modeForRunningGame(2,RunningGames::ats)==0&&modeForRunningGame(3,RunningGames::ats)==0,"ATS did not replace an ETS2 selection with ATS single");
        check(modeForRunningGame(0,RunningGames::ats)==0&&modeForRunningGame(1,RunningGames::ats)==1,"ATS did not keep its selected job type");
        check(modeForRunningGame(3,RunningGames::none)==3&&modeForRunningGame(1,RunningGames::both)==1,"Unknown or simultaneous games changed the selected mode");
        check(decodeOptions(Json{{"mode",4}}).mode==4&&decodeOptions(Json{{"mode",5}}).mode==5,"Multi-drop modes were rejected");
        check(modeForRunningGame(4,RunningGames::ats)==4&&modeForRunningGame(5,RunningGames::ets2)==5&&modeForRunningGame(4,RunningGames::ets2)==2&&modeForRunningGame(5,RunningGames::ats)==0,"Multi-drop running-game selection failed");
        rejected=false;try{decodeOptions(Json{{"mode",6},{"dark_theme",true}});}catch(const std::exception&){rejected=true;}check(rejected,"Unsupported mode was accepted in app options");
        rejected=false;try{decodeOptions(Json{{"mode",0},{"dark_theme","yes"}});}catch(const std::exception&){rejected=true;}check(rejected,"Malformed theme option was accepted");
        auto singlePlan=sample();singlePlan.values[MiddleCity].clear();singlePlan.values[MiddleCompany].clear();m.startSingle(singlePlan);
        check(singlePlan.stage=="to_d1"&&readJson(isolated/"game_job_request.json").at("destination_city")==singlePlan.values[FinalCity],"ETS single planner did not target its sole delivery");
        check(loadState(isolated).stage=="setup"&&decodeState(readJson(isolated/"dispatch_single.json")).stage=="to_d1","ETS single planner overwrote the multiple-stop route");
        // Read-only checks against the user's real files. Never refresh or save
        // the actual project from this test process.
        Model real(actual);check(!real.catalog.empty()&&!real.coords.empty(),"Existing catalogs failed to load");
        const auto saved=loadState(actual);check(decodeState(encodeState(saved)).values==saved.values,"Existing settings failed round-trip");
        check(real.cargoes("NBFC").count("Diesel")!=0,"Real company definition cargo missing");
        const auto middle=suitableStops("Berlin","London",real.catalog,real.coords);check(std::find(middle.begin(),middle.end(),"Amsterdam")!=middle.end(),"Existing coordinate filtering regressed");
        const auto ui=isolated/"ui-review";fixture(ui);Model uiModel(ui);uiModel.refresh();uiModel.save(sample());
        std::cout<<"PASS: "<<checks<<" checks: native catalog/cargo parsing, compatibility, route filtering, legacy/UTF-8 settings, pending request, stage transitions, malformed data rejection and existing project data.\n";
        std::cout<<"Isolated UI review data: "<<ui.u8string()<<"\n";return 0;
    }catch(const std::exception& ex){std::cerr<<"FAIL: "<<ex.what()<<"\n";return 1;}
}
