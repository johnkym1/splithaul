#include "ats_catalog.h"
#include "profile_files.h"
#include "job_history.h"
#include "../dispatch_shared/startup_diagnostic.h"
#include "cancel_hint.h"
#include "../dispatch_shared/cancel_protocol.h"
#include <iostream>
static unsigned checks=0;
static void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
int wmain(int argc,wchar_t** argv){try{
    if(argc!=3)throw std::runtime_error("Expected project and isolated test directory");
    const live::fs::path project=argv[1],temp=live::fs::path(argv[2])/live::uniqueId();
    dispatchGame::select(false);const auto atsBridge=live::ipcFolder();
    const auto atsProfile=ats::ensureProfileFiles(temp,"Same Profile");
    live::Request request;request.id=live::uniqueId();request.session=live::uniqueId();request.context="context";request.trailer="trailer";request.created=1000;request.expires=2000;
    request.route={"apples","paris","depot_a","lyon","depot_b"};
    const auto atsRequest=live::requestJson(request);
    live::Json status={{"protocol",live::protocolVersion},{"game","ats"},{"session_id",request.session},{"updated_utc_ms",1000},{"online",true}};
    check(live::freshStatus(status,1000),"ATS status rejected");
    dispatchGame::select(true);check(live::ipcFolder()!=atsBridge,"Shared game bridge path");
    check(std::wstring(dispatchGame::executable)==L"eurotrucks2.exe","Wrong game process");
    check(!live::freshStatus(status,1000),"ATS heartbeat accepted as ETS2");
    bool rejected=false;try{live::parseRequest(atsRequest);}catch(...){rejected=true;}check(rejected,"ATS request accepted as ETS2");
    const auto etsRequest=live::requestJson(request);check(etsRequest.at("game")=="ets2"&&live::parseRequest(etsRequest).id==request.id,"ETS2 request roundtrip failed");
    const auto etsProfile=ats::ensureProfileFiles(temp,"Same Profile");check(etsProfile!=atsProfile,"Same named profiles collide");
    check(live::readJson(etsProfile/"settings.config").at("mode")==2,"ETS profile defaults to ATS");
    status["game"]="ets2";check(live::freshStatus(status,1000),"ETS status rejected");
    live::Json diagnostic={{"protocol",live::protocolVersion},{"game","ets2"},{"online",false},{"state","startup_rejected"},{"pid",42},{"updated_utc_ms",1500},{"message","Unsupported build"}};
    check(live::startupDiagnostic(diagnostic,42,1000,2000)=="Unsupported build","ETS startup diagnostic hidden");
    check(!ats::cancelUnavailableReason(true,true,false,status,true).empty(),"ETS cancellation enabled without DLL capability");
    status["experimental_cancel_available"]=true;status["driving"]=false;status["context_id"]="context";status["cancel_target_token"]="target";
    status["menu_snapshot"]={{"captured",true},{"speed_mps",0.0},{"stopped",true},{"ignition_on",false}};
    check(ats::cancelUnavailableReason(true,true,false,status,true).empty(),"Guarded ETS cancellation remains disabled");
    check(!ats::cancelUnavailableReason(false,true,false,status,true).empty(),"Offline ETS cancellation enabled");
    check(!ats::cancelUnavailableReason(true,true,true,status,true).empty(),"Duplicate pending ETS cancellation enabled");
    status["driving"]=true;check(!ats::cancelUnavailableReason(true,true,false,status,true).empty(),"Driving ETS cancellation enabled");status["driving"]=false;
    live::CancelRequest cancel;cancel.id=live::uniqueId();cancel.session=request.session;cancel.context="context";cancel.profileId=ats::profileFolderId("Same Profile");cancel.target="target";cancel.created=1000;cancel.expires=2000;
    auto etsCancel=live::cancelRequestJson(cancel);check(etsCancel.at("game")=="ets2"&&live::parseCancelRequest(etsCancel).id==cancel.id,"ETS cancel roundtrip failed");
    auto wrongCancel=etsCancel;wrongCancel["game"]="ats";rejected=false;try{live::parseCancelRequest(wrongCancel);}catch(...){rejected=true;}check(rejected,"ATS cancellation accepted by ETS");
    ats::Catalog catalog(project);check(catalog.locations.count("paris")&&catalog.locations.count("berlin"),"ETS city catalog missing");
    check(!catalog.locations.count("yakima"),"ATS cities leaked into ETS");check(!catalog.cargo.empty(),"ETS cargo catalog empty");
    live::fs::create_directories(temp/"bridge_ets2/results");
    ats::exportJobHistory(etsProfile,temp/"bridge_ets2",&catalog,true);
    check(!live::fs::exists(atsProfile/"job_history.json"),"ETS history modified ATS profile");
    dispatchGame::select(false);rejected=false;try{live::parseRequest(etsRequest);}catch(...){rejected=true;}check(rejected,"ETS request accepted as ATS");
    rejected=false;try{live::parseCancelRequest(etsCancel);}catch(...){rejected=true;}check(rejected,"ETS cancellation accepted by ATS");
    check(live::ipcFolder()==atsBridge&&std::string(dispatchGame::savedDispatch)=="ats_dispatch.json","Switching back did not restore ATS storage");
    check(live::readJson(atsProfile/"settings.config").at("mode")==0,"ETS changed ATS preferences");
    std::cout<<"PASS: "<<checks<<" dual-game checks; ETS catalog "<<catalog.locations.size()<<" cities, "<<catalog.cargo.size()<<" cargo definitions.\n";
    return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
