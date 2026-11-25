#pragma once
#include "update_check.h"
#include "win_text.h"
#include <thread>
#include <mutex>
#include <memory>
#include <shellapi.h>

class UpdateControls {
    HWND parent,heading,button,status;
    struct Shared {std::mutex mutex;std::optional<updates::Result> result;};
    std::shared_ptr<Shared> shared=std::make_shared<Shared>();bool busy=false;
    HWND make(const wchar_t* kind,const wchar_t* caption,DWORD style,int id){const auto h=CreateWindowExW(0,kind,caption,WS_CHILD|style,0,0,0,0,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);if(!h)throw std::runtime_error("Could not create update controls.");return h;}
public:
    explicit UpdateControls(HWND owner):parent(owner){heading=make(L"STATIC",L"Updates",SS_LEFT,0);button=make(L"BUTTON",L"Check for updates",WS_TABSTOP,306);status=make(L"STATIC",L"Installed: " DISPATCH_VERSION_WIDE L" · Includes test releases",SS_LEFT,0);}
    void headingFont(HFONT font){SendMessageW(heading,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);}
    void show(bool visible){for(auto h:{heading,button,status})ShowWindow(h,visible?SW_SHOW:SW_HIDE);}
    void layout(int width,UINT dpi){const int x=(std::max)(330,width/2);auto place=[dpi](HWND h,int xPos,int y,int w,int height){MoveWindow(h,MulDiv(xPos,static_cast<int>(dpi),96),MulDiv(y,static_cast<int>(dpi),96),MulDiv(w,static_cast<int>(dpi),96),MulDiv(height,static_cast<int>(dpi),96),TRUE);};place(heading,x,252,width-44-x,26);place(button,x,292,180,36);place(status,x,332,width-44-x,24);}
    void start(){
        if(busy)return;busy=true;EnableWindow(button,FALSE);SetWindowTextW(status,L"Checking GitHub…");
        try{const auto state=shared;std::thread([state]{updates::Result result;try{result=updates::check();}catch(const std::exception& e){result.state=updates::State::error;result.message=e.what();}std::lock_guard<std::mutex> lock(state->mutex);state->result=std::move(result);}).detach();}
        catch(...){busy=false;EnableWindow(button,TRUE);SetWindowTextW(status,L"Update check could not start.");throw;}
    }
    void poll(){
        if(!busy)return;std::optional<updates::Result> result;{std::lock_guard<std::mutex> lock(shared->mutex);result=std::move(shared->result);shared->result.reset();}if(!result)return;
        busy=false;EnableWindow(button,TRUE);std::wstring message;const auto latest=winText::toWide(result->latest);
        switch(result->state){
        case updates::State::available:{SetWindowTextW(status,(L"Available: "+latest+(result->prerelease?L" (test release)":L"")).c_str());message=L"Dispatch Companion "+latest+(result->prerelease?L" (test release)":L"")+L" is available.\nYou have " DISPATCH_VERSION_WIDE L".\n\nOpen the GitHub release page to read the changes and download the installer?";if(MessageBoxW(parent,message.c_str(),L"Update available",MB_YESNO|MB_ICONINFORMATION)==IDYES&&reinterpret_cast<INT_PTR>(ShellExecuteW(parent,L"open",winText::toWide(result->url).c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)MessageBoxW(parent,L"Could not open your browser. Visit github.com/johnkym1/dispatch-companion/releases.",L"Dispatch Companion",MB_OK|MB_ICONERROR);break;}
        case updates::State::current:SetWindowTextW(status,L"You have the latest published version.");MessageBoxW(parent,L"You have the latest published Windows version: " DISPATCH_VERSION_WIDE L".",L"No update available",MB_OK|MB_ICONINFORMATION);break;
        case updates::State::ahead:SetWindowTextW(status,(L"Latest published: "+latest).c_str());message=L"You have " DISPATCH_VERSION_WIDE L".\nThe latest published Windows version is "+latest+L".\n\nYour installed build is newer; no update is needed.";MessageBoxW(parent,message.c_str(),L"No update needed",MB_OK|MB_ICONINFORMATION);break;
        case updates::State::none:SetWindowTextW(status,L"No published Windows installer is available.");MessageBoxW(parent,L"No published Windows installer with a checksum is available on GitHub yet.",L"Dispatch Companion updates",MB_OK|MB_ICONINFORMATION);break;
        case updates::State::error:SetWindowTextW(status,L"Could not check. Please try again.");MessageBoxW(parent,winText::toWide(result->message).c_str(),L"Update check failed",MB_OK|MB_ICONERROR);break;
        }
    }
};
