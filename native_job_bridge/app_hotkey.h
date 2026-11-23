#pragma once
#include "../dispatch_shared/app_launcher.h"
#include <thread>
#include <atomic>
#pragma comment(lib,"user32.lib")

namespace appHotkey {
static std::thread worker;
static std::thread listener;
static HANDLE stopEvent=nullptr;
static std::atomic<bool> busy=false;
static std::atomic<DWORD> notice=0;
static std::atomic<unsigned> lastStage=0;
static std::atomic<DWORD> lastError=ERROR_SUCCESS;
static std::atomic<unsigned> presses=0;
static std::atomic<unsigned> hotkeyEvents=0,rawChords=0,inputSource=0;
static std::atomic<bool> registeredHotkey=false,listenerRunning=false,foregroundConfirmed=false,minimizedGame=false;
static std::atomic<DWORD> registrationError=0;
static ATOM hotkeyId=0;
static std::atomic<DWORD> listenerThreadId=0;
static constexpr UINT recoveredMessage=WM_APP+92;
static ULONGLONG lastLaunch=0;
static DWORD foregroundProcess() {DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);return pid;}
static bool waitForForeground(DWORD pid,unsigned timeout) {
    const auto until=GetTickCount64()+timeout;
    do{if(foregroundProcess()==pid)return true;if(WaitForSingleObject(stopEvent,25)==WAIT_OBJECT_0)return false;}while(GetTickCount64()<until);
    return false;
}
struct Search {dispatchLauncher::Paths paths;uintptr_t project=0;HWND found=nullptr;};
static BOOL CALLBACK findWindow(HWND hwnd,LPARAM param) {
    auto& search=*reinterpret_cast<Search*>(param);
    if(reinterpret_cast<uintptr_t>(GetPropW(hwnd,dispatchLauncher::windowProperty))!=search.project)return TRUE;
    DWORD pid=0;GetWindowThreadProcessId(hwnd,&pid);HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!process)return TRUE;
    wchar_t path[32768];DWORD count=32768;const bool same=QueryFullProcessImageNameW(process,0,path,&count)&&_wcsicmp(path,search.paths.exe.c_str())==0;CloseHandle(process);
    if(same){search.found=hwnd;return FALSE;}return TRUE;
}
static void openApp(bool explicitShortcut) {
    DWORD error=ERROR_SUCCESS;
    foregroundConfirmed.store(false);minimizedGame.store(false);
    try {
        Search search;
        lastStage.store(1);
        if(!dispatchLauncher::registered(search.paths))error=ERROR_FILE_NOT_FOUND;
        else {
            lastStage.store(2);
            search.project=dispatchLauncher::projectKey(search.paths.project);
            EnumWindows(findWindow,reinterpret_cast<LPARAM>(&search));
            DWORD appPid=0;
            if(search.found){GetWindowThreadProcessId(search.found,&appPid);AllowSetForegroundWindow(appPid);ShowWindowAsync(search.found,SW_RESTORE);SetForegroundWindow(search.found);PostMessageW(search.found,dispatchLauncher::showMessage,0,0);}
            else {
                lastStage.store(3);
                STARTUPINFOW startup={};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_SHOWNORMAL;PROCESS_INFORMATION process={};
                auto command=L"\""+search.paths.exe.wstring()+L"\"";
                if(CreateProcessW(search.paths.exe.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,search.paths.project.c_str(),&startup,&process)){appPid=process.dwProcessId;AllowSetForegroundWindow(appPid);CloseHandle(process.hThread);CloseHandle(process.hProcess);}
                else error=GetLastError();
            }
            if(!error&&appPid) {
                bool focused=waitForForeground(appPid,search.found?750:5000);
                if(!focused&&explicitShortcut&&foregroundProcess()==GetCurrentProcessId()&&WaitForSingleObject(stopEvent,0)!=WAIT_OBJECT_0) {
                    // Yield exclusive-fullscreen focus from our own game window.
                    // This is limited to the user's explicit shortcut request.
                    const auto game=GetForegroundWindow();ShowWindowAsync(game,SW_MINIMIZE);minimizedGame.store(true);AllowSetForegroundWindow(appPid);
                    if(!search.found)EnumWindows(findWindow,reinterpret_cast<LPARAM>(&search));
                    if(search.found){ShowWindowAsync(search.found,SW_RESTORE);SetForegroundWindow(search.found);PostMessageW(search.found,dispatchLauncher::showMessage,0,0);}
                    focused=waitForForeground(appPid,2000);
                }
                foregroundConfirmed.store(focused);if(!focused)error=ERROR_TIMEOUT;
            }
        }
    }catch(...){lastStage.store(4);error=ERROR_GEN_FAILURE;}
    lastError.store(error);notice.store(error+1);busy.store(false);
}
static void updateRegistration() {
    const bool active=foregroundProcess()==GetCurrentProcessId();
    if(!active&&registeredHotkey.load()){UnregisterHotKey(nullptr,hotkeyId);registeredHotkey.store(false);}
    else if(active&&!registeredHotkey.load()&&hotkeyId){
        if(RegisterHotKey(nullptr,hotkeyId,dispatchLauncher::shortcutModifiers,dispatchLauncher::shortcutKey)){registeredHotkey.store(true);registrationError.store(0);}
        else registrationError.store(GetLastError());
    }
}
static void CALLBACK foregroundChanged(HWINEVENTHOOK,DWORD,HWND,LONG,LONG,DWORD,DWORD) {updateRegistration();}
static void requestOpen(unsigned source) {
    const auto now=GetTickCount64();
    if((source!=3&&foregroundProcess()!=GetCurrentProcessId())||busy.load()||now-lastLaunch<=750)return;
    if(worker.joinable())worker.join();lastLaunch=now;++presses;inputSource.store(source);lastError.store(ERROR_IO_PENDING);busy.store(true);
    try{worker=std::thread(openApp,source!=3);}catch(...){lastError.store(ERROR_NOT_ENOUGH_MEMORY);busy.store(false);notice.store(ERROR_NOT_ENOUGH_MEMORY+1);}
}
static bool requestRecovered() {const auto id=listenerThreadId.load();return id&&PostThreadMessageW(id,recoveredMessage,0,0)!=FALSE;}
static void listen() {
    listenerRunning.store(true);MSG message={};PeekMessageW(&message,nullptr,0,0,PM_NOREMOVE);listenerThreadId.store(GetCurrentThreadId());
    hotkeyId=GlobalAddAtomW(dispatchLauncher::shortcutAtom);
    if(!hotkeyId)registrationError.store(GetLastError());
    const auto hook=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,foregroundChanged,0,0,WINEVENT_OUTOFCONTEXT);
    updateRegistration();
    bool wasDown=false;
    for(;;){
        const auto ready=MsgWaitForMultipleObjects(1,&stopEvent,FALSE,20,QS_ALLINPUT);
        if(ready==WAIT_OBJECT_0)break;if(ready==WAIT_FAILED){registrationError.store(GetLastError());break;}
        while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
            if(message.message==WM_HOTKEY&&message.wParam==hotkeyId&&HIWORD(message.lParam)==dispatchLauncher::shortcutKey&&dispatchLauncher::shortcutChord(true,LOWORD(message.lParam))){++hotkeyEvents;requestOpen(1);}
            else if(message.message==recoveredMessage)requestOpen(3);
            TranslateMessage(&message);DispatchMessageW(&message);
        }
        updateRegistration();
        // Exclusive game input can suppress WM_HOTKEY delivery. Sample only
        // this chord on the listener, independently of rendering/SDK frames.
        unsigned modifiers=0;
        if(GetAsyncKeyState(VK_SHIFT)&0x8000)modifiers|=MOD_SHIFT;
        if(GetAsyncKeyState(VK_CONTROL)&0x8000)modifiers|=MOD_CONTROL;
        if(GetAsyncKeyState(VK_MENU)&0x8000)modifiers|=MOD_ALT;
        if((GetAsyncKeyState(VK_LWIN)|GetAsyncKeyState(VK_RWIN))&0x8000)modifiers|=MOD_WIN;
        const bool down=dispatchLauncher::shortcutChord((GetAsyncKeyState(dispatchLauncher::shortcutKey)&0x8000)!=0,modifiers);
        if(down&&!wasDown&&foregroundProcess()==GetCurrentProcessId()){++rawChords;requestOpen(2);}wasDown=down;
    }
    if(registeredHotkey.exchange(false))UnregisterHotKey(nullptr,hotkeyId);
    if(hook)UnhookWinEvent(hook);if(hotkeyId)GlobalDeleteAtom(hotkeyId);hotkeyId=0;listenerThreadId.store(0);listenerRunning.store(false);
}
static bool initialize() {
    if(listener.joinable())return true;
    presses.store(0);hotkeyEvents.store(0);rawChords.store(0);inputSource.store(0);lastLaunch=0;notice.store(0);lastError.store(0);lastStage.store(0);registrationError.store(0);foregroundConfirmed.store(false);minimizedGame.store(false);
    stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!stopEvent)return false;
    try{listener=std::thread(listen);return true;}catch(...){CloseHandle(stopEvent);stopEvent=nullptr;return false;}
}
static void poll() {
    const auto reported=notice.exchange(0);if(reported){
        if(reported==1)log("[GT_TELEMETRY] Shift+F9: Dispatch Companion foreground confirmed.");
        else {const auto message=std::string("[GT_TELEMETRY] Shift+F9 launcher failed: Windows error ")+std::to_string(reported-1)+", stage "+std::to_string(lastStage.load())+" (1=launcher.json, 2=window search, 3=process creation, 4=exception). Open Launch Dispatch.cmd once and retry.";log(message.c_str());}
    }
}
static void shutdown() {if(stopEvent)SetEvent(stopEvent);if(listener.joinable())listener.join();if(worker.joinable())worker.join();if(stopEvent){CloseHandle(stopEvent);stopEvent=nullptr;}}
static live::Json status() {return {{"shortcut",dispatchLauncher::shortcutLabel},{"presses",presses.load()},{"windows_hotkey_events",hotkeyEvents.load()},{"raw_chords",rawChords.load()},{"last_input",inputSource.load()==1?"windows_hotkey":inputSource.load()==2?"key_state_backup":"none"},{"foreground_pid",foregroundProcess()},{"busy",busy.load()},{"stage",lastStage.load()},{"windows_error",lastError.load()},{"listener_running",listenerRunning.load()},{"registered",registeredHotkey.load()},{"registration_error",registrationError.load()},{"foreground_confirmed",foregroundConfirmed.load()},{"game_minimized",minimizedGame.load()}};}
}
