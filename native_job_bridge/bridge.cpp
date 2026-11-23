// Exact-build live-offer experiment, restricted to the user's requested route.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include "scssdk_telemetry.h"
#include "native_arguments.h"
#pragma comment(lib,"bcrypt.lib")

static scs_log_t gameLog = nullptr;
static uintptr_t imageBase = 0;
static std::wstring folder;
static DWORD callbackThread = 0;
static ULONGLONG lastPoll = 0, lastSpeed = 0;
static bool driving = false, disabled = false, usageDone = false;
static bool jobSeen=false, carSeen=false, busSeen=false;
static bool jobActive=false, carActive=false, busActive=false;
static bool offerAttempted=false;
static float speed = 0;
static std::string lastState;
static constexpr uintptr_t registryRva=0x3719dd0, economyRva=0x30afe60, handlerRva=0x5bebc0;
static constexpr char expectedHash[]="28ca69a7eca99519371adcc3e78b978946ad28ce57332780e9fe4175be92f6a7";

static void log(const char* text) {
    if (gameLog) gameLog(SCS_LOG_TYPE_message,text);
}
static bool readMemory(uintptr_t address,void* out,size_t size) {
    SIZE_T count=0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),out,size,&count) && count==size;
}
static bool word(uintptr_t address,uintptr_t& value) { return readMemory(address,&value,sizeof(value)); }
static bool exactHost() {
    wchar_t name[32768];
    if(!GetModuleFileNameW(nullptr,name,32768)) return false;
    HANDLE file=CreateFileW(name,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
    if(file==INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE alg=nullptr; BCRYPT_HASH_HANDLE hash=nullptr;
    bool ok=BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;
    if(ok) ok=BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0)>=0;
    unsigned char bytes[65536],digest[32]; DWORD count=0;
    while(ok) {
        if(!ReadFile(file,bytes,sizeof(bytes),&count,nullptr)){ok=false;break;}
        if(!count)break;
        ok=BCryptHashData(hash,bytes,count,0)>=0;
    }
    if(ok)ok=BCryptFinishHash(hash,digest,sizeof(digest),0)>=0;
    if(hash)BCryptDestroyHash(hash);
    if(alg)BCryptCloseAlgorithmProvider(alg,0);
    CloseHandle(file);
    char hex[65]={};
    if(ok) for(int i=0;i<32;i++)sprintf_s(hex+i*2,3,"%02x",digest[i]);
    return ok && strcmp(hex,expectedHash)==0;
}
static void status(const char* state) {
    if(lastState==state)return;
    lastState=state;
    char data[768];
    sprintf_s(data,"{\n  \"stage\": \"fixed_route_offer_test\",\n  \"job_creation_enabled\": true,\n  \"route\": \"empty_palet: bn_farm.yakima -> sg_whs.wenatchee\",\n  \"state\": \"%s\",\n  \"pid\": %lu,\n  \"callback_thread\": %lu,\n  \"native_call_attempted\": %s,\n  \"offer_creation_confirmed\": false\n}\n",state,GetCurrentProcessId(),callbackThread,usageDone?"true":"false");
    std::wstring temp=folder+L"\\status.tmp", dest=folder+L"\\status.json";
    HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file!=INVALID_HANDLE_VALUE){DWORD count;WriteFile(file,data,static_cast<DWORD>(strlen(data)),&count,nullptr);CloseHandle(file);MoveFileExW(temp.c_str(),dest.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);}
    char message[256];sprintf_s(message,"[MSDC_NATIVE_PROBE] %s",state);log(message);
}
static bool registryReady(uintptr_t& registry) {
    uintptr_t economy=0,entries=0,count=0;
    if(!word(imageBase+registryRva,registry)||!registry||!word(imageBase+economyRva,economy)||!economy)return false;
    uintptr_t vtable=0;
    if(!word(economy,vtable)||vtable<imageBase||vtable>=imageBase+0x4000000)return false;
    if(!word(registry+8,entries)||!word(registry+16,count)||!entries||count>512)return false;
    for(size_t i=0;i<count;i++) {
        uintptr_t name=0,function=0;char text[4]={};
        if(!word(entries+i*0xa8+8,name)||!word(entries+i*0xa8+0xa0,function))return false;
        if(function==imageBase+handlerRva && readMemory(name,text,4) && memcmp(text,"job\0",4)==0)return true;
    }
    return false;
}
static bool handlerMatches() {
    const unsigned char expected[]={0x48,0x89,0x5c,0x24,0x08,0x55,0x48,0x8d,0x6c,0x24,0xa9,0x48,0x81,0xec,0xa0,0,0,0};
    unsigned char actual[sizeof(expected)];
    return readMemory(imageBase+handlerRva,actual,sizeof(actual)) && memcmp(actual,expected,sizeof(expected))==0;
}
// Proven usage branch reads count at +0x20 and returns before parsing strings
// when it is below three. No caller-provided cargo/companies can reach this call.
static bool callUsageOnly(uintptr_t registry) {
    alignas(16) unsigned char emptyArguments[64]={};
    using Handler=bool(__fastcall*)(void*,void*);
    __try { return reinterpret_cast<Handler>(imageBase+handlerRva)(reinterpret_cast<void*>(registry),emptyArguments); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
// Fixed negative test: empty source/target prevent reaching a mutating branch,
// even if a mod unexpectedly defines the sentinel cargo. No external argument
// content is accepted by this diagnostic build.
static bool callArgumentProbe(uintptr_t registry) {
    NativeStringInput tokens[]={borrowedInput("cheat"),borrowedInput("job"),borrowedInput("msdc_probe"),borrowedInput(""),borrowedInput("")};
    NativeParsedInput args={};args.data=tokens;args.count=5;
    using Handler=bool(__fastcall*)(void*,void*);
    __try { return reinterpret_cast<Handler>(imageBase+handlerRva)(reinterpret_cast<void*>(registry),&args); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
// Same input layout as the diagnostic probe, now with the exact authorized route.
// Handler return means completion of the request, NOT successful offer creation.
static bool callRequestedOffer(uintptr_t registry) {
    NativeStringInput tokens[]={borrowedInput("cheat"),borrowedInput("job"),borrowedInput("empty_palet"),borrowedInput("bn_farm.yakima"),borrowedInput("sg_whs.wenatchee")};
    NativeParsedInput args={};args.data=tokens;args.count=5;
    using Handler=bool(__fastcall*)(void*,void*);
    __try { return reinterpret_cast<Handler>(imageBase+handlerRva)(reinterpret_cast<void*>(registry),&args); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
SCSAPI_VOID onSpeed(const scs_string_t,const scs_u32_t,const scs_value_t* value,const scs_context_t) {
    if(value && value->type==SCS_VALUE_TYPE_float){speed=value->value_float.value;lastSpeed=GetTickCount64();}
    else lastSpeed=0;
}
SCSAPI_VOID onEvent(const scs_event_t event,const void* info,const scs_context_t) {
    if(event==SCS_TELEMETRY_EVENT_configuration && info){
        auto config=static_cast<const scs_telemetry_configuration_t*>(info);
        if(!config->id)return;
        bool active=config->attributes && config->attributes[0].name;
        if(strcmp(config->id,"job")==0){jobSeen=true;jobActive=active;}
        if(strcmp(config->id,"car_job")==0){carSeen=true;carActive=active;}
        if(strcmp(config->id,"bus_job")==0){busSeen=true;busActive=active;}
        return;
    }
    if(event==SCS_TELEMETRY_EVENT_started){driving=true;lastSpeed=0;return;}
    if(event==SCS_TELEMETRY_EVENT_paused){driving=false;lastSpeed=0;return;}
    if(event!=SCS_TELEMETRY_EVENT_frame_end||disabled)return;
    if(callbackThread!=GetCurrentThreadId()){disabled=true;status("disabled_thread_changed");return;}
    ULONGLONG now=GetTickCount64();if(now-lastPoll<1000)return;lastPoll=now;
    uintptr_t registry=0;
    if(!handlerMatches()){disabled=true;status("disabled_handler_changed");return;}
    if(!registryReady(registry)){status("waiting_for_native_objects");return;}
    if(usageDone){status(offerAttempted?"offer_handler_returned_unverified":"usage_probe_returned_check_game_log");return;}
    std::wstring request=folder+L"\\request.txt";
    HANDLE file=CreateFileW(request.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE){status("native_handler_found_waiting_request");return;}
    char text[64]={};DWORD count=0;BOOL ok=ReadFile(file,text,sizeof(text)-1,&count,nullptr);CloseHandle(file);
    if(!ok)return;
    while(count && (text[count-1]=='\r'||text[count-1]=='\n'))text[--count]=0;
    bool argumentProbe=strcmp(text,"probe_arguments")==0;
    bool createOffer=strcmp(text,"create_yakima_wenatchee_pallets")==0;
    if(!createOffer && !argumentProbe && strcmp(text,"probe_usage")!=0){status("request_rejected_unknown_operation");return;}
    if(!driving || !lastSpeed || now-lastSpeed>2000 || !std::isfinite(speed) || fabsf(speed)>0.1f){status("usage_probe_waiting_for_stationary_driving");return;}
    if(createOffer){
        if(!jobSeen||!carSeen||!busSeen){status("offer_waiting_for_all_job_configurations");return;}
        if(jobActive||carActive||busActive){status("offer_blocked_active_job");return;}
    }
    // Claim the request BEFORE calling, so an exception/reload cannot repeat it.
    if(!MoveFileExW(request.c_str(),(folder+L"\\request.consumed").c_str(),MOVEFILE_REPLACE_EXISTING)){status("request_claim_failed");return;}
    usageDone=true;
    offerAttempted=createOffer;
    log(createOffer?"[MSDC_NATIVE_PROBE] REQUEST empty_palet: bn_farm.yakima -> sg_whs.wenatchee. Native offer creation; result requires verification.":argumentProbe?"[MSDC_NATIVE_PROBE] Calling fixed ARGUMENT probe; source and target are empty, no job can be created.":"[MSDC_NATIVE_PROBE] Calling native job USAGE branch only; cargo/source/target are absent.");
    bool result=createOffer?callRequestedOffer(registry):argumentProbe?callArgumentProbe(registry):callUsageOnly(registry);
    if(!result){disabled=true;status("disabled_usage_probe_failed");return;}
    status(createOffer?"offer_handler_returned_unverified":"usage_probe_returned_check_game_log");
}
SCSAPI_RESULT scs_telemetry_init(const scs_u32_t version,const scs_telemetry_init_params_t* params) {
    if(version!=SCS_TELEMETRY_VERSION_1_01)return SCS_RESULT_unsupported;
    if(!params)return SCS_RESULT_invalid_parameter;
    auto p=static_cast<const scs_telemetry_init_params_v101_t*>(params);
    gameLog=p->common.log;
    if(!p->common.game_id || strcmp(p->common.game_id,"ats")!=0)return SCS_RESULT_unsupported;
    if(!exactHost()){log("[MSDC_NATIVE_PROBE] Refused: executable SHA-256 does not match tested ATS build.");return SCS_RESULT_generic_error;}
    imageBase=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if(!handlerMatches()){log("[MSDC_NATIVE_PROBE] Refused: handler bytes mismatch.");return SCS_RESULT_generic_error;}
    wchar_t local[32768];DWORD size=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
    if(!size||size>=32768)return SCS_RESULT_generic_error;
    folder=std::wstring(local)+L"\\MSDCNativeJobBridge";
    if(!CreateDirectoryW(folder.c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS)return SCS_RESULT_generic_error;
    callbackThread=GetCurrentThreadId();lastPoll=lastSpeed=0;driving=disabled=usageDone=false;lastState.clear();
    jobSeen=carSeen=busSeen=jobActive=carActive=busActive=offerAttempted=false;
    if(!p->register_for_event||!p->unregister_from_event||!p->register_for_channel)return SCS_RESULT_invalid_parameter;
    const scs_event_t events[]={SCS_TELEMETRY_EVENT_started,SCS_TELEMETRY_EVENT_paused,SCS_TELEMETRY_EVENT_frame_end,SCS_TELEMETRY_EVENT_configuration};
    int registered=0;
    for(auto event:events){
        if(p->register_for_event(event,onEvent,nullptr)!=SCS_RESULT_ok){
            for(int i=0;i<registered;i++)p->unregister_from_event(events[i]);
            return SCS_RESULT_generic_error;
        }
        ++registered;
    }
    if(p->register_for_channel("truck.speed",SCS_U32_NIL,SCS_VALUE_TYPE_float,SCS_TELEMETRY_CHANNEL_FLAG_each_frame|SCS_TELEMETRY_CHANNEL_FLAG_no_value,onSpeed,nullptr)!=SCS_RESULT_ok){
        for(auto event:events)p->unregister_from_event(event);
        return SCS_RESULT_generic_error;
    }
    status("loaded_fixed_route_offer_test");return SCS_RESULT_ok;
}
SCSAPI_VOID scs_telemetry_shutdown() {
    if(!folder.empty())status("unloaded");gameLog=nullptr;disabled=true;
}
