#pragma once
#include "update_download.h"
#include <thread>
#include <mutex>

class UpdateControls {
    HWND parent,heading,button,status;
    struct Shared {
        std::mutex mutex;std::atomic<bool> cancel{false};std::atomic<uint64_t> received{0};
        std::optional<updates::Result> result;std::shared_ptr<updates::Download> download;std::string error;bool done=false;
    };
    std::shared_ptr<Shared> shared=std::make_shared<Shared>();
    std::optional<updates::Result> offered;std::shared_ptr<updates::Download> ready;
    bool busy=false,downloading=false;
    HWND make(const wchar_t* kind,const wchar_t* caption,DWORD style,int id){const auto h=CreateWindowExW(0,kind,caption,WS_CHILD|style,0,0,0,0,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);if(!h)throw std::runtime_error("Could not create update controls.");return h;}
public:
    explicit UpdateControls(HWND owner):parent(owner){heading=make(L"STATIC",L"Updates",SS_LEFT,0);button=make(L"BUTTON",L"Check for updates",WS_TABSTOP,306);status=make(L"STATIC",L"Installed: " DISPATCH_VERSION_WIDE L" · Includes test releases",SS_LEFT,0);}
    ~UpdateControls(){shared->cancel=true;}
    bool keepingOpen()const{return busy||offered.has_value()||ready!=nullptr;}
    void headingFont(HFONT font){SendMessageW(heading,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);}
    void show(bool visible){for(auto h:{heading,button,status})ShowWindow(h,visible?SW_SHOW:SW_HIDE);}
    void layout(int width,UINT dpi){const int x=(std::max)(330,width/2);auto place=[dpi](HWND h,int xPos,int y,int w,int height){MoveWindow(h,MulDiv(xPos,static_cast<int>(dpi),96),MulDiv(y,static_cast<int>(dpi),96),MulDiv(w,static_cast<int>(dpi),96),MulDiv(height,static_cast<int>(dpi),96),TRUE);};place(heading,x,252,width-44-x,26);place(button,x,292,180,36);place(status,x,332,width-44-x,36);}
    bool start(bool activePlan=false){
        if(busy){if(downloading){shared->cancel=true;EnableWindow(button,FALSE);SetWindowTextW(status,L"Cancelling download…");}return false;}
        if(ready){
            if(updates::gamesRunning())throw std::runtime_error("Close ATS and ETS2 normally, then press Install update. Your download is ready.");
            const wchar_t* question=activePlan?L"An unfinished delivery plan is recorded. Updating requires closing SplitHaul and will not resume that plan.\n\nClose SplitHaul and start Setup now?":L"SplitHaul will close and start the verified update installer. Windows may ask for administrator permission.\n\nInstall now?";
            if(MessageBoxW(parent,question,L"Install SplitHaul update",MB_YESNO|MB_ICONINFORMATION|MB_DEFBUTTON2)!=IDYES)return false;
            if(updates::launchInstaller(parent,*ready))return true;
            SetWindowTextW(status,L"Installation cancelled. Ready to retry.");return false;
        }
        const auto state=shared;state->cancel=false;state->received=0;busy=true;downloading=offered.has_value();
        try{
            if(offered){
                SetWindowTextW(button,L"Cancel download");SetWindowTextW(status,L"Downloading update…");const auto release=*offered;
                std::thread([state,release]{std::shared_ptr<updates::Download> downloaded;std::string error;try{downloaded=updates::download(release,state->cancel,[state](uint64_t bytes){state->received=bytes;});}catch(const std::exception& ex){error=ex.what();}std::lock_guard<std::mutex> lock(state->mutex);state->download=std::move(downloaded);state->error=error;state->done=true;}).detach();
            }else{
                EnableWindow(button,FALSE);SetWindowTextW(status,L"Checking for updates…");
                std::thread([state]{updates::Result result;try{result=updates::check();}catch(const std::exception& ex){result.state=updates::State::error;result.message=ex.what();}std::lock_guard<std::mutex> lock(state->mutex);state->result=std::move(result);state->done=true;}).detach();
            }
        }catch(...){busy=false;EnableWindow(button,TRUE);SetWindowTextW(button,offered?L"Download update":L"Check for updates");throw;}
        return false;
    }
    void poll(){
        if(!busy)return;
        std::optional<updates::Result> result;std::shared_ptr<updates::Download> downloaded;std::string error;
        {std::lock_guard<std::mutex> lock(shared->mutex);if(!shared->done){if(downloading&&!shared->cancel&&offered){const auto percent=shared->received.load()*100/offered->installerBytes;SetWindowTextW(status,(L"Downloading: "+std::to_wstring(percent)+L"%").c_str());}return;}result=std::move(shared->result);shared->result.reset();downloaded=std::move(shared->download);error=std::move(shared->error);shared->error.clear();shared->done=false;}
        busy=false;EnableWindow(button,TRUE);
        if(downloading){
            downloading=false;
            if(shared->cancel){downloaded.reset();SetWindowTextW(button,L"Download update");SetWindowTextW(status,L"Download cancelled. Ready to retry.");return;}
            if(!downloaded){SetWindowTextW(button,L"Download update");SetWindowTextW(status,L"Download failed. Please retry.");MessageBoxW(parent,winText::toWide(error).c_str(),L"Update download failed",MB_OK|MB_ICONERROR);return;}
            ready=std::move(downloaded);SetWindowTextW(button,L"Install update");SetWindowTextW(status,L"Verified. Close games, then install.");return;
        }
        if(!result)return;
        switch(result->state){
        case updates::State::available:offered=*result;SetWindowTextW(button,L"Download update");SetWindowTextW(status,(L"Available: "+winText::toWide(result->latest)+(result->prerelease?L" (test release)":L"")).c_str());break;
        case updates::State::current:SetWindowTextW(status,L"You have the latest published version.");break;
        case updates::State::ahead:SetWindowTextW(status,(L"Latest published: "+winText::toWide(result->latest)+L". Your build is newer.").c_str());break;
        case updates::State::none:SetWindowTextW(status,L"No verified Windows installer is available.");break;
        case updates::State::error:SetWindowTextW(status,L"Could not check. Please try again.");MessageBoxW(parent,winText::toWide(result->message).c_str(),L"Update check failed",MB_OK|MB_ICONERROR);break;
        }
    }
};
