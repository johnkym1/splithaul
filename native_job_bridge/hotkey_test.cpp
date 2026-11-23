// Integration host for the actual launcher/listener, without game memory access.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string>
static std::string lastLog;
static void log(const char* value){lastLog=value;}
#include "app_hotkey.h"
static HWND details=nullptr;
static LRESULT CALLBACK procedure(HWND hwnd,UINT message,WPARAM wp,LPARAM lp){
    switch(message){
    case WM_CREATE:
        details=CreateWindowW(L"STATIC",L"Starting listener...",WS_CHILD|WS_VISIBLE,20,20,720,380,hwnd,nullptr,GetModuleHandleW(nullptr),nullptr);
        appHotkey::initialize();SetTimer(hwnd,1,100,nullptr);return 0;
    case WM_TIMER:{
        appHotkey::poll();const auto status=appHotkey::status().dump(2);const std::string value="Shift+F9 should open or focus Dispatch Companion.\r\nThis host does not read or change game memory.\r\n\r\n"+status+"\r\n\r\n"+lastLog;
        const auto count=MultiByteToWideChar(CP_UTF8,0,value.data(),static_cast<int>(value.size()),nullptr,0);std::wstring wide(static_cast<size_t>(count),0);MultiByteToWideChar(CP_UTF8,0,value.data(),static_cast<int>(value.size()),wide.data(),count);SetWindowTextW(details,wide.c_str());return 0;
    }
    case WM_CLOSE:{
        auto status=appHotkey::status();status["last_log"]=lastLog;
        wchar_t name[32768];GetModuleFileNameW(nullptr,name,32768);const auto root=std::filesystem::path(name).parent_path().parent_path().parent_path();
        try{live::writeJson(root/"research/shift-f9-20261121/harness-result.json",status);}catch(...){}
        appHotkey::shutdown();KillTimer(hwnd,1);DestroyWindow(hwnd);return 0;
    }
    case WM_DESTROY:PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(hwnd,message,wp,lp);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){
    WNDCLASSW wc={};wc.hInstance=instance;wc.lpfnWndProc=procedure;wc.lpszClassName=L"GTShortcutIntegrationHost";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);RegisterClassW(&wc);
    auto window=CreateWindowW(wc.lpszClassName,L"GT shortcut integration test",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,800,480,nullptr,nullptr,instance,nullptr);
    if(!window)return 1;ShowWindow(window,show);SetForegroundWindow(window);MSG message={};while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}return 0;
}
