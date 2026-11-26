#include "history_page.h"
#include "win_text.h"
#include "../dispatch_shared/app_launcher.h"
#include <shobjidl.h>
#include <wrl/client.h>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace {
std::wstring wide(const std::string& value) {
    return winText::toWide(value);
}
void text(HWND control,const std::wstring& value){const int length=GetWindowTextLengthW(control);std::wstring current(static_cast<size_t>(length)+1,L'\0');GetWindowTextW(control,current.data(),length+1);current.resize(static_cast<size_t>(length));if(current!=value)SetWindowTextW(control,value.c_str());}
std::wstring number(double value,int precision){std::wostringstream out;out<<std::fixed<<std::setprecision(precision)<<value;return out.str();}
std::wstring weight(const ats::HistoryEntry& entry){return entry.massKg?number(*entry.massKg/1000,2)+L" t":L"Not recorded";}
std::wstring date(uint64_t ms,bool compact=false){
    ULARGE_INTEGER stamp;stamp.QuadPart=(ms+11644473600000ULL)*10000;const FILETIME file={stamp.LowPart,stamp.HighPart};SYSTEMTIME utc={},local={};
    if(!FileTimeToSystemTime(&file,&utc)||!SystemTimeToTzSpecificLocalTime(nullptr,&utc,&local))throw std::runtime_error("Cannot display delivery date.");
    wchar_t day[96]={},time[48]={};if(!GetDateFormatEx(LOCALE_NAME_USER_DEFAULT,0,&local,compact?L"dd MMM yy":L"dd MMM yyyy",day,96,nullptr)||!GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT,0,&local,L"HH:mm",time,48))throw std::runtime_error("Cannot format delivery date.");return std::wstring(day)+L" "+time;
}
void checked(HRESULT result,const char* message){if(FAILED(result))throw std::runtime_error(message);}
struct Apartment {HRESULT result=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE);~Apartment(){if(SUCCEEDED(result))CoUninitialize();}};
struct CatalogGameScope {bool previous=dispatchGame::isETS2();explicit CatalogGameScope(bool ets){dispatchGame::select(ets);}~CatalogGameScope(){dispatchGame::select(previous);}};
LRESULT CALLBACK listNotifications(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR context){
    if(message==WM_NOTIFY){const auto notification=reinterpret_cast<const NMHDR*>(lp);if(notification&&notification->code==NM_CUSTOMDRAW){LRESULT result=0;if(reinterpret_cast<HistoryPage*>(context)->notify(notification,result))return result;}}
    if(message==WM_NCDESTROY)RemoveWindowSubclass(window,listNotifications,id);return DefSubclassProc(window,message,wp,lp);
}
}
HistoryPage::HistoryPage(HWND owner,HINSTANCE instance,const std::filesystem::path& root,AppTheme& appearance):parent(owner),project(root),theme(appearance){
    auto control=[&](const wchar_t* kind,const wchar_t* caption,DWORD style,int id){const auto item=CreateWindowW(kind,caption,WS_CHILD|style,0,0,0,0,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance,nullptr);if(!item)throw std::runtime_error("Cannot create history controls.");return item;};
    gameLabel=control(L"STATIC",L"Game",SS_LEFT,0);gameCombo=control(WC_COMBOBOXW,L"",WS_TABSTOP|CBS_DROPDOWNLIST|CBS_HASSTRINGS|CBS_OWNERDRAWFIXED,613);
    for(const auto* name:{L"Select game",L"ATS",L"ETS2"})SendMessageW(gameCombo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));SendMessageW(gameCombo,CB_SETCURSEL,0,0);
    profileLabel=control(L"STATIC",L"Profile",SS_LEFT,0);profileCombo=control(WC_COMBOBOXW,L"",WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST|CBS_HASSTRINGS|CBS_OWNERDRAWFIXED,614);
    followButton=control(L"BUTTON",L"Loaded profile",WS_TABSTOP,615);EnableWindow(followButton,FALSE);
    summary=control(L"STATIC",L"",SS_LEFT|SS_ENDELLIPSIS,0);help=control(L"STATIC",L"Game verified: ✓ game telemetry · ✗ driver confirmed. Select a row for full details.",SS_LEFT,0);
    list=control(WC_LISTVIEWW,L"Completed jobs",WS_TABSTOP|WS_BORDER|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS|LVS_NOSORTHEADER,612);
    ListView_SetExtendedListViewStyle(list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP);
    SetWindowSubclass(list,listNotifications,1,reinterpret_cast<DWORD_PTR>(this));
    const wchar_t* captions[]={L"Game verified",L"Date (local)",L"Cargo",L"Pickup",L"Delivery",L"Load",L"Result"};
    for(int i=0;i<7;++i){LVCOLUMNW column={};column.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM;column.pszText=const_cast<wchar_t*>(captions[i]);column.cx=110;column.iSubItem=i;ListView_InsertColumn(list,i,&column);}
    detailHeading=control(L"STATIC",L"Selected job",SS_RIGHT|SS_ENDELLIPSIS,619);routeHeading=control(L"STATIC",L"Cargo and route",SS_LEFT,0);resultHeading=control(L"STATIC",L"Delivery results",SS_LEFT,0);
    const DWORD detailStyle=WS_TABSTOP|WS_BORDER|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL;
    details=control(L"EDIT",L"",detailStyle,617);resultDetails=control(L"EDIT",L"",detailStyle,618);
    exportButton=control(L"BUTTON",L"Export all CSV...",WS_TABSTOP,610);selectedExportButton=control(L"BUTTON",L"Export selected CSV...",WS_TABSTOP,616);refreshButton=control(L"BUTTON",L"Refresh history",WS_TABSTOP,611);
    status=control(L"STATIC",L"Choose a game and saved app profile. The game can be closed.",SS_CENTER,0);
    EnableWindow(exportButton,FALSE);EnableWindow(selectedExportButton,FALSE);refreshProfiles();
}
HistoryPage::~HistoryPage(){if(detailFont)DeleteObject(detailFont);if(detailHeadingFont)DeleteObject(detailHeadingFont);if(detailSectionFont)DeleteObject(detailSectionFont);}
void HistoryPage::detailFonts(UINT dpi){
    if(detailDpi==dpi)return;
    auto make=[dpi](int size,int bold){return CreateFontW(-MulDiv(size,static_cast<int>(dpi),72),0,0,0,bold,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");};
    const auto body=make(12,FW_NORMAL),heading=make(14,FW_SEMIBOLD),section=make(11,FW_SEMIBOLD);
    if(!body||!heading||!section){if(body)DeleteObject(body);if(heading)DeleteObject(heading);if(section)DeleteObject(section);throw std::runtime_error("Could not create the History detail fonts.");}
    for(auto h:{details,resultDetails}){SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(body),TRUE);SendMessageW(h,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(MulDiv(10,static_cast<int>(dpi),96),MulDiv(10,static_cast<int>(dpi),96)));}
    SendMessageW(detailHeading,WM_SETFONT,reinterpret_cast<WPARAM>(heading),TRUE);for(auto h:{routeHeading,resultHeading})SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(section),TRUE);
    if(detailFont)DeleteObject(detailFont);if(detailHeadingFont)DeleteObject(detailHeadingFont);if(detailSectionFont)DeleteObject(detailSectionFont);detailFont=body;detailHeadingFont=heading;detailSectionFont=section;detailDpi=dpi;
}
void HistoryPage::detailsMessage(const std::wstring& message){text(detailHeading,L"Selected job");text(details,message);text(resultDetails,L"");}
void HistoryPage::layout(int width,int height,UINT dpi){
    auto place=[dpi](HWND item,int x,int y,int w,int h){MoveWindow(item,MulDiv(x,static_cast<int>(dpi),96),MulDiv(y,static_cast<int>(dpi),96),MulDiv(w,static_cast<int>(dpi),96),MulDiv(h,static_cast<int>(dpi),96),TRUE);};
    const int buttonsY=height-100,gridHeight=(std::max)(180,(buttonsY-208)*45/100),detailsY=192+gridHeight+16;
    const int column=(width-64)/2,bodyY=detailsY+62,bodyHeight=buttonsY-16-bodyY;
    place(gameLabel,24,88,50,26);place(gameCombo,80,84,124,200);place(profileLabel,220,88,58,26);place(profileCombo,284,84,width-488,240);place(followButton,width-180,84,156,28);
    place(summary,24,126,width-48,30);place(help,24,160,width-48,28);place(list,24,192,width-48,gridHeight);
    place(detailHeading,24,detailsY,width-48,28);place(routeHeading,24,detailsY+34,column,24);place(resultHeading,40+column,detailsY+34,column,24);place(details,24,bodyY,column,bodyHeight);place(resultDetails,40+column,bodyY,column,bodyHeight);
    const int buttonsLeft=(width-552)/2;
    place(exportButton,buttonsLeft,buttonsY,170,36);place(selectedExportButton,buttonsLeft+186,buttonsY,190,36);place(refreshButton,buttonsLeft+392,buttonsY,160,36);place(status,24,buttonsY+46,width-48,46);
    detailFonts(dpi);fitColumns();
}
void HistoryPage::fitColumns(){
    RECT area;GetClientRect(list,&area);const auto dpi=static_cast<int>(GetDpiForWindow(parent));auto pixels=[dpi](int value){return MulDiv(value,dpi,96);};
    const int available=area.right-pixels(370)-2,cargo=(std::max)(pixels(90),available*27/100),city=(available-cargo)/2;
    const int columns[]={pixels(105),pixels(125),cargo,city,available-cargo-city,pixels(65),pixels(75)};for(int i=0;i<7;++i)ListView_SetColumnWidth(list,i,columns[i]);
}
void HistoryPage::applyTheme(){
    SetWindowTheme(list,L"",L"");SetWindowTheme(ListView_GetHeader(list),L"",L"");ListView_SetBkColor(list,theme.surface);ListView_SetTextBkColor(list,theme.surface);ListView_SetTextColor(list,theme.foreground);RedrawWindow(list,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
}
void HistoryPage::show(bool value){visible=value;for(const auto item:{gameLabel,gameCombo,profileLabel,profileCombo,followButton,summary,help,list,detailHeading,routeHeading,resultHeading,details,resultDetails,exportButton,selectedExportButton,refreshButton,status})ShowWindow(item,value?SW_SHOW:SW_HIDE);if(value)refresh();}
void HistoryPage::selectProfile(const std::filesystem::path& path,const std::string& name){
    profileData=path;profileName=name;catalog.reset();entries.clear();populating=true;ListView_DeleteAllItems(list);populating=false;exportNotice.clear();detailsMessage(L"Select a recorded job to see its details.");EnableWindow(exportButton,FALSE);EnableWindow(selectedExportButton,FALSE);
}
void HistoryPage::refreshProfiles(){
    if(SendMessageW(profileCombo,CB_GETDROPPEDSTATE,0,0))return;
    profiles=selectedGame<0?std::vector<profileManager::Entry>():profileManager::list(project,selectedGame==1);
    SendMessageW(profileCombo,CB_RESETCONTENT,0,0);SendMessageW(profileCombo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Select profile"));int selected=0;
    for(size_t i=0;i<profiles.size();++i){const auto caption=wide(profiles[i].name);SendMessageW(profileCombo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(caption.c_str()));if(profileData==profileManager::store(project,selectedGame==1)/profiles[i].id)selected=static_cast<int>(i)+1;}
    if(!selected&&!profileData.empty())selectProfile({},{});SendMessageW(profileCombo,CB_SETCURSEL,selected,0);EnableWindow(profileCombo,selectedGame>=0&&!profiles.empty());
}
void HistoryPage::setProfile(const std::filesystem::path& path){
    liveProfile=path;EnableWindow(followButton,!exporting&&!path.empty());if(exporting||manualBrowsing)return;
    // Retain the displayed history when a game disconnects.
    if(path.empty()){if(!profileData.empty())manualBrowsing=true;return;}
    if(profileData==path)return;
    const bool ets=path.parent_path()==profileManager::store(project,true);
    if(path.parent_path()!=profileManager::store(project,ets))return;
    const auto entry=profileManager::inspect(path);selectedGame=ets?1:0;SendMessageW(gameCombo,CB_SETCURSEL,selectedGame+1,0);selectProfile(path,entry.name);refreshProfiles();if(visible)refresh();
}
void HistoryPage::refresh(){
    if(exporting)return;lastRefresh=GetTickCount64();
    try{
        refreshProfiles();
        if(profileData.empty()){ListView_DeleteAllItems(list);entries.clear();text(summary,selectedGame<0?L"Choose a game, then a saved app profile.":profiles.empty()?L"No saved app profiles for this game.":L"Choose a saved app profile.");detailsMessage(L"Recorded jobs can be viewed and exported with the game closed.");text(status,L"Select a job to export just that row, or export all jobs for the selected profile.");EnableWindow(exportButton,FALSE);EnableWindow(selectedExportButton,FALSE);return;}
        if(!catalog)try{CatalogGameScope target(selectedGame==1);catalog=std::make_unique<ats::Catalog>(project);}catch(const std::exception&){/* History remains readable without a catalog. */}
        auto loaded=ats::readHistoryEntries(profileData,catalog.get());
        const int selected=ListView_GetNextItem(list,-1,LVNI_SELECTED);std::string selectedId,selectedSession;if(selected>=0&&static_cast<size_t>(selected)<entries.size()){selectedId=entries[static_cast<size_t>(selected)].id;selectedSession=entries[static_cast<size_t>(selected)].session;}
        populating=true;SendMessageW(list,WM_SETREDRAW,FALSE,0);ListView_DeleteAllItems(list);entries=std::move(loaded);int select=entries.empty()?-1:0;size_t confirmed=0,cancelled=0;
        for(size_t i=0;i<entries.size();++i){const auto& entry=entries[i];if(entry.driverConfirmed)++confirmed;if(entry.outcome=="Cancelled")++cancelled;if(entry.id==selectedId&&entry.session==selectedSession)select=static_cast<int>(i);
            const std::wstring cells[]={entry.driverConfirmed?L"✗ Driver":L"✓ Game",date(entry.completedMs,true),wide(entry.cargo),wide(entry.pickupCity),wide(entry.deliveryCity),entry.massKg?weight(entry):L"—",wide(entry.outcome)};
            LVITEMW item={};item.mask=LVIF_TEXT;item.iItem=static_cast<int>(i);item.pszText=const_cast<wchar_t*>(cells[0].c_str());const auto row=ListView_InsertItem(list,&item);for(int column=1;column<7;++column)ListView_SetItemText(list,row,column,const_cast<wchar_t*>(cells[column].c_str()));
        }
        if(select>=0){ListView_SetItemState(list,select,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);ListView_EnsureVisible(list,select,FALSE);}SendMessageW(list,WM_SETREDRAW,TRUE,0);fitColumns();InvalidateRect(list,nullptr,TRUE);populating=false;
        text(summary,std::wstring(selectedGame==1?L"ETS2":L"ATS")+L" · "+wide(profileName)+L" · "+std::to_wstring(entries.size())+L" jobs  ·  "+std::to_wstring(entries.size()-cancelled)+L" delivered  ·  "+std::to_wstring(cancelled)+L" cancelled  ·  "+std::to_wstring(confirmed)+L" driver-confirmed");
        text(status,exportNotice.empty()?L"Export selected saves one job. Export all saves this profile's complete history.":exportNotice);EnableWindow(exportButton,!entries.empty());describeSelection();
    }catch(const std::exception& ex){populating=true;ListView_DeleteAllItems(list);entries.clear();populating=false;SendMessageW(list,WM_SETREDRAW,TRUE,0);EnableWindow(exportButton,FALSE);EnableWindow(selectedExportButton,FALSE);detailsMessage(L"History could not be read. Refresh or select another profile.");text(status,L"Could not refresh history: "+wide(ex.what()));}
}
void HistoryPage::describeSelection(){
    const int selected=ListView_GetNextItem(list,-1,LVNI_SELECTED);EnableWindow(selectedExportButton,selected>=0&&static_cast<size_t>(selected)<entries.size());if(selected<0||static_cast<size_t>(selected)>=entries.size()){detailsMessage(entries.empty()?L"No completed jobs have been recorded yet.\r\nFinished deliveries will appear here automatically.":L"Select a job above to see its full details.");return;}
    const auto& entry=entries[static_cast<size_t>(selected)];text(detailHeading,L"Selected job — "+wide(entry.outcome));
    std::wstring route=L"Cargo\r\n"+wide(entry.cargo)+L"\r\n\r\nPickup\r\n"+wide(entry.pickupCity)+L"\r\n"+wide(entry.pickupCompany)+L"\r\n\r\nDelivery\r\n"+wide(entry.deliveryCity)+L"\r\n"+wide(entry.deliveryCompany);
    if(entry.stop)route+=std::wstring(entry.stopCount>2?L"\r\n\r\nMulti-drop plan\r\nDelivery ":L"\r\n\r\nTwo-stop plan\r\nDelivery ")+std::to_wstring(entry.stop)+L" of "+std::to_wstring(entry.stopCount);text(details,route);
    std::wstring result=(entry.driverConfirmed?L"Driver confirmation time\r\n":L"Completed (local time)\r\n")+date(entry.completedMs)+L"\r\n\r\nLoad weight: "+weight(entry)+L"\r\nCargo units: "+(entry.units?std::to_wstring(*entry.units):L"Not recorded");
    result+=L"\r\n\r\nRevenue: "+(entry.revenue?number(*entry.revenue,0):L"Not recorded")+L"\r\nDistance: "+(entry.distanceKm?number(*entry.distanceKm,1)+L" km":L"Not recorded")+L"\r\nCargo damage: "+(entry.damage?number(*entry.damage*100,1)+L"%":L"Not recorded");
    result+=entry.driverConfirmed?L"\r\n\r\nVerification\r\nConfirmed by the driver.\r\nGame completion was not recorded.":L"\r\n\r\nVerification\r\nRecorded by game telemetry.";text(resultDetails,result);
}
void HistoryPage::exportCsv(bool selectedOnly){
    if(exporting||profileData.empty())return;refresh();if(profileData.empty())return;
    std::string id,session;if(selectedOnly){const auto index=ListView_GetNextItem(list,-1,LVNI_SELECTED);if(index<0||static_cast<size_t>(index)>=entries.size())return;id=entries[static_cast<size_t>(index)].id;session=entries[static_cast<size_t>(index)].session;}
    const auto selectedProfile=profileData;exporting=true;EnableWindow(gameCombo,FALSE);EnableWindow(profileCombo,FALSE);EnableWindow(followButton,FALSE);
    try{
        Apartment apartment;checked(apartment.result,"Cannot open the Windows Save As dialog.");Microsoft::WRL::ComPtr<IFileSaveDialog> dialog;checked(CoCreateInstance(CLSID_FileSaveDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(dialog.GetAddressOf())),"Cannot create the Save As dialog.");
        GUID client={0x47544859,0x1127,0x4d98,{}};const auto key=static_cast<uint64_t>(dispatchLauncher::projectKey(project));for(unsigned i=0;i<8;++i)client.Data4[i]=static_cast<BYTE>((key>>(i*8))&0xff);checked(dialog->SetClientGuid(client),"Cannot restore the export folder preference.");
        const COMDLG_FILTERSPEC filters[]={{L"CSV spreadsheet (*.csv)",L"*.csv"}};checked(dialog->SetFileTypes(1,filters),"Cannot configure the CSV file type.");checked(dialog->SetDefaultExtension(L"csv"),"Cannot set the CSV file extension.");checked(dialog->SetFileName(selectedOnly?L"Dispatch selected job.csv":L"Dispatch job history.csv"),"Cannot set the export filename.");dialog->SetTitle(selectedOnly?L"Export selected job":L"Export profile history");FILEOPENDIALOGOPTIONS options={};checked(dialog->GetOptions(&options),"Cannot configure Save As.");checked(dialog->SetOptions(options|FOS_FORCEFILESYSTEM|FOS_OVERWRITEPROMPT|FOS_PATHMUSTEXIST|FOS_STRICTFILETYPES),"Cannot configure Save As.");
        const auto result=dialog->Show(parent);if(result==HRESULT_FROM_WIN32(ERROR_CANCELLED)){exportNotice=L"Export cancelled. No copy was saved.";text(status,exportNotice);lastRefresh=GetTickCount64();exporting=false;EnableWindow(gameCombo,TRUE);refreshProfiles();EnableWindow(followButton,!liveProfile.empty());return;}checked(result,"The Save As dialog could not complete.");
        Microsoft::WRL::ComPtr<IShellItem> selected;checked(dialog->GetResult(selected.GetAddressOf()),"No export destination was selected.");PWSTR path=nullptr;checked(selected->GetDisplayName(SIGDN_FILESYSPATH,&path),"Select a normal folder for the exported CSV.");const auto destination=std::filesystem::path(path);CoTaskMemFree(path);
        ats::saveHistoryCsvSelection(selectedProfile,destination,catalog.get(),id,session);exportNotice=L"Last export saved to: "+destination.wstring();text(status,exportNotice);
    }catch(const std::exception& ex){exportNotice=L"Export failed: "+wide(ex.what());text(status,exportNotice);}lastRefresh=GetTickCount64();exporting=false;EnableWindow(gameCombo,TRUE);refreshProfiles();EnableWindow(followButton,!liveProfile.empty());
}
void HistoryPage::tick(){if(visible&&!exporting&&GetTickCount64()-lastRefresh>=30000)refresh();}
bool HistoryPage::command(int id,int notification){
    if(exporting)return id>=610&&id<=616;
    if(id==613&&notification==CBN_SELCHANGE){manualBrowsing=true;selectedGame=static_cast<int>(SendMessageW(gameCombo,CB_GETCURSEL,0,0))-1;selectProfile({},{});refreshProfiles();refresh();return true;}
    if(id==614&&notification==CBN_SELCHANGE){manualBrowsing=true;const auto index=SendMessageW(profileCombo,CB_GETCURSEL,0,0);if(index>0&&static_cast<size_t>(index)<=profiles.size()){const auto& entry=profiles[static_cast<size_t>(index)-1];selectProfile(profileManager::store(project,selectedGame==1)/entry.id,entry.name);}else selectProfile({},{});refresh();return true;}
    if(notification!=BN_CLICKED)return false;if(id==610){exportCsv();return true;}if(id==616){exportCsv(true);return true;}if(id==611){refresh();return true;}if(id==615){if(!liveProfile.empty()){manualBrowsing=false;setProfile(liveProfile);refresh();}return true;}return false;
}
bool HistoryPage::notify(const NMHDR* message,LRESULT& result){
    if(message->hwndFrom==list&&message->code==LVN_ITEMCHANGED){if(!populating)describeSelection();result=0;return true;}
    if(message->hwndFrom==list&&message->code==NM_CUSTOMDRAW){auto draw=reinterpret_cast<NMLVCUSTOMDRAW*>(const_cast<NMHDR*>(message));if(draw->nmcd.dwDrawStage==CDDS_PREPAINT){result=CDRF_NOTIFYITEMDRAW;return true;}if(draw->nmcd.dwDrawStage==CDDS_ITEMPREPAINT){
        const bool selected=(ListView_GetItemState(list,static_cast<int>(draw->nmcd.dwItemSpec),LVIS_SELECTED)&LVIS_SELECTED)!=0;draw->clrText=theme.foreground;draw->clrTextBk=selected?theme.selection:theme.surface;draw->nmcd.uItemState&=~CDIS_SELECTED;result=CDRF_NOTIFYSUBITEMDRAW;return true;
    }if(draw->nmcd.dwDrawStage==(CDDS_ITEMPREPAINT|CDDS_SUBITEM)){
        const auto index=static_cast<size_t>(draw->nmcd.dwItemSpec);const bool selected=(ListView_GetItemState(list,static_cast<int>(index),LVIS_SELECTED)&LVIS_SELECTED)!=0;draw->clrText=draw->iSubItem==0&&index<entries.size()?(entries[index].driverConfirmed?theme.unverified:theme.verified):theme.foreground;draw->clrTextBk=selected?theme.selection:theme.surface;draw->nmcd.uItemState&=~CDIS_SELECTED;result=CDRF_NEWFONT;return true;
    }}
    if(message->hwndFrom==ListView_GetHeader(list)&&message->code==NM_CUSTOMDRAW){const auto draw=reinterpret_cast<const NMCUSTOMDRAW*>(message);if(draw->dwDrawStage==CDDS_PREPAINT){result=CDRF_NOTIFYITEMDRAW;return true;}if(draw->dwDrawStage==CDDS_ITEMPREPAINT){
        FillRect(draw->hdc,&draw->rc,theme.backgroundHandle());SetBkMode(draw->hdc,TRANSPARENT);SetTextColor(draw->hdc,theme.foreground);SelectObject(draw->hdc,reinterpret_cast<HFONT>(SendMessageW(list,WM_GETFONT,0,0)));wchar_t caption[128]={};HDITEMW item={};item.mask=HDI_TEXT;item.pszText=caption;item.cchTextMax=128;Header_GetItem(message->hwndFrom,static_cast<int>(draw->dwItemSpec),&item);auto box=draw->rc;box.left+=6;DrawTextW(draw->hdc,caption,-1,&box,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);result=CDRF_SKIPDEFAULT;return true;
    }}return false;
}
