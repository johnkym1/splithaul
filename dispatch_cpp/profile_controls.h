#pragma once
#include "profile_manager.h"
#include "win_text.h"
#include <commctrl.h>
#include <functional>

class ProfileControls {
    friend struct ProfileControlsTest;
    HWND parent,heading;
    std::array<HWND,2> labels{},combos{},buttons{};
    std::array<std::vector<profileManager::Entry>,2> entries;
    std::array<std::string,2> selected{},signatures{};
    std::filesystem::path root;
    std::function<std::filesystem::path()> activeProfile;
    std::function<std::array<profileManager::Protection,2>()> protectionReader;
    HWND make(const wchar_t* kind,const wchar_t* caption,DWORD style,int id){
        const auto control=CreateWindowExW(0,kind,caption,WS_CHILD|style,0,0,0,0,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
        if(!control)throw std::runtime_error("Could not create app profile controls.");return control;
    }
public:
    ProfileControls(HWND owner,const std::filesystem::path& data,std::function<std::filesystem::path()> active,std::function<std::array<profileManager::Protection,2>()> reader=profileManager::currentProtection):parent(owner),root(data),activeProfile(std::move(active)),protectionReader(std::move(reader)){
        heading=make(L"STATIC",L"App profiles",SS_LEFT,0);
        for(size_t i=0;i<2;++i){
            labels[i]=make(L"STATIC",i?L"ETS2":L"ATS",SS_LEFT,0);
            combos[i]=make(WC_COMBOBOXW,L"",WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST|CBS_HASSTRINGS|CBS_OWNERDRAWFIXED,320+static_cast<int>(i)*2);
            buttons[i]=make(L"BUTTON",L"Delete",WS_TABSTOP,321+static_cast<int>(i)*2);EnableWindow(buttons[i],FALSE);
        }
        refresh();
    }
    void headingFont(HFONT font){SendMessageW(heading,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);}
    void show(bool visible){ShowWindow(heading,visible?SW_SHOW:SW_HIDE);for(size_t i=0;i<2;++i)for(auto h:{labels[i],combos[i],buttons[i]})ShowWindow(h,visible?SW_SHOW:SW_HIDE);if(visible)refresh();}
    void layout(int width,UINT dpi){
        auto place=[dpi](HWND h,int x,int y,int w,int height){MoveWindow(h,MulDiv(x,static_cast<int>(dpi),96),MulDiv(y,static_cast<int>(dpi),96),MulDiv(w,static_cast<int>(dpi),96),MulDiv(height,static_cast<int>(dpi),96),TRUE);};
        const int x=(std::max)(330,width/2);place(heading,x,112,width-44-x,26);
        for(size_t i=0;i<2;++i){const int y=144+static_cast<int>(i)*36;place(labels[i],x,y+4,40,26);place(combos[i],x+42,y,width-44-(x+42)-12-72,220);place(buttons[i],width-116,y,72,28);SendMessageW(combos[i],CB_SETDROPPEDWIDTH,MulDiv(320,static_cast<int>(dpi),96),0);}
    }
    void refresh(){
        const auto protection=protectionReader();const auto active=activeProfile();
        for(size_t i=0;i<2;++i){
            auto next=profileManager::list(root,i!=0);std::string signature;
            for(const auto& entry:next){signature+=entry.id;signature+=profileManager::protectedEntry(entry,protection[i],profileManager::store(root,i!=0)/entry.id,active)?"!":".";}
            if(signature!=signatures[i]||SendMessageW(combos[i],CB_GETCOUNT,0,0)==0){
                // Do not rebuild an open list underneath the user's pointer.
                if(SendMessageW(combos[i],CB_GETDROPPEDSTATE,0,0)){EnableWindow(buttons[i],FALSE);continue;}
                entries[i]=std::move(next);signatures[i]=signature;
                SendMessageW(combos[i],CB_RESETCONTENT,0,0);SendMessageW(combos[i],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Select one"));int index=0;
                for(size_t n=0;n<entries[i].size();++n){const auto& entry=entries[i][n];const bool locked=profileManager::protectedEntry(entry,protection[i],profileManager::store(root,i!=0)/entry.id,active);
                    const auto caption=winText::toWide(entry.name)+(locked?(protection[i].name==entry.name?L" (loaded)":L" (protected)"):L"");
                    const auto added=SendMessageW(combos[i],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(caption.c_str()));
                    if(locked)SendMessageW(combos[i],CB_SETITEMDATA,static_cast<WPARAM>(added),0x47545052);
                    if(selected[i]==entry.id)index=static_cast<int>(n)+1;
                }
                if(index==0)selected[i].clear();SendMessageW(combos[i],CB_SETCURSEL,index,0);
            }
            const auto found=std::find_if(entries[i].begin(),entries[i].end(),[&](const profileManager::Entry& entry){return entry.id==selected[i];});
            EnableWindow(buttons[i],found!=entries[i].end()&&!profileManager::protectedEntry(*found,protection[i],profileManager::store(root,i!=0)/found->id,active));
        }
    }
    bool command(int id,int notification){
        if(id<320||id>323)return false;const size_t game=static_cast<size_t>((id-320)/2);
        if(id%2==0&&notification==CBN_SELCHANGE){const auto index=SendMessageW(combos[game],CB_GETCURSEL,0,0);selected[game]=index>0&&static_cast<size_t>(index)<=entries[game].size()?entries[game][static_cast<size_t>(index)-1].id:std::string();refresh();return true;}
        if(id%2==1&&notification==BN_CLICKED){
            const auto found=std::find_if(entries[game].begin(),entries[game].end(),[&](const profileManager::Entry& entry){return entry.id==selected[game];});if(found==entries[game].end())return true;
            const auto entry=*found;const auto protection=protectionReader();
            if(profileManager::protectedEntry(entry,protection[game],profileManager::store(root,game!=0)/entry.id,activeProfile())){refresh();return true;}
            const auto message=L"Delete the "+std::wstring(game?L"ETS2":L"ATS")+L" app profile ‘"+winText::toWide(entry.name)+L"’?\n\nThis removes its Dispatch Companion settings, history and saved dispatch data from the app. A recoverable backup is kept in deleted_profiles.\n\nYour game profile and game saves are unaffected.";
            if(MessageBoxW(parent,message.c_str(),L"Delete app profile",MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)!=IDYES)return true;
            // Recheck after confirmation: the game may have switched profiles.
            const auto latest=protectionReader();profileManager::archive(root,game!=0,entry,latest[game],activeProfile());selected[game].clear();refresh();return true;
        }
        return false;
    }
};
