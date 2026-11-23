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
void text(HWND control,const std::wstring& value){SetWindowTextW(control,value.c_str());}
std::wstring number(double value,int precision){std::wostringstream out;out<<std::fixed<<std::setprecision(precision)<<value;return out.str();}
std::wstring weight(const ats::HistoryEntry& entry){return entry.massKg?number(*entry.massKg/1000,2)+L" t":L"Not recorded";}
std::wstring date(uint64_t ms,bool compact=false){
    ULARGE_INTEGER stamp;stamp.QuadPart=(ms+11644473600000ULL)*10000;const FILETIME file={stamp.LowPart,stamp.HighPart};SYSTEMTIME utc={},local={};
    if(!FileTimeToSystemTime(&file,&utc)||!SystemTimeToTzSpecificLocalTime(nullptr,&utc,&local))throw std::runtime_error("Cannot display delivery date.");
    wchar_t day[96]={},time[48]={};if(!GetDateFormatEx(LOCALE_NAME_USER_DEFAULT,0,&local,compact?L"dd MMM yy":L"dd MMM yyyy",day,96,nullptr)||!GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT,0,&local,L"HH:mm",time,48))throw std::runtime_error("Cannot format delivery date.");return std::wstring(day)+L" "+time;
}
void checked(HRESULT result,const char* message){if(FAILED(result))throw std::runtime_error(message);}
struct Apartment {HRESULT result=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE);~Apartment(){if(SUCCEEDED(result))CoUninitialize();}};
LRESULT CALLBACK listNotifications(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR context){
    if(message==WM_NOTIFY){const auto notification=reinterpret_cast<const NMHDR*>(lp);if(notification&&notification->code==NM_CUSTOMDRAW){LRESULT result=0;if(reinterpret_cast<HistoryPage*>(context)->notify(notification,result))return result;}}
    if(message==WM_NCDESTROY)RemoveWindowSubclass(window,listNotifications,id);return DefSubclassProc(window,message,wp,lp);
}
}
HistoryPage::HistoryPage(HWND owner,HINSTANCE instance,const std::filesystem::path& root,AppTheme& appearance):parent(owner),project(root),theme(appearance){
    auto control=[&](const wchar_t* kind,const wchar_t* caption,DWORD style,int id){const auto item=CreateWindowW(kind,caption,WS_CHILD|style,0,0,0,0,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance,nullptr);if(!item)throw std::runtime_error("Cannot create history controls.");return item;};
    summary=control(L"STATIC",L"",SS_LEFT,0);help=control(L"STATIC",L"Game verified: ✓ game telemetry · ✗ driver confirmed. Select a row for full details.",SS_LEFT,0);
    list=control(WC_LISTVIEWW,L"Completed jobs",WS_TABSTOP|WS_BORDER|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS|LVS_NOSORTHEADER,612);
    ListView_SetExtendedListViewStyle(list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP);
    SetWindowSubclass(list,listNotifications,1,reinterpret_cast<DWORD_PTR>(this));
    const wchar_t* captions[]={L"Game verified",L"Date (local)",L"Cargo",L"Pickup",L"Delivery",L"Load",L"Result"};
    for(int i=0;i<7;++i){LVCOLUMNW column={};column.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM;column.pszText=const_cast<wchar_t*>(captions[i]);column.cx=110;column.iSubItem=i;ListView_InsertColumn(list,i,&column);}
    details=control(L"STATIC",L"",SS_LEFT,0);exportButton=control(L"BUTTON",L"Export CSV...",WS_TABSTOP,610);refreshButton=control(L"BUTTON",L"Refresh history",WS_TABSTOP,611);
    status=control(L"STATIC",L"Export CSV opens Save As. Choose a folder and filename for your copy.",SS_LEFT,0);
}
void HistoryPage::layout(int width,int height,UINT dpi){
    auto place=[dpi](HWND item,int x,int y,int w,int h){MoveWindow(item,MulDiv(x,static_cast<int>(dpi),96),MulDiv(y,static_cast<int>(dpi),96),MulDiv(w,static_cast<int>(dpi),96),MulDiv(h,static_cast<int>(dpi),96),TRUE);};
    const int buttonsY=height-100,detailsY=buttonsY-190;place(summary,24,84,width-48,30);place(help,24,118,width-48,28);place(list,24,150,width-48,detailsY-166);place(details,24,detailsY,width-48,174);place(exportButton,24,buttonsY,190,36);place(refreshButton,230,buttonsY,160,36);place(status,24,buttonsY+46,width-48,46);
    fitColumns();
}
void HistoryPage::fitColumns(){
    RECT area;GetClientRect(list,&area);const auto dpi=static_cast<int>(GetDpiForWindow(parent));auto pixels=[dpi](int value){return MulDiv(value,dpi,96);};
    const int available=area.right-pixels(370)-2,cargo=(std::max)(pixels(90),available*27/100),city=(available-cargo)/2;
    const int columns[]={pixels(105),pixels(125),cargo,city,available-cargo-city,pixels(65),pixels(75)};for(int i=0;i<7;++i)ListView_SetColumnWidth(list,i,columns[i]);
}
void HistoryPage::applyTheme(){
    SetWindowTheme(list,L"",L"");SetWindowTheme(ListView_GetHeader(list),L"",L"");ListView_SetBkColor(list,theme.surface);ListView_SetTextBkColor(list,theme.surface);ListView_SetTextColor(list,theme.foreground);RedrawWindow(list,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
}
void HistoryPage::show(bool value){visible=value;for(const auto item:{summary,help,list,details,exportButton,refreshButton,status})ShowWindow(item,value?SW_SHOW:SW_HIDE);if(value)refresh();}
void HistoryPage::setProfile(const std::filesystem::path& path){if(profileData==path)return;profileData=path;catalog.reset();entries.clear();ListView_DeleteAllItems(list);exportNotice.clear();if(visible)refresh();}
void HistoryPage::refresh(){
    if(exporting)return;lastRefresh=GetTickCount64();
    try{
        if(!catalog){text(summary,L"Loading job history...");RedrawWindow(parent,nullptr,nullptr,RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN);}
        if(profileData.empty()){ListView_DeleteAllItems(list);entries.clear();text(summary,L"Waiting for a game profile.");text(details,L"Load a profile in the selected game to view its history.");EnableWindow(exportButton,FALSE);return;}
        if(!catalog)catalog=std::make_unique<ats::Catalog>(project);ats::exportJobHistory(profileData,live::ipcFolder(),catalog.get(),true);auto loaded=ats::readHistoryEntries(profileData,catalog.get());
        const int selected=ListView_GetNextItem(list,-1,LVNI_SELECTED);std::string selectedId,selectedSession;if(selected>=0&&static_cast<size_t>(selected)<entries.size()){selectedId=entries[static_cast<size_t>(selected)].id;selectedSession=entries[static_cast<size_t>(selected)].session;}
        populating=true;SendMessageW(list,WM_SETREDRAW,FALSE,0);ListView_DeleteAllItems(list);entries=std::move(loaded);int select=entries.empty()?-1:0;size_t confirmed=0,cancelled=0;
        for(size_t i=0;i<entries.size();++i){const auto& entry=entries[i];if(entry.driverConfirmed)++confirmed;if(entry.outcome=="Cancelled")++cancelled;if(entry.id==selectedId&&entry.session==selectedSession)select=static_cast<int>(i);
            const std::wstring cells[]={entry.driverConfirmed?L"✗ Driver":L"✓ Game",date(entry.completedMs,true),wide(entry.cargo),wide(entry.pickupCity),wide(entry.deliveryCity),entry.massKg?weight(entry):L"—",wide(entry.outcome)};
            LVITEMW item={};item.mask=LVIF_TEXT;item.iItem=static_cast<int>(i);item.pszText=const_cast<wchar_t*>(cells[0].c_str());const auto row=ListView_InsertItem(list,&item);for(int column=1;column<7;++column)ListView_SetItemText(list,row,column,const_cast<wchar_t*>(cells[column].c_str()));
        }
        if(select>=0){ListView_SetItemState(list,select,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);ListView_EnsureVisible(list,select,FALSE);}SendMessageW(list,WM_SETREDRAW,TRUE,0);fitColumns();InvalidateRect(list,nullptr,TRUE);populating=false;
        text(summary,std::to_wstring(entries.size())+L" jobs  ·  "+std::to_wstring(entries.size()-cancelled)+L" delivered  ·  "+std::to_wstring(cancelled)+L" cancelled  ·  "+std::to_wstring(confirmed)+L" driver-confirmed");
        text(status,exportNotice.empty()?L"Export CSV opens Save As. Choose a folder and filename for your copy.":exportNotice);EnableWindow(exportButton,TRUE);describeSelection();
    }catch(const std::exception& ex){populating=false;SendMessageW(list,WM_SETREDRAW,TRUE,0);text(status,L"Could not refresh history: "+wide(ex.what()));}
}
void HistoryPage::describeSelection(){
    const int selected=ListView_GetNextItem(list,-1,LVNI_SELECTED);if(selected<0||static_cast<size_t>(selected)>=entries.size()){text(details,entries.empty()?L"No completed jobs have been recorded yet.\r\nFinished deliveries will appear here automatically.":L"Select a job above to see its full details.");return;}
    const auto& entry=entries[static_cast<size_t>(selected)];std::wstring description=wide(entry.cargo)+L"  ·  "+wide(entry.outcome)+L"\r\nPickup: "+wide(entry.pickupCompany)+L" — "+wide(entry.pickupCity)+L"\r\nDelivery: "+wide(entry.deliveryCompany)+L" — "+wide(entry.deliveryCity)+L"\r\nLoad: "+weight(entry);if(entry.units)description+=L" ("+std::to_wstring(*entry.units)+L" units)";
    description+=L"  ·  Source: "+wide(entry.source)+L"\r\nRevenue: "+(entry.revenue?number(*entry.revenue,0):L"Not recorded")+L"  ·  Distance: "+(entry.distanceKm?number(*entry.distanceKm,1)+L" km":L"Not recorded")+L"  ·  Damage: "+(entry.damage?number(*entry.damage*100,1)+L"%":L"Not recorded");
    description+=L"\r\n"+date(entry.completedMs)+(entry.driverConfirmed?L" — driver confirmation time; delivery time was not recorded.":L" — game completion time.");if(entry.stop)description+=L"\r\nTwo-stop plan: delivery "+std::to_wstring(entry.stop)+L" of 2.";text(details,description);
}
void HistoryPage::exportCsv(){
    if(exporting||profileData.empty())return;refresh();exporting=true;
    try{
        Apartment apartment;checked(apartment.result,"Cannot open the Windows Save As dialog.");Microsoft::WRL::ComPtr<IFileSaveDialog> dialog;checked(CoCreateInstance(CLSID_FileSaveDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(dialog.GetAddressOf())),"Cannot create the Save As dialog.");
        GUID client={0x47544859,0x1127,0x4d98,{}};const auto key=static_cast<uint64_t>(dispatchLauncher::projectKey(project));for(unsigned i=0;i<8;++i)client.Data4[i]=static_cast<BYTE>((key>>(i*8))&0xff);checked(dialog->SetClientGuid(client),"Cannot restore the export folder preference.");
        const COMDLG_FILTERSPEC filters[]={{L"CSV spreadsheet (*.csv)",L"*.csv"}};checked(dialog->SetFileTypes(1,filters),"Cannot configure the CSV file type.");checked(dialog->SetDefaultExtension(L"csv"),"Cannot set the CSV file extension.");checked(dialog->SetFileName(L"Dispatch job history.csv"),"Cannot set the export filename.");dialog->SetTitle(L"Export job history");FILEOPENDIALOGOPTIONS options={};checked(dialog->GetOptions(&options),"Cannot configure Save As.");checked(dialog->SetOptions(options|FOS_FORCEFILESYSTEM|FOS_OVERWRITEPROMPT|FOS_PATHMUSTEXIST|FOS_STRICTFILETYPES),"Cannot configure Save As.");
        const auto result=dialog->Show(parent);if(result==HRESULT_FROM_WIN32(ERROR_CANCELLED)){exportNotice=L"Export cancelled. No copy was saved.";text(status,exportNotice);lastRefresh=GetTickCount64();exporting=false;return;}checked(result,"The Save As dialog could not complete.");
        Microsoft::WRL::ComPtr<IShellItem> selected;checked(dialog->GetResult(selected.GetAddressOf()),"No export destination was selected.");PWSTR path=nullptr;checked(selected->GetDisplayName(SIGDN_FILESYSPATH,&path),"Select a normal folder for the exported CSV.");const auto destination=std::filesystem::path(path);CoTaskMemFree(path);
        ats::exportJobHistory(profileData,live::ipcFolder(),catalog.get(),true);ats::saveHistoryCsvCopy(profileData,destination,true);exportNotice=L"Last export saved to: "+destination.wstring();text(status,exportNotice);
    }catch(const std::exception& ex){exportNotice=L"Export failed: "+wide(ex.what());text(status,exportNotice);}lastRefresh=GetTickCount64();exporting=false;
}
void HistoryPage::tick(){if(visible&&!exporting&&GetTickCount64()-lastRefresh>=30000)refresh();}
bool HistoryPage::command(int id,int notification){if(notification!=BN_CLICKED)return false;if(id==610){exportCsv();return true;}if(id==611){refresh();return true;}return false;}
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
