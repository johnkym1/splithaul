#include "live_panel.h"
#include "dispatch_layout.h"
#include "dispatch_core.h"
#include <commctrl.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cwchar>
#include "../dispatch_shared/payload.h"
#include "../dispatch_shared/trailer_combination.h"
#include "../dispatch_shared/startup_diagnostic.h"
#include "job_history.h"
#include <shlobj.h>
#include <fstream>
#include "ats_profile_welcome.h"
#include "profile_files.h"
#include "restart_match.h"
#include "../dispatch_shared/cancel_protocol.h"
#include "cancel_hint.h"
#include "win_text.h"

namespace {
std::wstring wide(const std::string& input) {
    return winText::toWide(dispatchGame::displayText(input), 0);
}
void text(HWND h,const std::string& s){const auto next=wide(s);std::wstring current(static_cast<size_t>(GetWindowTextLengthW(h))+1,L'\0');GetWindowTextW(h,current.data(),static_cast<int>(current.size()));current.resize(wcslen(current.c_str()));if(current!=next)SetWindowTextW(h,next.c_str());}
std::string tonnes(double kg){std::ostringstream s;s<<std::fixed<<std::setprecision(2)<<kg/1000<<" t";return s.str();}
bool runningGame(const live::Json& status,uint64_t* created=nullptr) {
    const auto pid=status.value("pid",DWORD(0));if(!pid)return false;
    HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!p)return false;
    wchar_t path[32768];DWORD count=32768;const bool ok=QueryFullProcessImageNameW(p,0,path,&count)!=FALSE;DWORD code=0;const bool alive=GetExitCodeProcess(p,&code)&&code==STILL_ACTIVE;
    if(created){FILETIME birth={},exit={},kernel={},user={};*created=0;if(GetProcessTimes(p,&birth,&exit,&kernel,&user)){ULARGE_INTEGER stamp;stamp.LowPart=birth.dwLowDateTime;stamp.HighPart=birth.dwHighDateTime;*created=(stamp.QuadPart-116444736000000000ULL)/10000;}}CloseHandle(p);
    return ok&&alive&&_wcsicmp(std::filesystem::path(path).filename().c_str(),dispatchGame::executable)==0;
}
bool observedGameExited(const live::Json& previous,uint64_t created) {
    const auto pid=previous.value("pid",DWORD(0));if(!pid)return false;
    HANDLE process=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if(!process)return GetLastError()==ERROR_INVALID_PARAMETER;
    bool exited=WaitForSingleObject(process,0)==WAIT_OBJECT_0;
    if(!exited&&created){FILETIME birth={},exit={},kernel={},user={};if(GetProcessTimes(process,&birth,&exit,&kernel,&user)){ULARGE_INTEGER stamp;stamp.LowPart=birth.dwLowDateTime;stamp.HighPart=birth.dwHighDateTime;exited=(stamp.QuadPart-116444736000000000ULL)/10000!=created;}}
    CloseHandle(process);return exited;
}
std::string currentProfile(const live::Json& status) {
    uint64_t created=0;if(!runningGame(status,&created)||!created)return {};
    PWSTR documents=nullptr;if(FAILED(SHGetKnownFolderPath(FOLDERID_Documents,0,nullptr,&documents)))return {};
    const auto path=std::filesystem::path(documents)/dispatchGame::documents/L"game.log.txt";CoTaskMemFree(documents);
    WIN32_FILE_ATTRIBUTE_DATA info={};if(!GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&info))return {};
    ULARGE_INTEGER stamp;stamp.LowPart=info.ftLastWriteTime.dwLowDateTime;stamp.HighPart=info.ftLastWriteTime.dwHighDateTime;
    if(stamp.QuadPart/10000ULL<created+11644473600000ULL)return {};
    std::ifstream input(path,std::ios::binary|std::ios::ate);if(!input)return {};
    const auto size=input.tellg();if(size<=0||size>std::streamoff(32*1024*1024))return {};
    std::string contents(static_cast<size_t>(size),'\0');input.seekg(0);if(!input.read(contents.data(),size))return {};
    return ats::selectedProfileFromLog(contents);
}
live::Json bridgeStatus(const std::filesystem::path& bridge) {
    auto s=live::readJson(bridge/"status.json");uint64_t created=0;
    if(runningGame(s,&created)){const auto problem=live::startupDiagnostic(s,s.value("pid",DWORD(0)),created,live::utcMs());if(!problem.empty())throw std::runtime_error(problem);}
    if(!live::freshStatus(s,live::utcMs())||!s.contains("trailer")||!s["trailer"].is_object()||!s.contains("job")||!s["job"].is_object()||!s.contains("can_queue")||!s["can_queue"].is_boolean()||!runningGame(s))throw std::runtime_error("ATS live dispatch is offline. Start ATS and load your single-player test profile.");return s;
}
}
LivePanel::LivePanel(HWND h,HINSTANCE app,const std::filesystem::path& root):parent(h),instance(app),project(root),bridge(live::ipcFolder()) {
    try{catalog=std::make_unique<ats::Catalog>(project);}catch(const std::exception& e){catalogError=e.what();}
    const wchar_t* names[]={L"Pickup city",L"Pickup company",L"Cargo for attached trailer",L"Delivery city",L"Delivery company",L"Final delivery city",L"Final delivery company"};
    for(size_t i=0;i<7;++i){labels[i]=CreateWindowW(L"STATIC",names[i],WS_CHILD|SS_LEFT,0,0,0,0,parent,nullptr,instance,nullptr);combos[i]=CreateWindowW(WC_COMBOBOXW,L"",WS_CHILD|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST|CBS_HASSTRINGS|CBS_OWNERDRAWFIXED,0,0,0,0,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(400+i)),instance,nullptr);SendMessageW(combos[i],CB_SETMINVISIBLE,10,0);}
    percentLabel=CreateWindowW(L"STATIC",L"Final load remaining (%)",WS_CHILD,0,0,0,0,parent,nullptr,instance,nullptr);
    percentEdit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",std::to_wstring(savedPercent).c_str(),WS_CHILD|WS_TABSTOP|ES_NUMBER|ES_AUTOHSCROLL,0,0,0,0,parent,reinterpret_cast<HMENU>(407),instance,nullptr);SendMessageW(percentEdit,EM_SETLIMITTEXT,3,0);
    INITCOMMONCONTROLSEX spinnerControls={sizeof(spinnerControls),ICC_UPDOWN_CLASS};InitCommonControlsEx(&spinnerControls);
    percentSpin=CreateWindowExW(0,UPDOWN_CLASSW,L"",WS_CHILD|UDS_ARROWKEYS|UDS_SETBUDDYINT|UDS_NOTHOUSANDS,0,0,0,0,parent,reinterpret_cast<HMENU>(408),instance,nullptr);
    if(!percentSpin)throw std::runtime_error("Could not create the remaining-load number control.");
    SendMessageW(percentSpin,UDM_SETBUDDY,reinterpret_cast<WPARAM>(percentEdit),0);
    SendMessageW(percentSpin,UDM_SETRANGE32,1,99);SendMessageW(percentSpin,UDM_SETPOS32,0,savedPercent);
    trailerText=CreateWindowW(L"STATIC",L"",WS_CHILD,0,0,0,0,parent,nullptr,instance,nullptr);help=CreateWindowW(L"STATIC",L"",WS_CHILD,0,0,0,0,parent,nullptr,instance,nullptr);
    dispatchButton=CreateWindowW(L"BUTTON",L"Dispatch job to the game",WS_CHILD|WS_TABSTOP|BS_DEFPUSHBUTTON,0,0,0,0,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(DispatchId)),instance,nullptr);
    acknowledgeButton=CreateWindowW(L"BUTTON",L"Acknowledge completion",WS_CHILD|WS_TABSTOP,0,0,0,0,parent,reinterpret_cast<HMENU>(502),instance,nullptr);
    cancelButton=CreateWindowW(L"BUTTON",L"Cancel current job",WS_CHILD|WS_TABSTOP,0,0,0,0,parent,reinterpret_cast<HMENU>(503),instance,nullptr);
    cancelTooltip=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,nullptr,WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,parent,nullptr,instance,nullptr);
    if(cancelTooltip){TOOLINFOW tool={};tool.cbSize=sizeof(tool);tool.uFlags=TTF_SUBCLASS;tool.hwnd=parent;tool.uId=503;tool.lpszText=const_cast<wchar_t*>(L"");SendMessageW(cancelTooltip,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&tool));SendMessageW(cancelTooltip,TTM_SETMAXTIPWIDTH,0,350);}
    const DWORD textStyle=WS_CHILD|WS_TABSTOP|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL;
    statusText=CreateWindowW(L"EDIT",L"",textStyle,0,0,0,0,parent,nullptr,instance,nullptr);jobText=CreateWindowW(L"EDIT",L"",textStyle,0,0,0,0,parent,nullptr,instance,nullptr);tick();
}
void LivePanel::selectGame(bool ets) {
    if(dispatchGame::isETS2()==ets)return;
    if(shouldWarnOnClose()||!pendingCancelId.empty())throw std::runtime_error("Finish or acknowledge the current dispatch before switching games.");
    dispatchGame::select(ets);bridge=live::ipcFolder();loadProfile("");displayedProfile.clear();
    online=gameClosed=connectedDuringRun=false;observedGameCreated=lastHistorySync=lastResumeAttempt=0;
    status=observedGame=live::Json::object();body.clear();signature.clear();offlineReason.clear();
    historyRows=0;historyError.clear();historyReceipt.clear();catalogError.clear();
    for(auto& choices:options)choices.clear();catalog.reset();
    try{catalog=std::make_unique<ats::Catalog>(project);}catch(const std::exception& ex){catalogError=ex.what();}
}
void LivePanel::loadProfile(const std::string& name) {
    profileData.clear();selected.fill({});lastId.clear();lastSession.clear();formClearedId.clear();reviewedSession.clear();
    submitted=0;savedPlan=false;savedPercent=50;result=live::Json::object();plan=live::Json::object();reviewPromptPosted=false;sendError.clear();pendingCancelId.clear();followCancelId.clear();cancelMessage.clear();
    if(!name.empty())try {
        const auto dir=ats::ensureProfileFiles(project,name);
        if(std::filesystem::exists(dir/dispatchGame::savedDispatch)) {
            const auto saved=live::readJson(dir/dispatchGame::savedDispatch);lastId=saved.value("last_request_id",std::string());lastSession=saved.value("session_id",std::string());submitted=saved.value("submitted_utc_ms",uint64_t(0));savedPlan=saved.value("two_stop",false);
            if(!lastId.empty()&&!live::requestId(lastId))throw std::runtime_error("Invalid saved dispatch identifier.");
            formClearedId=saved.value("form_cleared_for",std::string());reviewedSession=saved.value("completion_review_prompted_session",std::string());
            const auto cleanup=saved.value("cancel_cleanup",live::Json::object());const auto priorOutcome=cleanup.value("outcome",std::string());if(priorOutcome=="game_requested"||priorOutcome=="contact_lost"){const auto id=cleanup.value("request_id",std::string());if(live::requestId(id))followCancelId=id;}
            savedPercent=saved.value("remaining_percent",50u);if(savedPercent<1||savedPercent>99)savedPercent=50;
            if(formClearedId!=lastId||lastId.empty()){const auto r=live::parseRoute(saved.at("route"));selected={r.sourceCity,r.sourceCompany,r.cargo,r.targetCity,r.targetCompany};if(savedPlan){const auto final=live::parseRoute(saved.at("final_route"));selected[5]=final.targetCity;selected[6]=final.targetCompany;}}
        }
        profileData=dir;
        if(std::filesystem::exists(dir/"cancel_pending.json")){const auto pending=live::readJson(dir/"cancel_pending.json");pendingCancelId=pending.at("request_id").get<std::string>();if(!live::requestId(pendingCancelId))throw std::runtime_error("Invalid pending cancellation identifier.");}
    }catch(const std::exception& ex){profileData.clear();selected.fill({});lastId.clear();lastSession.clear();pendingCancelId.clear();sendError=std::string("Profile dispatch data: ")+ex.what();}
    if(percentEdit){syncing=true;SetWindowTextW(percentEdit,std::to_wstring(savedPercent).c_str());SendMessageW(percentSpin,UDM_SETPOS32,0,savedPercent);syncing=false;}
}
live::Route LivePanel::route() const {return {selected[2],selected[0],selected[1],selected[3],selected[4]};}
live::Route LivePanel::finalRoute() const {return {selected[2],selected[0],selected[1],selected[5],selected[6]};}
void LivePanel::setTwoStop(bool value) {twoStop=value;text(labels[3],value?"First delivery city":"Delivery city");text(labels[4],value?"First delivery company":"Delivery company");text(dispatchButton,value?"Start two-stop plan":"Dispatch job to ATS");}
void LivePanel::setRandom(bool value) {
    if(randomMiddle==value)return;
    randomMiddle=value;
    if(value&&twoStop&&online&&!body.empty()&&(lastId.empty()||formClearedId==lastId)&&!shouldWarnOnClose()&&!ats::awaitingAcknowledgement(savedPlan,lastId,lastSession,formClearedId,plan,result)&&!ats::canConfirmUnrecordedCompletion(savedPlan,lastId,lastSession,formClearedId,plan,result,status,live::utcMs())){selected[3].clear();selected[4].clear();}
    choices();render();
}
void LivePanel::show(bool value) {visible=value;for(size_t i=0;i<7;++i){ShowWindow(labels[i],value&&(i<5||twoStop)?SW_SHOW:SW_HIDE);ShowWindow(combos[i],value&&(i<5||twoStop)?SW_SHOW:SW_HIDE);}for(auto h:{percentLabel,percentEdit,percentSpin})ShowWindow(h,value&&twoStop?SW_SHOW:SW_HIDE);for(auto h:{trailerText,help,dispatchButton,acknowledgeButton,cancelButton,statusText,jobText})ShowWindow(h,value?SW_SHOW:SW_HIDE);if(!value&&cancelTooltip)SendMessageW(cancelTooltip,TTM_ACTIVATE,FALSE,0);if(value)tick();}
void LivePanel::layout(int width,int height,UINT dpi) {
    const DispatchLayout page(true,twoStop,height);
    auto place=[dpi](HWND h,int x,int y,int w,int hgt){MoveWindow(h,MulDiv(x,static_cast<int>(dpi),96),MulDiv(y,static_cast<int>(dpi),96),MulDiv(w,static_cast<int>(dpi),96),MulDiv(hgt,static_cast<int>(dpi),96),TRUE);};
    for(size_t i=0;i<7;++i){const int y=DispatchLayout::fieldTop(static_cast<int>(i));place(labels[i],44,y+4,216,26);place(combos[i],266,y,(std::min)(420,width-310),340);}
    place(percentLabel,44,DispatchLayout::fieldTop(7)+4,216,26);place(percentEdit,266,DispatchLayout::fieldTop(7),88,28);place(percentSpin,354,DispatchLayout::fieldTop(7),22,28);
    place(trailerText,44,page.guidanceTop+40,width-88,80);place(help,44,page.guidanceTop+124,width-88,40);
    const int buttonWidth=(width-112)/3;
    place(dispatchButton,44,page.actionsTop+38,buttonWidth,36);place(acknowledgeButton,56+buttonWidth,page.actionsTop+38,buttonWidth,36);place(cancelButton,68+buttonWidth*2,page.actionsTop+38,buttonWidth,36);
    const int column=(width-64)/2;
    place(statusText,44,page.statusTop+40,column-40,page.statusHeight-56);place(jobText,60+column,page.statusTop+40,column-40,page.statusHeight-56);
    if(cancelTooltip){RECT area={};GetWindowRect(cancelButton,&area);MapWindowPoints(HWND_DESKTOP,parent,reinterpret_cast<POINT*>(&area),2);TOOLINFOW tool={};tool.cbSize=sizeof(tool);tool.hwnd=parent;tool.uId=503;tool.rect=area;SendMessageW(cancelTooltip,TTM_NEWTOOLRECTW,0,reinterpret_cast<LPARAM>(&tool));}
}
unsigned LivePanel::remainingPercent() const {
    wchar_t value[8]={};const auto n=GetWindowTextW(percentEdit,value,8);
    if(n<1||n>2||!std::all_of(value,value+n,[](wchar_t c){return c>=L'0'&&c<=L'9';}))throw std::runtime_error("Choose a remaining percentage from 1 to 99.");
    const auto percent=static_cast<unsigned>(std::wcstoul(value,nullptr,10));if(percent<1||percent>99)throw std::runtime_error("Choose a remaining percentage from 1 to 99.");return percent;
}
bool LivePanel::shouldWarnOnClose() const {
    const bool same=online&&lastSession==status.value("session_id",std::string());
    const auto j=status.value("job",live::Json::object());
    if(online&&j.value("active",false))return true;
    const auto phase=plan.value("state",std::string());
    if(same&&savedPlan&&!phase.empty()&&phase!="complete"&&phase!="stopped"&&phase!="uncertain")return true;
    return same&&!lastId.empty()&&!live::terminal(result.value("state",std::string()))&&(!result.empty()||(submitted&&live::utcMs()<submitted+125000));
}
void LivePanel::choices() {
    syncing=true;
    const bool awaiting=ats::awaitingAcknowledgement(savedPlan,lastId,lastSession,formClearedId,plan,result);
    const bool autoMiddle=twoStop&&randomMiddle&&online&&!body.empty()&&(lastId.empty()||formClearedId==lastId)&&!shouldWarnOnClose()&&!awaiting&&!ats::canConfirmUnrecordedCompletion(savedPlan,lastId,lastSession,formClearedId,plan,result,status,live::utcMs());
    // Populate the final city first so its current value can constrain the first stop.
    for(const size_t i:{0u,1u,2u,5u,6u,3u,4u}) {
        options[i].clear();if(catalog){if(i==0)options[i]=catalog->sourceCities(body);else if(i==1)options[i]=catalog->companies(selected[0],body,"",true);else if(i==2)options[i]=catalog->cargoes(selected[1],body);else if(i==3&&twoStop)options[i]=catalog->firstStops(selected[2],selected[0],selected[5]);else if(i==3||i==5){options[i]=catalog->destinationCities(selected[2]);if(i==5&&twoStop)options[i].erase(std::remove_if(options[i].begin(),options[i].end(),[&](const ats::Choice& c){return catalog->firstStops(selected[2],selected[0],c.id).empty();}),options[i].end());}else options[i]=catalog->companies(selected[i-1],body,selected[2],false);}
        if(autoMiddle&&(i==3||i==4)&&!selected[5].empty()&&!options[i].empty()&&std::none_of(options[i].begin(),options[i].end(),[&](const ats::Choice& c){return c.id==selected[i];})){
            const auto index=std::uniform_int_distribution<size_t>(0,options[i].size()-1)(randomEngine);
            selected[i]=options[i][index].id;if(i==3)selected[4].clear();
        }
        SendMessageW(combos[i],CB_RESETCONTENT,0,0);int chosen=-1;
        for(const auto& c:options[i]){const auto index=SendMessageW(combos[i],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide(c.label).c_str()));if(c.id==selected[i])chosen=static_cast<int>(index);}
        // Keep the form while disconnected. Once a live trailer is known,
        // incompatible saved selections are cleared in dependency order.
        if(chosen<0&&awaiting&&!selected[i].empty()){
            auto name=dispatch::displayToken(selected[i]);if(catalog){const auto& names=(i==0||i==3||i==5)?catalog->cityNames:catalog->companyNames;if(i==2){if(catalog->cargo.count(selected[i]))name=catalog->cargo.at(selected[i]).label;}else if(names.count(selected[i]))name=names.at(selected[i]);}
            options[i].push_back({selected[i],name});chosen=static_cast<int>(SendMessageW(combos[i],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide(name).c_str())));
        }else if(chosen<0&&!body.empty())selected[i].clear();
        SendMessageW(combos[i],CB_SETCURSEL,static_cast<WPARAM>(chosen),0);
    }syncing=false;
}
void LivePanel::render() {
    const auto t=status.value("trailer",live::Json::object());const auto j=status.value("job",live::Json::object());
    const auto resultState=result.value("state",std::string());
    const bool currentSession=online&&lastSession==status.value("session_id",std::string());
    const bool receiptExpired=result.empty()&&submitted&&live::utcMs()>submitted+125000;
    const auto planState=plan.value("state",std::string());
    const bool planActive=savedPlan&&currentSession&&!plan.empty()&&planState!="complete"&&planState!="stopped"&&planState!="uncertain";
    const bool pending=planActive||(!lastId.empty()&&currentSession&&!receiptExpired&&(resultState.empty()||!live::terminal(resultState)));
    const bool formClean=!lastId.empty()&&formClearedId==lastId;
    const bool awaiting=ats::awaitingAcknowledgement(savedPlan,lastId,lastSession,formClearedId,plan,result);
    const bool review=online&&ats::canConfirmUnrecordedCompletion(savedPlan,lastId,lastSession,formClearedId,plan,result,status,live::utcMs());
    const bool needsAcknowledgement=awaiting||review;
    const bool attached=online&&!profileData.empty()&&t.value("connected",false)&&live::supportedTrailer(t)&&!body.empty();
    for(size_t i=0;i<7;++i)EnableWindow(combos[i],attached&&!pending&&!needsAcknowledgement&&!(twoStop&&randomMiddle&&(i==3||i==4))&&(i==0||!selected[(i==3||i==5)?2:i-1].empty())&&!options[i].empty());
    for(const size_t i:{3u,4u})EnableWindow(labels[i],IsWindowEnabled(combos[i]));
    unsigned percent=50;bool percentageValid=true;try{if(twoStop)percent=remainingPercent();}catch(...){percentageValid=false;}
    EnableWindow(percentEdit,!pending&&!needsAcknowledgement);
    EnableWindow(percentSpin,!pending&&!needsAcknowledgement);
    const bool valid=percentageValid&&catalog&&catalog->valid(route(),body)&&(!twoStop||(catalog->valid(finalRoute(),body)&&catalog->alongRoute(selected[0],selected[3],selected[5])));
    text(dispatchButton,twoStop?"Start two-stop plan":"Dispatch job to ATS");
    EnableWindow(dispatchButton,attached&&pendingCancelId.empty()&&status.value("can_queue",false)&&!pending&&!needsAcknowledgement&&valid);
    EnableWindow(acknowledgeButton,needsAcknowledgement);
    const auto cancelReason=ats::cancelUnavailableReason(online,!profileData.empty(),!pendingCancelId.empty(),status,!lastId.empty()&&lastId!=formClearedId);
    EnableWindow(cancelButton,cancelReason.empty());
    if(cancelTooltip){cancelTooltipText=wide(cancelReason);TOOLINFOW tool={};tool.cbSize=sizeof(tool);tool.hwnd=parent;tool.uId=503;tool.lpszText=cancelTooltipText.empty()?const_cast<wchar_t*>(L""):cancelTooltipText.data();SendMessageW(cancelTooltip,TTM_UPDATETIPTEXTW,0,reinterpret_cast<LPARAM>(&tool));SendMessageW(cancelTooltip,TTM_ACTIVATE,visible&&!cancelReason.empty(),0);}
    if(!catalogError.empty())text(trailerText,catalogError);
    else if(!online)text(trailerText,"Trailer information will appear when ATS is connected.");
    else if(!attached)text(trailerText,t.value("connected",false)?live::trailerSupportMessage(t):"Attach an owned combination of 1-"+std::to_string(live::trailerLimit())+" trailers. Recognised dollies are excluded.");
    else {
        std::string description="Attached: "+std::to_string(t.value("trailer_count",0u))+" trailer(s), "+std::to_string(t.value("dolly_count",0u))+" dolly/dollies | "+dispatch::displayToken(body)+" | "+dispatch::displayToken(t.value("chain_type",std::string()));
        const auto capacity=status.value("trailer_capacity",live::Json::object());
        if(capacity.value("available",false)) {
            live::TrailerCapacity cap;cap.grossKg=capacity.at("gross_limit_kg");cap.chassisKg=capacity.at("chassis_mass_kg");cap.bodyKg=capacity.at("body_mass_kg");cap.volumeM3=capacity.at("volume_m3");cap.axles=capacity.at("axles");
            std::ostringstream volume;volume<<std::fixed<<std::setprecision(2)<<cap.volumeM3;
            description+="\r\nCombination ceiling: "+tonnes(cap.payloadKg())+" payload / "+volume.str()+" m³";
            if(catalog&&catalog->cargo.count(selected[2])){const auto& c=catalog->cargo.at(selected[2]);const auto maximum=cap.maximumUnits(c.unitKg,c.unitM3);if(maximum){description+="\r\nFull-load estimate: "+std::to_string(maximum)+" units / "+tonnes(maximum*c.unitKg);if(twoStop&&percentageValid)try{const auto remain=live::remainingUnits(maximum,percent);description+=" | final "+std::to_string(percent)+"%: "+std::to_string(remain)+" units / "+tonnes(remain*c.unitKg);}catch(...){description+=" | remaining portion needs at least one whole unit.";}}}
        }
        text(trailerText,description);
    }
    text(help,twoStop?(randomMiddle?"Choose Final delivery. A compatible first city and company are picked randomly.\r\nFirst leg: maximum load. Final leg: selected percentage. Shift+F9 opens Dispatch.":"Choose either delivery city first. First delivery follows the route to Final.\r\nFirst leg: maximum load. Final leg: selected percentage. Shift+F9 opens Dispatch."):"Compatible cargo only. Jobs use the maximum whole-unit load.\r\nShift+F9 opens Dispatch Companion from ATS.");
    std::string message;
    if(!sendError.empty())message=sendError;
    else if(awaiting)message=(resultState=="cancelled"?"Job cancelled.":savedPlan?"Both deliveries completed successfully.":"Delivery completed successfully.")+std::string("\r\nAcknowledge completion to clear this form and choose a new job.");
    else if(review)message="ATS has no active job. The previous session has no delivery receipt.\r\nUse Acknowledge completion if you have verified that delivery finished.";
    else if(!online&&gameClosed)message="ATS has closed. Your dispatch details are retained.\r\nAfter reopening ATS, load your profile and bring this app forward to review the delivery.";
    else if(!online)message=offlineReason.empty()?"Start ATS and load your single-player test profile. The live dispatcher must be installed.":offlineReason;
    else if(!lastId.empty()&&!result.empty()&&!formClean&&currentSession)message=result.value("message",std::string())+"\r\nDispatch status: "+resultState;
    else if(!lastId.empty()&&!formClean&&!currentSession&&status.value("resumed_monitoring",false)&&status.value("last_request_id",std::string())==lastId)message="Tracking resumed for your active job after the ATS restart.\r\nDeliver normally; the result will be recorded.";
    else if(!lastId.empty()&&!formClean&&!currentSession)message="Previous-session dispatch retained. Return to Drive to check its status.\r\nAcknowledge completion becomes available when ATS reports no active job.";
    else if(pending)message="Request submitted. Return to the ATS driving view with the truck stopped and trailer attached.";
    else if(!lastId.empty()&&receiptExpired&&currentSession)message="No result was received before the request expired. Check the active job in ATS before sending another dispatch.";
    else if(status.value("disabled",false))message="Live dispatch stopped after an uncertain result. Check the game before restarting the session.";
    else if(j.value("active",false))message="A job is active in ATS. Finish or cancel it before dispatching another.";
    else if(!live::supportedTrailer(t))message=live::trailerSupportMessage(t);
    else message="Ready to choose a compatible ATS job. No save reload is needed.";
    if(pending&&status.value("state",std::string()).rfind("request_waiting_",0)==0)message="Request received. Return to Drive, keep the truck stopped and attach the supported owned combination.";
    if(savedPlan&&currentSession&&!plan.empty()&&!formClean&&!awaiting) {
        if(planState=="waiting_handoff"||planState=="complete"||planState=="stopped"||planState=="uncertain")message=plan.value("message",std::string());
        message+="\r\nTwo-stop plan: "+planState;
    }
    if(!historyError.empty())message+="\r\nHistory: "+historyError;
    if(!cancelMessage.empty())message+="\r\n"+cancelMessage;
    text(statusText,message);
    std::string jobDescription;
    if(online&&j.value("active",false)&&j.contains("route")) {try{
        const auto r=live::parseRoute(j["route"]);
        auto company=[&](const std::string& id){return catalog&&catalog->companyNames.count(id)?catalog->companyNames.at(id):dispatch::displayToken(id);};
        auto city=[&](const std::string& id){return catalog&&catalog->cityNames.count(id)?catalog->cityNames.at(id):dispatch::displayToken(id);};
        const auto cargoName=catalog&&catalog->cargo.count(r.cargo)?catalog->cargo.at(r.cargo).label:dispatch::displayToken(r.cargo);
        jobDescription="GAME JOB   "+cargoName+"\r\nPICKUP   "+company(r.sourceCompany)+" — "+city(r.sourceCity)+"\r\nDELIVERY   "+company(r.targetCompany)+" — "+city(r.targetCity)+"\r\nCargo: "+std::string(j.value("cargo_loaded",false)?"loaded":"awaiting loading");
        if(j.value("payload_known",false))jobDescription+=" | "+std::to_string(j.value("cargo_units",0u))+" units / "+tonnes(j.value("cargo_mass_kg",0.0));
    }catch(...){jobDescription="Game job details are unavailable.";}}
    else if(awaiting){
        auto city=[&](const std::string& id){return catalog&&catalog->cityNames.count(id)?catalog->cityNames.at(id):dispatch::displayToken(id);};
        jobDescription="FINISHED DISPATCH   "+city(selected[0])+" → "+city(selected[3]);if(savedPlan)jobDescription+=" → "+city(selected[5]);
        jobDescription+="\r\nDelivery history is retained. The form waits for your acknowledgement.";
    }
    else if(formClean)jobDescription="Previous dispatch finished. Choose a new route above.";
    else if(!lastId.empty())jobDescription="Last dispatch: "+lastId+(currentSession?std::string():"\r\nThis dispatch belongs to an earlier game session.");
    else if(catalog)jobDescription=std::to_string(catalog->locations.size())+" ATS city records loaded. Each chosen depot is checked in the running economy before assignment.";
    if(savedPlan&&!plan.empty()&&!formClean&&!awaiting)jobDescription+="\r\n"+std::string(planState.rfind("second_",0)==0?"FINAL LEG":"TWO-STOP PLAN")+(currentSession?std::string():" — earlier session; no automatic resume");
    if(savedPlan&&!formClean&&plan.value("initial_units",0u))jobDescription+=" | "+tonnes(plan.value("initial_mass_kg",0.0))+" → "+tonnes(plan.value("remaining_mass_kg",0.0));
    if(historyRows)jobDescription+="\r\nJob history: "+std::to_string(historyRows)+" finished jobs.";
    if(online){const auto profile=currentProfile(status);if(!profile.empty())jobDescription="Welcome, "+profile+"!\r\n"+jobDescription;else jobDescription="Waiting to identify the ATS profile.\r\n"+jobDescription;}
    text(jobText,jobDescription);
}
bool LivePanel::clearCompletedForm() {
    if(profileData.empty()||!online||currentProfile(bridgeStatus(bridge))!=displayedProfile)throw std::runtime_error("ATS profile changed or could not be identified. Wait for it to load.");
    const bool confirmed=ats::awaitingAcknowledgement(savedPlan,lastId,lastSession,formClearedId,plan,result);
    if(!confirmed&&!(online&&ats::canConfirmUnrecordedCompletion(savedPlan,lastId,lastSession,formClearedId,plan,result,status,live::utcMs())))return false;
    if(!confirmed){
        auto notice=live::readJson(profileData/dispatchGame::savedDispatch);
        if(notice.value("last_request_id",std::string())!=lastId||notice.value("session_id",std::string())!=lastSession)throw std::runtime_error("Saved dispatch changed. Reopen the app before acknowledging.");
        if(notice.value("completion_review_prompted_session",std::string())!=status.at("session_id")){notice["completion_review_prompted_session"]=status.at("session_id");live::writeJson(profileData/dispatchGame::savedDispatch,notice);}reviewedSession=status.at("session_id");
        if(MessageBoxW(parent,L"A new the game session was detected after a restart or reload. the game now reports no active job, but the previous delivery receipt is missing.\n\nHave you checked the game and confirmed that this delivery is complete?\n\nYes clears the form and records your confirmation. No keeps the job details for review. This does not change any game job.",L"Confirm delivery after the game restart",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES)return false;
    }
    const auto saved=live::readJson(profileData/dispatchGame::savedDispatch);
    if(saved.value("last_request_id",std::string())!=lastId||saved.value("session_id",std::string())!=lastSession)throw std::runtime_error("Saved dispatch changed. Reopen the app before acknowledging.");
    const auto updated=confirmed?ats::acknowledgeCompletedForm(saved,plan,result):ats::acknowledgeUnrecordedCompletion(saved,plan,result,bridgeStatus(bridge),live::utcMs());
    if(!confirmed)ats::retainUserConfirmedDelivery(profileData,bridge,updated);
    live::writeJson(profileData/dispatchGame::savedDispatch,updated);
    selected.fill({});formClearedId=lastId;sendError.clear();lastHistorySync=0;syncHistory();choices();return true;
}
void LivePanel::syncHistory() {
    if(profileData.empty())return;
    const auto phase=result.value("state",std::string());const auto receipt=(phase=="delivered"||phase=="cancelled")?result.value("request_id",std::string())+":"+std::to_string(result.value("updated_utc_ms",uint64_t(0))):std::string();
    const auto now=live::utcMs();if(lastHistorySync&&now<lastHistorySync+30000&&(receipt.empty()||receipt==historyReceipt))return;
    lastHistorySync=now;historyReceipt=receipt;
    try{historyRows=ats::exportJobHistory(profileData,bridge,catalog.get(),true).rows;historyError.clear();}catch(const std::exception& ex){historyError=ex.what();}
}
void LivePanel::requestCancel() {
    if(profileData.empty()||!pendingCancelId.empty())throw std::runtime_error("No profile is ready for cancellation.");
    const auto confirmedProfile=displayedProfile;const auto confirmedDir=profileData;
    auto fresh=bridgeStatus(bridge);const auto menu=fresh.at("menu_snapshot");
    if(!fresh.value("experimental_cancel_available",false)||fresh.at("driving").get<bool>()||!menu.at("stopped").get<bool>()||!menu.at("ignition_on").is_boolean()||currentProfile(fresh)!=displayedProfile)throw std::runtime_error("Stop the truck and open the ATS menu before cancelling.");
    if(MessageBoxW(parent,L"This action will cancel any active job in the game. It will also stop any remaining stops. Completed deliveries remain in history. The game may charge a cancellation penalty. Cancellation is experimental; use a disposable test profile.\n\nContinue?",L"Cancel current job",MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)!=IDYES)return;
    const auto checked=bridgeStatus(bridge);const auto checkedMenu=checked.at("menu_snapshot");
    if(displayedProfile!=confirmedProfile||profileData!=confirmedDir||!checked.value("experimental_cancel_available",false)||checked.at("session_id")!=fresh.at("session_id")||checked.at("context_id")!=fresh.at("context_id")||checked.at("cancel_target_token")!=fresh.at("cancel_target_token")||checked.at("driving").get<bool>()||!checkedMenu.at("stopped").get<bool>()||!checkedMenu.at("ignition_on").is_boolean()||currentProfile(checked)!=confirmedProfile)throw std::runtime_error("ATS state changed. Open the menu and try again.");
    live::CancelRequest request;request.id=live::uniqueId();request.session=checked.at("session_id");request.context=checked.at("context_id");request.target=checked.at("cancel_target_token");request.profileId=ats::profileFolderId(confirmedProfile);request.created=live::utcMs();request.expires=request.created+30000;
    live::parseCancelRequest(live::cancelRequestJson(request));
    const auto pending=live::Json{{"request_id",request.id},{"session_id",request.session},{"dispatch_id",lastId},{"dispatch_session_id",lastSession},{"created_utc_ms",request.created}};
    const auto pendingFile=profileData/"cancel_pending.json";live::writeJson(pendingFile,pending,false);
    try{live::writeJson(bridge/"cancel_request.json",live::cancelRequestJson(request),false);}catch(...){DeleteFileW(pendingFile.c_str());throw;}
    pendingCancelId=request.id;sendError.clear();cancelMessage="Cancellation requested; waiting for ATS.";
}
void LivePanel::pollCancelResult() {
    if(profileData.empty()||pendingCancelId.empty())return;
    const auto resultFile=bridge/"cancel_results"/(pendingCancelId+".json");const auto pending=live::readJson(profileData/"cancel_pending.json");
    live::Json reply;
    if(std::filesystem::exists(resultFile))reply=live::readJson(resultFile);
    else {
        const auto created=pending.at("created_utc_ms").get<uint64_t>(),now=live::utcMs();if(now<=created||now-created<=35000)return;
        // A timeout cannot prove what the game did. Stop only a still-queued
        // cancellation request bearing this exact ID; never remove others.
        const auto queued=bridge/"cancel_request.json";
        if(std::filesystem::exists(queued))try{const auto request=live::parseCancelRequest(live::readJson(queued));if(request.id==pendingCancelId){const auto archive=bridge/"duplicates"/(live::uniqueId()+".timed-out-cancel.json");MoveFileExW(queued.c_str(),archive.c_str(),MOVEFILE_WRITE_THROUGH);}}catch(...){}
        reply={{"protocol",live::protocolVersion},{"game",dispatchGame::id},{"request_id",pendingCancelId},{"session_id",pending.at("session_id")},{"outcome","contact_lost"}};
    }
    if(reply.at("protocol")!=live::protocolVersion||reply.at("game")!=dispatchGame::id||reply.at("request_id")!=pendingCancelId||reply.at("session_id")!=pending.at("session_id"))throw std::runtime_error("Invalid cancellation response.");
    const auto outcome=reply.at("outcome").get<std::string>();
    if(outcome=="rejected_state"){cancelMessage="Cancellation was not attempted: ATS state changed. Open the menu and retry.";}
    else {
        if(outcome!="game_confirmed"&&outcome!="game_requested"&&outcome!="no_game_job"&&outcome!="game_unavailable"&&outcome!="game_unconfirmed"&&outcome!="stop_failed"&&outcome!="contact_lost")throw std::runtime_error("Unknown cancellation response.");
        const auto savedId=pending.value("dispatch_id",std::string());
        if(!savedId.empty()) {
            const auto saved=live::readJson(profileData/dispatchGame::savedDispatch);
            live::writeJson(profileData/dispatchGame::savedDispatch,live::applyCancelCleanup(saved,pending,reply,live::utcMs()));
            selected.fill({});formClearedId=savedId;
        }
        cancelMessage=outcome=="game_confirmed"?"Dispatch stopped; ATS confirmed cancellation.":outcome=="no_game_job"?"Dispatch stopped; ATS reported no active job.":outcome=="stop_failed"?"App data cleared; dispatch was disabled. Game cancellation is not confirmed.":outcome=="contact_lost"?"App data cleared; game cancellation and remaining stops could not be confirmed.":"Dispatch stopped; game cancellation is not confirmed.";
        if(outcome=="game_requested"||outcome=="contact_lost")followCancelId=pendingCancelId;
        choices();
    }
    if(!DeleteFileW((profileData/"cancel_pending.json").c_str())&&GetLastError()!=ERROR_FILE_NOT_FOUND)throw std::runtime_error("Cannot clear pending cancellation marker.");pendingCancelId.clear();
}
bool LivePanel::defaultPickupCity() {
    if(!online||!catalog||!selected[0].empty()||body.empty()||shouldWarnOnClose()||ats::awaitingAcknowledgement(savedPlan,lastId,lastSession,formClearedId,plan,result))return false;
    const auto position=status.value("truck_position",live::Json());
    if(!position.is_object()||!position.contains("x")||!position["x"].is_number()||!position.contains("z")||!position["z"].is_number())return false;
    const auto city=catalog->nearbyCity(position["x"].get<double>(),position["z"].get<double>());
    if(city.empty()||catalog->companies(city,body,"",true).empty())return false;
    selected[0]=city;return true;
}
void LivePanel::tick() {
    const auto oldBody=body,oldSignature=signature;bool wasOnline=online;
    try{status=bridgeStatus(bridge);online=true;offlineReason.clear();const auto t=status.at("trailer");body=t.value("body_type",std::string());signature=t.value("signature",std::string());}catch(const std::exception& ex){online=false;offlineReason=ex.what();status=live::Json::object();body.clear();signature.clear();}
    if(online){uint64_t created=0;if(runningGame(status,&created)&&created){observedGame=status;observedGameCreated=created;gameClosed=false;connectedDuringRun=true;}}
    else if(observedGameExited(observedGame,observedGameCreated))gameClosed=true;
    const auto profile=online?currentProfile(status):std::string();const bool profileChanged=profile!=displayedProfile;
    if(profileChanged){loadProfile(profile);lastHistorySync=0;historyRows=0;historyReceipt.clear();historyError.clear();displayedProfile=profile;const auto caption=wide(profile.empty()?"Dispatch Companion":profile+" Dispatch");SendMessageW(parent,ProfileTitleMessage,reinterpret_cast<WPARAM>(&profile),reinterpret_cast<LPARAM>(caption.c_str()));}
    std::string activeId=lastId;
    if(savedPlan&&!lastId.empty())try {
        plan=live::readJson(bridge/"plans"/(lastId+".json"));
        if(plan.at("plan_id")!=lastId||plan.at("session_id")!=lastSession||!plan.at("state").is_string()||!plan.at("message").is_string()||!live::requestId(plan.at("active_request_id").get<std::string>()))throw std::runtime_error("Invalid plan result");
        activeId=plan.at("active_request_id").get<std::string>();
    }catch(...){plan=live::Json::object();}
    if(!activeId.empty())try{result=live::readJson(bridge/"results"/(activeId+".json"));if(result.value("request_id",std::string())!=activeId||result.value("session_id",std::string())!=lastSession||!result.contains("state")||!result["state"].is_string()||!result.contains("message")||!result["message"].is_string())result=live::Json::object();}catch(...){result=live::Json::object();}
    try{pollCancelResult();}catch(const std::exception& ex){sendError=ex.what();}
    if(!profileData.empty()&&!followCancelId.empty())try {
        const auto reply=live::readJson(bridge/"cancel_results"/(followCancelId+".json"));const auto outcome=reply.at("outcome").get<std::string>();
        if(reply.at("request_id")!=followCancelId||reply.at("game")!=dispatchGame::id)throw std::runtime_error("Invalid cancellation follow-up.");
        if(outcome=="game_confirmed"||outcome=="game_unconfirmed") {
            const auto savedFile=profileData/dispatchGame::savedDispatch;if(std::filesystem::exists(savedFile)){auto saved=live::readJson(savedFile);if(saved.contains("cancel_cleanup")&&saved["cancel_cleanup"].value("request_id",std::string())==followCancelId){saved["cancel_cleanup"]["outcome"]=outcome;live::writeJson(savedFile,saved);}}
            cancelMessage=outcome=="game_confirmed"?"Dispatch stopped; ATS confirmed cancellation.":"Dispatch stopped; ATS did not confirm cancellation.";followCancelId.clear();
        }
    }catch(const std::exception&){}
    if(online&&!savedPlan&&!profileData.empty()&&formClearedId!=lastId&&status.value("build_id",std::string())==dispatchGame::build&&!status.value("resumed_monitoring",false)&&live::utcMs()>lastResumeAttempt+5000&&ats::restartJobCandidate(result,status,lastId,lastSession,live::utcMs()))try {
        const auto ownership=live::readJson(profileData/"owned_requests"/(lastId+".json"));
        if(ownership.at("request_id")!=lastId||ownership.at("session_id")!=lastSession||currentProfile(status)!=displayedProfile)throw std::runtime_error("Cannot verify this profile's saved dispatch.");
        const auto& trailer=status.at("trailer");if(!trailer.at("connected").get<bool>()||!live::supportedTrailer(trailer)||trailer.at("signature").get<std::string>().empty())throw std::runtime_error("Waiting for the supported attached trailer combination before resuming tracking.");
        const live::Json request={{"protocol",live::protocolVersion},{"request_id",lastId},{"original_session_id",lastSession},{"target_session_id",status.at("session_id")},{"context_id",status.at("context_id")},{"trailer_signature",trailer.at("signature")},{"created_utc_ms",live::utcMs()}};
        lastResumeAttempt=live::utcMs();live::writeJson(bridge/"resume.json",request,false);
    }catch(const std::exception&){lastResumeAttempt=live::utcMs();}
    syncHistory();
    if(online&&!profileData.empty()&&ats::verifiedDeliveryReady(savedPlan,lastId,lastSession,formClearedId,plan,result))try {
        if(!historyError.empty())throw std::runtime_error("Delivery is confirmed, but history could not be saved: "+historyError);
        const auto entries=ats::readHistoryEntries(profileData,catalog.get());const auto deliveredId=result.at("request_id").get<std::string>();
        const auto stored=std::find_if(entries.begin(),entries.end(),[&](const ats::HistoryEntry& entry){return entry.id==deliveredId&&entry.session==lastSession&&!entry.driverConfirmed&&entry.outcome=="Delivered";});
        if(stored==entries.end())throw std::runtime_error("Delivery is confirmed, but its profile history entry has not been saved yet.");
        clearCompletedForm();
    }catch(const std::exception& ex){sendError=ex.what();}
    const bool pickupDefaulted=defaultPickupCity();
    if(pickupDefaulted||profileChanged||oldBody!=body||oldSignature!=signature||wasOnline!=online||options[0].empty())choices();
    if(visible)render();
    if(visible&&online&&!reviewPromptPosted&&GetForegroundWindow()==parent&&!IsIconic(parent)&&reviewedSession!=status.value("session_id",std::string())&&ats::canConfirmUnrecordedCompletion(savedPlan,lastId,lastSession,formClearedId,plan,result,status,live::utcMs()))try {
        const auto saved=live::readJson(profileData/dispatchGame::savedDispatch);
        if(ats::restartReviewDue(saved,plan,result,status,live::utcMs()))reviewPromptPosted=PostMessageW(parent,RestartReviewMessage,0,0)!=FALSE;
    }catch(const std::exception& ex){sendError=ex.what();}
}
void LivePanel::reviewRestart() {
    if(!reviewPromptPosted)return;
    try {
        if(visible&&GetForegroundWindow()==parent&&!IsIconic(parent)){
            const auto fresh=bridgeStatus(bridge);auto saved=live::readJson(profileData/dispatchGame::savedDispatch);
            if(saved.value("last_request_id",std::string())==lastId&&saved.value("session_id",std::string())==lastSession&&ats::restartReviewDue(saved,plan,result,fresh,live::utcMs())){
                // Save notification state before opening the modal dialog: timer
                // callbacks and reopening the app must not repeat the question.
                saved["completion_review_prompted_session"]=fresh.at("session_id");live::writeJson(profileData/dispatchGame::savedDispatch,saved);reviewedSession=fresh.at("session_id");status=fresh;online=true;
                clearCompletedForm();
            }
        }
    }catch(const std::exception& ex){sendError=ex.what();}
    reviewPromptPosted=false;if(visible)render();
}
void LivePanel::send() {
    if(!pendingCancelId.empty())throw std::runtime_error("Wait for the cancellation request to finish.");
    if(profileData.empty()||currentProfile(bridgeStatus(bridge))!=displayedProfile)throw std::runtime_error("ATS profile changed or could not be identified. Wait for it to load.");
    if(ats::awaitingAcknowledgement(savedPlan,lastId,lastSession,formClearedId,plan,result)||ats::canConfirmUnrecordedCompletion(savedPlan,lastId,lastSession,formClearedId,plan,result,status,live::utcMs()))throw std::runtime_error("Acknowledge the previous delivery before starting another dispatch.");
    if(!catalog)throw std::runtime_error(catalogError);
    const auto fresh=bridgeStatus(bridge);const auto t=fresh.at("trailer");const auto currentBody=t.value("body_type",std::string());
    if(!fresh.value("can_queue",false)||!t.value("connected",false)||!live::supportedTrailer(t))throw std::runtime_error("ATS is not ready for another dispatch. Check the trailer and active job.");
    if(!catalog->valid(route(),currentBody))throw std::runtime_error("The selected job is incompatible with the current trailer or company data.");
    if(twoStop&&(!catalog->valid(finalRoute(),currentBody)||!catalog->alongRoute(selected[0],selected[3],selected[5])))throw std::runtime_error("Choose a first delivery along the route to the final city, with both depots accepting this cargo.");
    live::Request r;r.id=live::uniqueId();r.session=fresh.at("session_id");r.context=fresh.at("context_id");r.trailer=t.at("signature");r.created=live::utcMs();r.expires=r.created+120000;r.route=route();
    r.twoStop=twoStop;if(twoStop){r.finalRoute=finalRoute();r.secondId=live::uniqueId();r.remainingPercent=remainingPercent();}
    // Persist the identifier first, so reopening the app can recover its result.
    live::Json saved={{"route",live::routeJson(r.route)},{"last_request_id",r.id},{"session_id",r.session},{"submitted_utc_ms",r.created},{"two_stop",twoStop}};
    if(twoStop){saved["final_route"]=live::routeJson(r.finalRoute);saved["second_request_id"]=r.secondId;}
    saved["remaining_percent"]=r.remainingPercent;
    uint64_t gameCreated=0;if(!runningGame(fresh,&gameCreated)||!gameCreated)throw std::runtime_error("ATS process changed before dispatch. Please retry after it reconnects.");saved["game_process_id"]=fresh.at("pid");saved["game_process_created_utc_ms"]=gameCreated;
    const auto ownershipDir=profileData/"owned_requests";std::filesystem::create_directories(ownershipDir);
    for(const auto& id:{r.id,r.secondId})if(!id.empty())live::writeJson(ownershipDir/(id+".json"),live::Json{{"request_id",id},{"session_id",r.session}},false);
    live::writeJson(profileData/dispatchGame::savedDispatch,saved);
    lastId=r.id;lastSession=r.session;submitted=r.created;savedPlan=twoStop;formClearedId.clear();reviewedSession.clear();result=live::Json::object();plan=live::Json::object();
    try{live::writeJson(bridge/"request.json",live::requestJson(r),false);sendError.clear();}catch(...){lastId.clear();throw;}
    tick();
}
bool LivePanel::command(int id,int notification) {
    if(id==503&&notification==BN_CLICKED){try{requestCancel();}catch(const std::exception& e){sendError=e.what();}render();return true;}
    if(id==502&&notification==BN_CLICKED){if(reviewPromptPosted)return true;reviewPromptPosted=true;try{clearCompletedForm();}catch(const std::exception& e){sendError=e.what();}reviewPromptPosted=false;render();return true;}
    if(id==407&&notification==EN_CHANGE&&!syncing&&percentEdit&&statusText){sendError.clear();render();return true;}
    if(id>=400&&id<407&&notification==CBN_SELCHANGE&&!syncing){
        const auto i=static_cast<size_t>(id-400);if(twoStop&&randomMiddle&&(i==3||i==4))return true;
        const auto n=SendMessageW(combos[i],CB_GETCURSEL,0,0);selected[i]=n>=0&&static_cast<size_t>(n)<options[i].size()?options[i][static_cast<size_t>(n)].id:std::string();
        if(i<3)for(size_t k=i+1;k<7;++k)selected[k].clear();
        else if(i==3||i==5)selected[i+1].clear();
        if(twoStop&&randomMiddle&&i==5){selected[3].clear();selected[4].clear();}
        sendError.clear();choices();
        if(twoStop&&i==5&&selected[3].empty()&&!options[3].empty()){
            selected[3]=options[3].front().id;selected[4].clear();choices();
            if(!options[4].empty()){selected[4]=options[4].front().id;choices();}
        }
        render();return true;
    }
    if(id==DispatchId&&notification==BN_CLICKED){try{send();}catch(const std::exception& e){sendError=e.what();render();}return true;}return false;
}
