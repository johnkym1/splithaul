#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include "dispatch_core.h"
#include "live_panel.h"
#include "branding.h"
#include "app_options.h"
#include "app_version.h"
#include "app_links.h"
#include "installed_storage.h"
#include "dispatch_layout.h"
#include "app_theme.h"
#include "history_page.h"
#include "profile_files.h"
#include "profile_controls.h"
#include "update_controls.h"
#include "win_text.h"
#include "../dispatch_shared/app_launcher.h"
#include <algorithm>
#include <memory>
#include <stdexcept>

using namespace dispatch;
static std::unique_ptr<Model> model;
static State route;
static HWND window=nullptr,title=nullptr,subtitle=nullptr,hint=nullptr,statusText=nullptr,details=nullptr;
static HWND saveButton=nullptr,newButton=nullptr,refreshButton=nullptr,firstButton=nullptr,finalButton=nullptr;
static std::array<HWND,FieldCount> labels{},combos{};
static HFONT normalFont=nullptr,titleFont=nullptr,sectionFont=nullptr;
static bool syncing=false;
static HINSTANCE instance=nullptr;
static std::unique_ptr<LivePanel> livePanel;
static HWND modeCombo=nullptr,randomCheck=nullptr,fullCapacityCheck=nullptr;
// Visual order is independent of persisted mode IDs from earlier releases.
static void selectModeOption(int mode) {
    const auto count=SendMessageW(modeCombo,CB_GETCOUNT,0,0);
    for(LRESULT row=0;row<count;++row)if(SendMessageW(modeCombo,CB_GETITEMDATA,static_cast<WPARAM>(row),0)==mode){SendMessageW(modeCombo,CB_SETCURSEL,static_cast<WPARAM>(row),0);return;}
}

static bool liveMode=true;
static std::unique_ptr<Branding> branding;
static HWND logo=nullptr;
static bool customDataRoot=false;
static AppOptions appOptions;
static RunningGames lastRunningGames=RunningGames::none;
static RunningGames detectRunningGames() {
    const HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot==INVALID_HANDLE_VALUE)return RunningGames::none;
    PROCESSENTRY32W entry={};entry.dwSize=sizeof(entry);
    bool ats=false,ets2=false;
    if(Process32FirstW(snapshot,&entry))do {
        if(_wcsicmp(entry.szExeFile,L"amtrucks.exe")==0)ats=true;
        else if(_wcsicmp(entry.szExeFile,L"eurotrucks2.exe")==0)ets2=true;
    }while(Process32NextW(snapshot,&entry));
    CloseHandle(snapshot);
    return ats&&ets2?RunningGames::both:ats?RunningGames::ats:ets2?RunningGames::ets2:RunningGames::none;
}
static fs::path profileSettings;
static void saveCurrentOptions(const AppOptions& options) {
    if(profileSettings.empty()){saveOptions(model->root,options);return;}
    auto data=live::readJson(profileSettings);data["mode"]=options.mode;data["dark_theme"]=options.dark;data["random"]=options.random;data["full_trailer_loads"]=options.fullCapacity;
    decodeOptions(data);live::writeJson(profileSettings,data);
}
static AppTheme theme;
static bool optionsPage=false,historyVisible=false;
static std::unique_ptr<HistoryPage> historyPage;
static HWND historyButton=nullptr;
static State etsMultiple,etsSingle;
static HWND optionsButton=nullptr,modeLabel=nullptr,themeButton=nullptr,optionsHelp=nullptr;
static HWND aboutHeading=nullptr,aboutText=nullptr,themeHeading=nullptr;
static HWND donateButton=nullptr,donateHelp=nullptr;
static std::unique_ptr<ProfileControls> profileControls;
static std::unique_ptr<UpdateControls> updateControls;
static std::array<HWND,5> dispatchHeadings{};
constexpr int SaveId=200,NewId=201,RefreshId=202,FirstId=203,FinalId=204;
static void mode() {
    const bool planner=!liveMode&&!optionsPage&&!historyVisible,multiple=appOptions.mode==3;
    SetWindowTextW(labels[FinalCity],multiple?L"Final delivery city":L"Delivery city");SetWindowTextW(labels[FinalCompany],multiple?L"Final delivery company":L"Delivery company");
    for(size_t i=0;i<FieldCount;++i){ShowWindow(labels[i],planner&&(i<5||multiple)?SW_SHOW:SW_HIDE);ShowWindow(combos[i],planner&&(i<5||multiple)?SW_SHOW:SW_HIDE);}
    for(auto h:{hint,statusText,details,saveButton,newButton,refreshButton,finalButton})ShowWindow(h,planner?SW_SHOW:SW_HIDE);
    ShowWindow(firstButton,planner&&multiple?SW_SHOW:SW_HIDE);
    for(auto h:{modeCombo,modeLabel,themeButton,themeHeading,optionsHelp,aboutHeading,aboutText,donateButton,donateHelp})ShowWindow(h,optionsPage?SW_SHOW:SW_HIDE);
    SendMessageW(randomCheck,BM_SETCHECK,appOptions.random?BST_CHECKED:BST_UNCHECKED,0);
    ShowWindow(randomCheck,optionsPage&&(appOptions.mode==1||appOptions.mode==3)?SW_SHOW:SW_HIDE);
    SendMessageW(fullCapacityCheck,BM_SETCHECK,appOptions.fullCapacity?BST_CHECKED:BST_UNCHECKED,0);
    ShowWindow(fullCapacityCheck,optionsPage?SW_SHOW:SW_HIDE);
    if(profileControls)profileControls->show(optionsPage);
    if(updateControls)updateControls->show(optionsPage);
    SetWindowTextW(optionsButton,optionsPage?L"Dispatch":L"Options");SetWindowTextW(historyButton,historyVisible?L"Dispatch":L"History");const auto sub=L"by grumpyman · "+std::wstring(historyVisible?L"History":optionsPage?L"Options":modeLabels[static_cast<size_t>(appOptions.mode)]);SetWindowTextW(subtitle,sub.c_str());
    if(livePanel){livePanel->setRandom(appOptions.random);livePanel->setFullCapacity(appOptions.fullCapacity);livePanel->show(liveMode&&!optionsPage&&!historyVisible);}
    if(historyPage)historyPage->show(historyVisible);
    for(auto h:dispatchHeadings)ShowWindow(h,!optionsPage&&!historyVisible?SW_SHOW:SW_HIDE);
    SetWindowTextW(dispatchHeadings[1],liveMode?L"Trailer and loading":L"Route guidance");
    SetWindowTextW(dispatchHeadings[4],liveMode?L"Job details":L"Plan details");
    InvalidateRect(window,nullptr,TRUE);
}

static std::wstring wide(const std::string& input) {
    return winText::toWide(dispatchGame::displayText(input));
}
static std::string utf8(const std::wstring& text) {
    return winText::toUtf8(text);
}
static void text(HWND control,const std::string& value) {SetWindowTextW(control,wide(value).c_str());}
static std::string selection(HWND combo) {
    const auto i=SendMessageW(combo,CB_GETCURSEL,0,0);if(i==CB_ERR)return {};
    const auto length=SendMessageW(combo,CB_GETLBTEXTLEN,static_cast<WPARAM>(i),0);
    if(length==CB_ERR)return {};
    std::wstring value(static_cast<size_t>(length)+1,L'\0');SendMessageW(combo,CB_GETLBTEXT,static_cast<WPARAM>(i),reinterpret_cast<LPARAM>(value.data()));value.resize(static_cast<size_t>(length));return utf8(value);
}
static void list(HWND combo,const std::vector<std::string>& choices,const std::string& value) {
    SendMessageW(combo,CB_RESETCONTENT,0,0);int selected=-1;
    for(const auto& item:choices) {
        const auto index=SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide(item).c_str()));
        if(item==value)selected=static_cast<int>(index);
    }
    // Keep saved selections visible, even when a later game catalog differs.
    // They cannot be used for a new plan without passing current validation.
    if(selected<0&&!value.empty())selected=static_cast<int>(SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide(value).c_str())));
    SendMessageW(combo,CB_SETCURSEL,static_cast<WPARAM>(selected),0);
}
static void render() {
    syncing=true;
    const bool setup=route.stage=="setup";
    for(size_t i=0;i<FieldCount;++i) {
        const auto choices=model->choices(static_cast<Field>(i),route);
        list(combos[i],choices,route.values[i]);
        const bool prerequisite=i==0||!route.values[i-1].empty();
        EnableWindow(combos[i],setup&&prerequisite&&!choices.empty());
    }
    EnableWindow(saveButton,setup);EnableWindow(firstButton,route.stage=="to_d2");EnableWindow(finalButton,route.stage=="to_d1");
    text(saveButton,appOptions.mode==2?"Save single job plan":"Save and start plan");
    if(setup) {
        if(route.values[PickupCompany].empty())text(hint,"Choose a pickup company to see the cargo it offers.");
        else {
            const auto& out=model->cargoes(route.values[PickupCompany]);
            if(out.empty())text(hint,"No outgoing cargo was found for "+route.values[PickupCompany]+" in the extracted game data.");
            else if(!route.values[FinalCompany].empty()) {
                size_t count=0;const auto& in=model->cargoes(route.values[FinalCompany],"in");for(const auto& cargo:out)if(in.count(cargo))++count;
                text(hint,std::to_string(count)+" cargo types from this pickup are accepted by the final company.");
            }else text(hint,std::to_string(out.size())+" cargo types offered by "+route.values[PickupCompany]+".");
        }
    }else text(hint,"Saved route selections are locked. Choose New route to plan another run.");
    if(appOptions.mode==2)text(statusText,"ETS single is a route planner. Live ETS game assignment is not connected. Save prepares a pending job request only.");
    else if(setup)text(statusText,"Set up your route. Suggested first-drop cities follow an approximate corridor between pickup and final delivery.");
    else if(route.stage=="to_d2")text(statusText,"First leg prepared in the planner. Live game assignment is not connected to this app yet.");
    else if(route.stage=="to_d1")text(statusText,"First delivery marked complete. Continue to the final delivery (D1).");
    else text(statusText,"Both stops marked complete.");
    if(appOptions.mode==2)text(details,setup?"Select pickup → cargo → delivery.":"PLANNED DELIVERY   "+route.values[StartCity]+" → "+route.values[FinalCity]+" | "+route.values[Cargo]);
    else if(setup)text(details,"Select pickup → cargo → final delivery → first trailer drop.");
    else text(details,"PICKUP   "+route.values[StartCity]+" — "+route.values[PickupCompany]+" | "+route.values[Cargo]+"\r\nFIRST DROP   "+route.values[MiddleCity]+" — "+route.values[MiddleCompany]+"\r\nFINAL DELIVERY   "+route.values[FinalCity]+" — "+route.values[FinalCompany]);
    syncing=false;
}
static int scale(int value) {return MulDiv(value,static_cast<int>(GetDpiForWindow(window)),96);}
static void position(HWND control,int x,int y,int width,int height) {MoveWindow(control,scale(x),scale(y),scale(width),scale(height),TRUE);}
static int modeComboWidth() {
    const auto dc=GetDC(modeCombo);if(!dc)return 150;
    const auto previous=SelectObject(dc,normalFont);int longest=0;
    for(const auto caption:modeLabels){SIZE size={};GetTextExtentPoint32W(dc,caption,lstrlenW(caption),&size);longest=(std::max)(longest,static_cast<int>(size.cx));}
    SelectObject(dc,previous);ReleaseDC(modeCombo,dc);
    // Allow for the themed text insets and dropdown arrow, at the current DPI.
    return MulDiv(longest,96,static_cast<int>(GetDpiForWindow(window)))+48;
}
static void layout() {
    if(!title)return;RECT client;GetClientRect(window,&client);const auto dpi=GetDpiForWindow(window);const int width=MulDiv(client.right,96,static_cast<int>(dpi)),height=MulDiv(client.bottom,96,static_cast<int>(dpi));
    position(logo,24,12,52,52);position(title,90,14,width-362,34);position(subtitle,90,50,width-362,22);position(historyButton,width-248,20,100,34);position(optionsButton,width-136,20,100,34);
    const int modeWidth=modeComboWidth();
    position(modeLabel,44,112,(std::max)(330,width/2)-76,26);position(modeCombo,44,152,modeWidth,220);
    position(randomCheck,44+modeWidth+20,152,112,28);
    position(fullCapacityCheck,44,184,(std::max)(330,width/2)-76,28);
    if(profileControls)profileControls->layout(width,dpi);
    position(themeHeading,44,252,(std::max)(330,width/2)-76,26);position(themeButton,44,292,220,36);
    if(updateControls)updateControls->layout(width,dpi);
    position(aboutHeading,44,392,width-88,28);position(aboutText,44,432,width-88,150);
    position(donateButton,44,594,172,36);position(donateHelp,232,594,width-276,44);
    position(optionsHelp,44,662,width-88,138);
    const DispatchLayout page(liveMode,multipleMode(appOptions.mode),height,appOptions.mode>=4);
    const int column=(width-64)/2;
    position(dispatchHeadings[0],44,112,width-88,26);position(dispatchHeadings[1],44,page.guidanceTop+12,width-88,26);
    position(dispatchHeadings[2],44,page.actionsTop+10,width-88,26);position(dispatchHeadings[3],44,page.statusTop+12,column-40,26);position(dispatchHeadings[4],60+column,page.statusTop+12,column-40,26);
    for(size_t i=0;i<FieldCount;++i){const int y=DispatchLayout::fieldTop(static_cast<int>(i));position(labels[i],44,y+4,216,26);position(combos[i],266,y,(std::min)(420,width-310),340);}
    position(hint,44,page.guidanceTop+38,width-88,42);
    const int buttonWidth=(width-112)/3;
    position(saveButton,44,page.actionsTop+38,buttonWidth,36);position(newButton,56+buttonWidth,page.actionsTop+38,buttonWidth,36);position(refreshButton,68+buttonWidth*2,page.actionsTop+38,buttonWidth,36);
    position(statusText,44,page.statusTop+40,column-40,page.statusHeight-56);position(details,60+column,page.statusTop+40,column-40,page.statusHeight-56);
    const int half=(width-100)/2;position(firstButton,44,page.actionsTop+86,half,36);position(finalButton,56+half,page.actionsTop+86,half,36);
    if(livePanel)livePanel->layout(width,height,GetDpiForWindow(window));
    if(historyPage)historyPage->layout(width,height,dpi);
    InvalidateRect(window,nullptr,TRUE);
}
// Draw section outlines on the page itself so the native controls retain their
// focus, tab order and theme handling without overlapping group-box windows.
static void paintPageBoxes(HDC dc,int width,int height) {
    if(historyVisible)return;
    const auto pen=CreatePen(PS_SOLID,scale(1),theme.border);
    const auto oldPen=SelectObject(dc,pen),oldBrush=SelectObject(dc,GetStockObject(NULL_BRUSH));
    auto box=[&](int x,int y,int w,int h){RoundRect(dc,scale(x),scale(y),scale(x+w),scale(y+h),scale(12),scale(12));};
    box(width-260,8,236,58);
    if(optionsPage){
        box(24,96,width-48,124);box(24,236,width-48,136);box(24,376,width-48,440);
        const int divider=(std::max)(330,width/2)-20;
        MoveToEx(dc,scale(divider),scale(116),nullptr);LineTo(dc,scale(divider),scale(200));
        MoveToEx(dc,scale(divider),scale(256),nullptr);LineTo(dc,scale(divider),scale(340));
        MoveToEx(dc,scale(44),scale(648),nullptr);LineTo(dc,scale(width-44),scale(648));
    }else{
        const DispatchLayout page(liveMode,multipleMode(appOptions.mode),height,appOptions.mode>=4);
        box(24,96,width-48,page.routeBottom-96);box(24,page.guidanceTop,width-48,page.guidanceHeight);box(24,page.actionsTop,width-48,page.actionsHeight);
        for(const int row:{0,3,5}){
            if(row==5&&!multipleMode(appOptions.mode))continue;
            const int y=DispatchLayout::fieldTop(row)-10;
            MoveToEx(dc,scale(44),scale(y),nullptr);LineTo(dc,scale(width-44),scale(y));
        }
        const int column=(width-64)/2;box(24,page.statusTop,column,page.statusHeight);box(40+column,page.statusTop,column,page.statusHeight);
    }
    SelectObject(dc,oldBrush);SelectObject(dc,oldPen);DeleteObject(pen);
}
static HWND control(const wchar_t* kind,const wchar_t* value,DWORD style,int id) {
    HWND h=CreateWindowExW(0,kind,value,WS_CHILD|WS_VISIBLE|style,0,0,0,0,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance,nullptr);
    if(!h)throw std::runtime_error("Could not create a Windows control.");return h;
}
static void fonts() {
    const int dpi=static_cast<int>(GetDpiForWindow(window));
    auto next=CreateFontW(-MulDiv(11,dpi,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    auto nextTitle=CreateFontW(-MulDiv(21,dpi,72),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    auto nextSection=CreateFontW(-MulDiv(11,dpi,72),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    for(HWND child=GetWindow(window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(next),TRUE);
    SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(nextTitle),TRUE);
    for(auto h:{modeLabel,themeHeading,aboutHeading})SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(nextSection),TRUE);
    for(auto h:dispatchHeadings)SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(nextSection),TRUE);
    if(profileControls)profileControls->headingFont(nextSection);
    if(updateControls)updateControls->headingFont(nextSection);
    if(sectionFont)DeleteObject(sectionFont);sectionFont=nextSection;
    if(normalFont)DeleteObject(normalFont);if(titleFont)DeleteObject(titleFont);normalFont=next;titleFont=nextTitle;
}
static void selectRunningGame(RunningGames running) {
    const int nextMode=modeForRunningGame(appOptions.mode,running);
    if(nextMode==appOptions.mode||!livePanel||livePanel->shouldWarnOnClose())return;
    try{livePanel->selectGame(nextMode>=2);}catch(const std::exception&){return;}
    historyPage->setProfile({});profileSettings.clear();
    SetWindowTextW(window,SPLITHAUL_BRAND_WIDE);SetWindowTextW(title,SPLITHAUL_NAME_WIDE);
    appOptions.mode=nextMode;liveMode=true;route=nextMode==2?etsSingle:etsMultiple;
    selectModeOption(nextMode);
    livePanel->setTwoStop(multipleMode(nextMode));livePanel->setMultiDrop(nextMode>=4);render();mode();layout();
}
static void error(const std::exception& ex) {MessageBoxW(window,wide(ex.what()).c_str(),SPLITHAUL_NAME_WIDE,MB_OK|MB_ICONERROR);}
static LRESULT CALLBACK procedure(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    try {
        switch(message) {
        case WM_CREATE: {
            window=hwnd;
            SetPropW(hwnd,dispatchLauncher::windowProperty,reinterpret_cast<HANDLE>(dispatchLauncher::projectKey(model->root)));
            logo=control(L"STATIC",L"",SS_OWNERDRAW,601);
            title=control(L"STATIC",L"SplitHaul",SS_LEFT,0);
            subtitle=control(L"STATIC",L"",SS_LEFT,0);
            optionsButton=control(L"BUTTON",L"Options",WS_TABSTOP,301);
            historyButton=control(L"BUTTON",L"History",WS_TABSTOP,303);
            const wchar_t* sections[]={L"Route",L"Trailer and loading",L"Actions",L"Dispatch status",L"Job details"};
            for(size_t i=0;i<dispatchHeadings.size();++i)dispatchHeadings[i]=control(L"STATIC",sections[i],SS_LEFT,0);
            modeLabel=control(L"STATIC",L"Game and job type",SS_LEFT,0);
            modeCombo=control(WC_COMBOBOXW,L"",WS_TABSTOP|CBS_DROPDOWNLIST|CBS_HASSTRINGS|CBS_OWNERDRAWFIXED,300);
            for(const int modeId:{0,1,4,2,3,5}){const auto row=SendMessageW(modeCombo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(modeLabels[static_cast<size_t>(modeId)]));SendMessageW(modeCombo,CB_SETITEMDATA,static_cast<WPARAM>(row),modeId);}
            selectModeOption(appOptions.mode);
            randomCheck=control(L"BUTTON",L"Random",WS_TABSTOP|BS_AUTOCHECKBOX,305);
            fullCapacityCheck=control(L"BUTTON",L"Full trailer loads",WS_TABSTOP|BS_AUTOCHECKBOX,308);
            themeButton=control(L"BUTTON",appOptions.dark?L"Dark theme: On":L"Dark theme: Off",WS_TABSTOP,302);
            themeHeading=control(L"STATIC",L"Appearance",SS_LEFT,0);
            optionsHelp=control(L"STATIC",L"Full trailer loads fills new SplitHaul jobs to trailer weight/volume capacity, ignoring country road-weight limits. Existing deliveries keep their load.\n\nThe running game is selected automatically. Attach up to 3 cargo trailers in ATS or 2 in ETS2; dollies are excluded. Multi-drop supports 3-8 deliveries.",SS_LEFT,0);
            aboutHeading=control(L"STATIC",L"About SplitHaul",SS_LEFT,0);
            aboutText=control(L"STATIC",
                SPLITHAUL_BRAND_WIDE L" · Version " DISPATCH_VERSION_WIDE L" (development)\n"
                L"Updated " DISPATCH_RELEASE_DATE_WIDE L"\n"
                L"GitHub: johnkym1\n\n"
                L"Created in November 2026 for single-player ATS and ETS2 drivers. "
                L"SplitHaul helps you plan compatible loads, dispatch single, dual or multi-drop jobs, "
                L"and keep an exportable delivery history.",SS_LEFT,0);
            donateButton=control(L"BUTTON",L"Donate here",WS_TABSTOP,304);
            EnableWindow(donateButton,appLinks::donationAvailable);
            donateHelp=control(L"STATIC",appLinks::donationAvailable
                ?L"Support Grumpyman on GitHub Sponsors.\nChoose your own amount; no fixed tiers."
                :L"GitHub Sponsors is awaiting approval.\nChoose your own amount when live; no fixed tiers.",SS_LEFT,0);
            const wchar_t* captions[]={L"Starting city",L"Pickup company",L"Cargo",L"Final delivery city",L"Final delivery company",L"First delivery city",L"First delivery company"};
            for(size_t i=0;i<FieldCount;++i){labels[i]=control(L"STATIC",captions[i],SS_LEFT,0);combos[i]=control(WC_COMBOBOXW,L"",WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST|CBS_HASSTRINGS|CBS_OWNERDRAWFIXED,100+static_cast<int>(i));SendMessageW(combos[i],CB_SETMINVISIBLE,10,0);}
            hint=control(L"STATIC",L"",SS_LEFT,0);
            saveButton=control(L"BUTTON",L"Save and start plan",WS_TABSTOP|BS_DEFPUSHBUTTON,SaveId);
            newButton=control(L"BUTTON",L"New route",WS_TABSTOP,NewId);
            refreshButton=control(L"BUTTON",L"Refresh company list",WS_TABSTOP,RefreshId);
            const DWORD textStyle=WS_TABSTOP|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL;
            statusText=control(L"EDIT",L"",textStyle,0);details=control(L"EDIT",L"",textStyle,0);
            firstButton=control(L"BUTTON",L"Mark first delivery done",WS_TABSTOP,FirstId);
            finalButton=control(L"BUTTON",L"Mark final delivery complete",WS_TABSTOP,FinalId);
            livePanel=std::make_unique<LivePanel>(hwnd,instance,model->root);
            historyPage=std::make_unique<HistoryPage>(hwnd,instance,model->root,theme);
            profileControls=std::make_unique<ProfileControls>(hwnd,model->root,[]{return livePanel?livePanel->activeProfileData():fs::path();});
            updateControls=std::make_unique<UpdateControls>(hwnd);
            livePanel->setTwoStop(multipleMode(appOptions.mode));livePanel->setMultiDrop(appOptions.mode>=4);
            fonts();theme.apply(hwnd,appOptions.dark);historyPage->applyTheme();render();mode();layout();SetTimer(hwnd,1,1000,nullptr);return 0;
        }
        case WM_COMMAND: {
            const int id=LOWORD(wp);
            if(profileControls&&profileControls->command(id,HIWORD(wp)))return 0;
            if(id==306&&HIWORD(wp)==BN_CLICKED){if(updateControls&&updateControls->start(livePanel&&livePanel->shouldWarnOnClose()))DestroyWindow(hwnd);return 0;}
            if(id==301&&HIWORD(wp)==BN_CLICKED){optionsPage=!optionsPage;historyVisible=false;mode();layout();return 0;}
            if(id==303&&HIWORD(wp)==BN_CLICKED){historyVisible=!historyVisible;optionsPage=false;mode();layout();return 0;}
            if(id==305&&HIWORD(wp)==BN_CLICKED){
                auto next=appOptions;next.random=SendMessageW(randomCheck,BM_GETCHECK,0,0)==BST_CHECKED;
                try{saveCurrentOptions(next);}catch(...){SendMessageW(randomCheck,BM_SETCHECK,appOptions.random?BST_CHECKED:BST_UNCHECKED,0);throw;}
                appOptions=next;livePanel->setRandom(appOptions.random);return 0;
            }
            if(id==308&&HIWORD(wp)==BN_CLICKED){
                auto next=appOptions;next.fullCapacity=SendMessageW(fullCapacityCheck,BM_GETCHECK,0,0)==BST_CHECKED;
                try{saveCurrentOptions(next);}catch(...){SendMessageW(fullCapacityCheck,BM_SETCHECK,appOptions.fullCapacity?BST_CHECKED:BST_UNCHECKED,0);throw;}
                appOptions=next;livePanel->setFullCapacity(appOptions.fullCapacity);return 0;
            }
            if(id==304&&HIWORD(wp)==BN_CLICKED){
                if(appLinks::donationAvailable&&reinterpret_cast<INT_PTR>(ShellExecuteW(hwnd,L"open",appLinks::donationUrl,nullptr,nullptr,SW_SHOWNORMAL))<=32)
                    throw std::runtime_error("Cannot open the donation page. Check your default browser and try again.");
                return 0;
            }
            if(id==302&&HIWORD(wp)==BN_CLICKED){auto next=appOptions;next.dark=!next.dark;saveCurrentOptions(next);appOptions=next;SetWindowTextW(themeButton,appOptions.dark?L"Dark theme: On":L"Dark theme: Off");theme.apply(hwnd,appOptions.dark);historyPage->applyTheme();return 0;}
            if(id==300&&HIWORD(wp)==CBN_SELCHANGE){
                auto next=appOptions;const auto row=SendMessageW(modeCombo,CB_GETCURSEL,0,0);if(row==CB_ERR)return 0;next.mode=static_cast<int>(SendMessageW(modeCombo,CB_GETITEMDATA,static_cast<WPARAM>(row),0));
                if(next.mode!=appOptions.mode&&livePanel->shouldWarnOnClose()){selectModeOption(appOptions.mode);throw std::runtime_error("Finish the current delivery plan before changing its game or delivery mode.");}
                const bool changingGame=etsMode(next.mode)!=etsMode(appOptions.mode);
                if(changingGame&&livePanel->shouldWarnOnClose()){selectModeOption(appOptions.mode);throw std::runtime_error("Finish the current game job before switching games in SplitHaul.");}
                if(changingGame){
                    try{livePanel->selectGame(etsMode(next.mode));}catch(...){selectModeOption(appOptions.mode);throw;}
                    historyPage->setProfile({});profileSettings.clear();SetWindowTextW(hwnd,SPLITHAUL_BRAND_WIDE);SetWindowTextW(title,SPLITHAUL_NAME_WIDE);
                }else saveCurrentOptions(next);
                saveOptions(model->root,next);
                if(appOptions.mode==2)etsSingle=route;else if(appOptions.mode==3)etsMultiple=route;
                appOptions=next;liveMode=true;route=appOptions.mode==2?etsSingle:etsMultiple;livePanel->setTwoStop(multipleMode(appOptions.mode));livePanel->setMultiDrop(appOptions.mode>=4);render();mode();layout();return 0;
            }
            if(livePanel&&livePanel->command(id,HIWORD(wp)))return 0;
            if(historyPage&&historyPage->command(id,HIWORD(wp)))return 0;
            if(id>=100&&id<100+static_cast<int>(FieldCount)&&HIWORD(wp)==CBN_SELCHANGE&&!syncing) {
                const auto index=static_cast<size_t>(id-100);route.values[index]=selection(combos[index]);
                for(size_t i=index+1;i<FieldCount;++i)route.values[i].clear();render();return 0;
            }
            if(HIWORD(wp)!=BN_CLICKED)return 0;
            if(id==SaveId){if(appOptions.mode==2)model->startSingle(route);else model->start(route);render();}
            else if(id==NewId){State next;if(appOptions.mode==2)writeJsonAtomic(model->root/"dispatch_single.json",encodeState(next));else model->save(next);route=next;render();SetFocus(combos[0]);}
            else if(id==RefreshId){model->refresh();render();MessageBoxW(hwnd,(std::to_wstring(model->catalog.size())+L" cities loaded from extracted ETS2 definitions.").c_str(),L"Company list refreshed",MB_OK|MB_ICONINFORMATION);}
            else if((id==FirstId&&route.stage=="to_d2")||(id==FinalId&&route.stage=="to_d1")){auto next=route;advance(next);if(appOptions.mode==2)writeJsonAtomic(model->root/"dispatch_single.json",encodeState(next));else model->save(next);route=next;render();}
            return 0;
        }
        case WM_SIZE:layout();return 0;
        case WM_TIMER:if(livePanel){const auto running=detectRunningGames();if(running!=lastRunningGames){lastRunningGames=running;selectRunningGame(running);}livePanel->tick();if(livePanel->connectedGameExited()&&!(updateControls&&updateControls->keepingOpen())){DestroyWindow(hwnd);return 0;}if(historyPage)historyPage->setProfile(livePanel->activeProfileData());}if(historyPage)historyPage->tick();if(optionsPage&&profileControls)profileControls->refresh();if(updateControls)updateControls->poll();return 0;
        case WM_NOTIFY:{LRESULT result=0;if(historyPage&&historyPage->notify(reinterpret_cast<const NMHDR*>(lp),result))return result;break;}
        case HistoryPage::OpenMessage:historyVisible=true;optionsPage=false;mode();layout();return 0;
        case LivePanel::RestartReviewMessage:if(livePanel)livePanel->reviewRestart();return 0;
        case LivePanel::ProfileTitleMessage:{
            const auto caption=reinterpret_cast<const wchar_t*>(lp);const auto profile=reinterpret_cast<const std::string*>(wp);
            if(caption){SetWindowTextW(hwnd,caption);if(title)SetWindowTextW(title,SPLITHAUL_NAME_WIDE);}
            if(profile&&!profile->empty()){
                const auto path=ats::ensureProfileFiles(model->root,*profile)/"settings.config";
                const auto data=live::readJson(path);if(data.value("profile_name",std::string())!=*profile)throw std::runtime_error("Profile settings do not match the current game profile.");
                auto next=decodeOptions(data);next.mode=modeForGame(next.mode,dispatchGame::isETS2());profileSettings=path;appOptions=next;liveMode=true;
                if(modeCombo)selectModeOption(next.mode);
                if(themeButton)SetWindowTextW(themeButton,next.dark?L"Dark theme: On":L"Dark theme: Off");
                if(livePanel){route=next.mode==2?etsSingle:etsMultiple;livePanel->setTwoStop(multipleMode(next.mode));livePanel->setMultiDrop(next.mode>=4);theme.apply(hwnd,next.dark);if(historyPage)historyPage->applyTheme();render();mode();layout();}
            }else profileSettings.clear();
            return 0;
        }
        case dispatchLauncher::showMessage:lastRunningGames=detectRunningGames();selectRunningGame(lastRunningGames);ShowWindow(hwnd,IsIconic(hwnd)?SW_RESTORE:SW_SHOW);SetForegroundWindow(hwnd);return 0;
        case WM_CLOSE:
            if(livePanel&&livePanel->shouldWarnOnClose()&&MessageBoxW(hwnd,L"A delivery plan is active. Keep SplitHaul open while delivering to monitor load weights and the next stop.\n\nClosing this app does not cancel the assigned job. Close it anyway?",L"Keep SplitHaul open",MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)!=IDYES)return 0;
            DestroyWindow(hwnd);return 0;
        case WM_DRAWITEM:{const auto item=reinterpret_cast<DRAWITEMSTRUCT*>(lp);if(item&&item->CtlID==601&&branding){branding->paint(*item,theme.backgroundHandle());return TRUE;}if(item&&theme.draw(*item))return TRUE;break;}
        case WM_MEASUREITEM:{const auto item=reinterpret_cast<MEASUREITEMSTRUCT*>(lp);if(item&&item->CtlType==ODT_COMBOBOX){item->itemHeight=static_cast<UINT>(scale(24));return TRUE;}break;}
        case WM_GETMINMAXINFO: {
            auto info=reinterpret_cast<MINMAXINFO*>(lp);const UINT dpi=GetDpiForWindow(hwnd);RECT r={0,0,MulDiv(700,static_cast<int>(dpi),96),MulDiv(DispatchLayout::minimumHeight,static_cast<int>(dpi),96)};
            AdjustWindowRectExForDpi(&r,WS_OVERLAPPEDWINDOW,FALSE,0,dpi);info->ptMinTrackSize={r.right-r.left,r.bottom-r.top};return 0;
        }
        case WM_DPICHANGED: {const auto r=reinterpret_cast<RECT*>(lp);SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);fonts();layout();return 0;}
        case WM_CTLCOLORSTATIC:{const auto control=reinterpret_cast<HWND>(lp);const int id=GetDlgCtrlID(control);const auto dc=reinterpret_cast<HDC>(wp);const auto brush=theme.color(dc,id==617||id==618,id==619||!IsWindowEnabled(control));if(id==LivePanel::RoadTrainWarningId)SetTextColor(dc,appOptions.dark?RGB(255,100,100):RGB(190,0,0));return brush;}
        case WM_CTLCOLORBTN:return theme.color(reinterpret_cast<HDC>(wp),false,!IsWindowEnabled(reinterpret_cast<HWND>(lp)));
        case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:return theme.color(reinterpret_cast<HDC>(wp),true);
        case WM_ERASEBKGND:{RECT area;GetClientRect(hwnd,&area);const auto dc=reinterpret_cast<HDC>(wp);FillRect(dc,&area,theme.backgroundHandle());const auto dpi=static_cast<int>(GetDpiForWindow(hwnd));paintPageBoxes(dc,MulDiv(area.right,96,dpi),MulDiv(area.bottom,96,dpi));return TRUE;}
        case WM_DESTROY:RemovePropW(hwnd,dispatchLauncher::windowProperty);KillTimer(hwnd,1);PostQuitMessage(0);return 0;
        }
    }catch(const std::exception& ex){syncing=false;error(ex);if(message==WM_CREATE)return -1;return 0;}
    return DefWindowProcW(hwnd,message,wp,lp);
}
static fs::path dataRoot() {
    int argc=0;LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(!argv)throw std::runtime_error("Cannot read command line.");
    fs::path selected;
    for(int i=1;i<argc;++i){if(std::wstring(argv[i])==L"--data-dir"&&i+1<argc)selected=argv[++i];else{LocalFree(argv);throw std::runtime_error("Usage: SplitHaul.exe [--data-dir folder]");}}
    LocalFree(argv);if(!selected.empty()){customDataRoot=true;return fs::absolute(selected);}
    wchar_t name[32768];const DWORD count=GetModuleFileNameW(nullptr,name,32768);if(!count||count>=32768)throw std::runtime_error("Cannot locate this executable.");
    fs::path root=fs::path(name).parent_path();
    if(fs::exists(root/"installed-layout.json")) {
        PWSTR local=nullptr;
        if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&local)))throw std::runtime_error("Cannot locate your local app-data folder.");
        const auto userData=installedStorage::userDataRoot(fs::path(local));CoTaskMemFree(local);
        return installedStorage::prepare(root,userData);
    }
    for(int i=0;i<5;++i){if(fs::exists(root/"company_catalog.json")||fs::is_directory(root/"game_data"/"base"/"def"/"company"))return root;root=root.parent_path();}
    throw std::runtime_error("Cannot find app data. Keep the executable with this project, or use --data-dir.");
}
int WINAPI wWinMain(HINSTANCE app,HINSTANCE,PWSTR,int show) {
    instance=app;SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HANDLE mutex=nullptr;int result=1;
    try {
        const auto root=dataRoot();
        const auto mutexName=L"Local\\CppDispatchPlanner-"+std::to_wstring(dispatchLauncher::projectKey(root));
        mutex=CreateMutexW(nullptr,FALSE,mutexName.c_str());
        if(!mutex)throw std::runtime_error("Could not initialize the app instance.");
        if(GetLastError()==ERROR_ALREADY_EXISTS)throw std::runtime_error("The dispatch planner for this data folder is already open.");
        model=std::make_unique<Model>(root);etsMultiple=loadState(root);etsSingle=fs::exists(root/"dispatch_single.json")?decodeState(readJson(root/"dispatch_single.json")):State{};
        int fallbackMode=0;if(fs::exists(root/"ats_dispatch.json"))fallbackMode=readJson(root/"ats_dispatch.json").value("two_stop",false)?1:0;
        appOptions=loadOptions(root,fallbackMode);lastRunningGames=detectRunningGames();appOptions.mode=modeForRunningGame(appOptions.mode,lastRunningGames);liveMode=true;route=appOptions.mode==2?etsSingle:etsMultiple;
        dispatchGame::select(etsMode(appOptions.mode));
        bool launcherRegistered=true;
        if(!customDataRoot){
            dispatchGame::select(false);launcherRegistered=dispatchLauncher::registerApp(root);
            dispatchGame::select(true);launcherRegistered=dispatchLauncher::registerApp(root)&&launcherRegistered;
            dispatchGame::select(etsMode(appOptions.mode));
        }
        if(!customDataRoot&&!launcherRegistered)MessageBoxW(nullptr,L"The Shift+F9 app shortcut could not be registered. SplitHaul can still be used normally. Open it again after checking access to the live-dispatch folder.",L"SplitHaul shortcut",MB_OK|MB_ICONWARNING);
        branding=std::make_unique<Branding>(app);
        INITCOMMONCONTROLSEX common={sizeof(common),ICC_STANDARD_CLASSES|ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&common);
        WNDCLASSEXW wc={};wc.cbSize=sizeof(wc);wc.hInstance=app;wc.lpfnWndProc=procedure;wc.lpszClassName=L"CppTwoStopDispatch";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=branding->largeIcon();wc.hIconSm=branding->smallIcon();wc.hbrBackground=GetSysColorBrush(COLOR_WINDOW);
        if(!RegisterClassExW(&wc))throw std::runtime_error("Cannot register the app window.");
        const UINT dpi=GetDpiForSystem();RECT r={0,0,MulDiv(780,static_cast<int>(dpi),96),MulDiv(DispatchLayout::minimumHeight,static_cast<int>(dpi),96)};AdjustWindowRectExForDpi(&r,WS_OVERLAPPEDWINDOW,FALSE,0,dpi);
        const auto caption=SPLITHAUL_BRAND_WIDE;
        window=CreateWindowExW(0,wc.lpszClassName,caption,WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,app,nullptr);
        if(!window)throw std::runtime_error("Cannot create the app window.");
        // The opening monitor can use a different DPI from the system default.
        // Size the height using the window's actual DPI so the lower panels fit.
        const UINT windowDpi=GetDpiForWindow(window);RECT opening={};GetWindowRect(window,&opening);
        RECT tall={0,0,0,MulDiv(DispatchLayout::minimumHeight,static_cast<int>(windowDpi),96)};
        AdjustWindowRectExForDpi(&tall,WS_OVERLAPPEDWINDOW,FALSE,0,windowDpi);
        const int openingHeight=tall.bottom-tall.top;
        MONITORINFO monitor={};monitor.cbSize=sizeof(monitor);int openingY=opening.top;
        if(GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor))openingY=(std::max)(monitor.rcWork.top,(std::min)(opening.top,monitor.rcWork.bottom-openingHeight));
        SetWindowPos(window,nullptr,opening.left,openingY,opening.right-opening.left,openingHeight,SWP_NOZORDER|SWP_NOACTIVATE);
        ShowWindow(window,show);SetForegroundWindow(window);UpdateWindow(window);MSG msg;int next=0;
        while((next=GetMessageW(&msg,nullptr,0,0))>0){if(!IsDialogMessageW(window,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
        result=next<0?1:static_cast<int>(msg.wParam);
    }catch(const std::exception& ex){error(ex);}
    if(mutex)CloseHandle(mutex);if(normalFont)DeleteObject(normalFont);if(titleFont)DeleteObject(titleFont);if(sectionFont)DeleteObject(sectionFont);return result;
}
