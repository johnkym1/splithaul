// Exact-build, one-shot owned-trailer assignment experiment.
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
#include "assignment_state.h"
#pragma comment(lib,"bcrypt.lib")

static scs_log_t gameLog = nullptr;
static uintptr_t imageBase = 0;
static std::wstring folder;
static DWORD callbackThread = 0;
static ULONGLONG lastPoll = 0, lastSpeed = 0;
static bool driving = false, disabled = false, usageDone = false;
static bool jobSeen=false, carSeen=false, busSeen=false;
static bool jobActive=false, carActive=false, busActive=false;
static bool offerAttempted=false, assignmentAttempted=false, awaitingTelemetry=false;
static bool trailerConnected=false;
static ULONGLONG trailerTime=0, verificationDeadline=0;
static AssignmentTelemetry jobTelemetry;
static unsigned jobRevisionBefore=0;
static constexpr uintptr_t acceptRva=0x7891a0, lookupRva=0x12d6f0;
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
    char data[1024];
    sprintf_s(data,"{\n  \"stage\": \"automatic_assignment_test\",\n  \"route\": \"empty_palet: bn_farm.yakima -> sg_whs.wenatchee\",\n  \"state\": \"%s\",\n  \"pid\": %lu,\n  \"callback_thread\": %lu,\n  \"offer_call_attempted\": %s,\n  \"assignment_call_attempted\": %s,\n  \"assignment_confirmed_unloaded\": %s\n}\n",state,GetCurrentProcessId(),callbackThread,offerAttempted?"true":"false",assignmentAttempted?"true":"false",assignmentAttempted&&jobTelemetry.confirmedUnloaded(jobRevisionBefore)?"true":"false");
    std::wstring temp=folder+L"\\status.tmp", dest=folder+L"\\status.json";
    HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file!=INVALID_HANDLE_VALUE){DWORD count;WriteFile(file,data,static_cast<DWORD>(strlen(data)),&count,nullptr);CloseHandle(file);MoveFileExW(temp.c_str(),dest.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);}
    char message[256];sprintf_s(message,"[MSDC_AUTO_DISPATCH] %s",state);log(message);
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

static bool assignmentCodeMatches() {
    { const unsigned char expected[]={0x48,0x8b,0xc4,0x55,0x41,0x54,0x41,0x55,0x41,0x57,0x48,0x8d,0xa8,0x68,0xfe,0xff}; unsigned char actual[sizeof(expected)];
      if(!readMemory(imageBase+acceptRva,actual,sizeof(actual)) || memcmp(actual,expected,sizeof(expected)))return false; }
    { const unsigned char expected[]={0x40,0x56,0x48,0x81,0xec,0x90,0x00,0x00,0x00,0x48,0x8d,0x44,0x24,0x40,0x48,0xc7}; unsigned char actual[sizeof(expected)];
      if(!readMemory(imageBase+lookupRva,actual,sizeof(actual)) || memcmp(actual,expected,sizeof(expected)))return false; }
    return handlerMatches();
}
static uintptr_t lookupUnit(const char* name) {
    using Lookup=uintptr_t(__fastcall*)(const char* const*);
    __try { return reinterpret_cast<Lookup>(imageBase+lookupRva)(&name); }
    __except(EXCEPTION_EXECUTE_HANDLER) { disabled=true;return 0; }
}
static bool liveUnit(uintptr_t pointer) {
    uint32_t flags=0;return pointer && readMemory(pointer+8,&flags,sizeof(flags)) && (flags&0x80000000u);
}
struct AssignmentObjects { uintptr_t economy=0,player=0,source=0,target=0,cargo=0,trailer=0; };
static const char* findObjects(AssignmentObjects& o) {
    uintptr_t vehicles=0,ownedTrailer=0,actorRoot=0,actor=0,vtable=0;
    if(!word(imageBase+economyRva,o.economy)||!liveUnit(o.economy)||!word(o.economy+0x18,o.player)||!liveUnit(o.player))return "blocked_economy_or_player_missing";
    // Player native job references, independently of telemetry configuration.
    for(uintptr_t off=0x28;off<=0x38;off+=8){uintptr_t job=0;if(!word(o.player+off,job)||job)return "blocked_native_job_reference_present";}
    uint32_t vehicleMode=0;
    if(!readMemory(o.player+0x98,&vehicleMode,4)||vehicleMode!=0)return "blocked_not_truck_mode";
    if(!word(o.player+0xa0,vehicles)||!liveUnit(vehicles)||!word(vehicles+0x48,ownedTrailer)||!liveUnit(ownedTrailer)||!word(ownedTrailer+0x1a0,o.trailer)||!liveUnit(o.trailer))return "blocked_owned_trailer_missing";
    if(!word(imageBase+0x3719ca8,actorRoot)||!actorRoot||!word(actorRoot+0x31b0,actor)||!liveUnit(actor))return "blocked_actor_missing";
    o.source=lookupUnit("company.volatile.bn_farm.yakima");
    o.target=lookupUnit("company.volatile.sg_whs.wenatchee");
    o.cargo=lookupUnit("cargo.empty_palet");
    if(!liveUnit(o.source)||!liveUnit(o.target)||!liveUnit(o.cargo))return "blocked_route_units_missing";
    for(auto company:{o.source,o.target}) {
        uintptr_t prefab=0;
        if(!word(company,vtable)||vtable!=imageBase+0x2300988 || !word(company+0x10,prefab)||!prefab)return "blocked_company_prefab_missing";
    }
    return nullptr;
}
static bool stringEquals(uintptr_t stringObject,const char* text) {
    uintptr_t pointer=0;uint32_t length=0;char actual[96]={};size_t size=strlen(text);
    return size<sizeof(actual) && word(stringObject+8,pointer) && readMemory(stringObject+0x10,&length,4) && length==size && readMemory(pointer,actual,size+1) && memcmp(actual,text,size+1)==0;
}
// Refuse absent, expired, duplicate or incorrectly typed candidates. The cargo
// array at +0xa0 is distinct from the freight array at +0x78.
static uintptr_t matchingOffer(const AssignmentObjects& o) {
    uintptr_t items=0,count=0,selected=0;uint32_t clock=0;
    if(!word(o.source+0xa8,items)||!word(o.source+0xb0,count)||count>64||!items||!readMemory(o.economy+0x19c,&clock,4))return 0;
    for(uintptr_t i=0;i<count;i++) {
        uintptr_t candidate=0,vtable=0,cargo=0;uint32_t expiry=0;
        if(!word(items+i*8,candidate)||!liveUnit(candidate)||!word(candidate,vtable)||vtable!=imageBase+0x2352698)return 0;
        if(!word(candidate+0x58,cargo)||!readMemory(candidate+0x48,&expiry,4))return 0;
        if(cargo!=o.cargo || expiry<=clock || !stringEquals(candidate+0x10,"sg_whs.wenatchee"))continue;
        if(selected)return 0;
        selected=candidate;
    }
    return selected;
}
static bool createOffer(uintptr_t registry) {
    NativeStringInput tokens[]={borrowedInput("cheat"),borrowedInput("job"),borrowedInput("empty_palet"),borrowedInput("bn_farm.yakima"),borrowedInput("sg_whs.wenatchee")};
    NativeParsedInput args={};args.data=tokens;args.count=5;
    using Handler=bool(__fastcall*)(void*,void*);
    __try { return reinterpret_cast<Handler>(imageBase+handlerRva)(reinterpret_cast<void*>(registry),&args); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool acceptOffer(uintptr_t economy,uintptr_t company,uintptr_t offer) {
    using Accept=unsigned char(__fastcall*)(uintptr_t,uintptr_t,uintptr_t);
    __try { return reinterpret_cast<Accept>(imageBase+acceptRva)(economy,company,offer)!=0; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
SCSAPI_VOID onSpeed(const scs_string_t,const scs_u32_t,const scs_value_t* value,const scs_context_t) {
    if(value && value->type==SCS_VALUE_TYPE_float){speed=value->value_float.value;lastSpeed=GetTickCount64();}
    else lastSpeed=0;
}
SCSAPI_VOID onTrailer(const scs_string_t,const scs_u32_t,const scs_value_t* value,const scs_context_t) {
    if(value && value->type==SCS_VALUE_TYPE_bool){trailerConnected=value->value_bool.value!=0;trailerTime=GetTickCount64();}
    else trailerTime=0;
}
SCSAPI_VOID onEvent(const scs_event_t event,const void* info,const scs_context_t) {
    if(event==SCS_TELEMETRY_EVENT_configuration && info) {
        auto c=static_cast<const scs_telemetry_configuration_t*>(info);
        if(!c->id)return;
        bool active=c->attributes && c->attributes[0].name;
        if(strcmp(c->id,"job")==0){jobSeen=true;jobActive=active;jobTelemetry.update(c->attributes);}
        if(strcmp(c->id,"car_job")==0){carSeen=true;carActive=active;}
        if(strcmp(c->id,"bus_job")==0){busSeen=true;busActive=active;}
        return;
    }
    if(event==SCS_TELEMETRY_EVENT_started){driving=true;lastSpeed=trailerTime=0;return;}
    if(event==SCS_TELEMETRY_EVENT_paused){driving=false;lastSpeed=trailerTime=0;return;}
    if(event!=SCS_TELEMETRY_EVENT_frame_end||disabled)return;
    if(callbackThread!=GetCurrentThreadId()){disabled=true;status("disabled_thread_changed");return;}
    ULONGLONG now=GetTickCount64();if(now-lastPoll<1000)return;lastPoll=now;
    if(awaitingTelemetry) {
        if(jobTelemetry.confirmedUnloaded(jobRevisionBefore)){awaitingTelemetry=false;disabled=true;status("assignment_confirmed_unloaded_check_pickup_route");}
        else if(jobTelemetry.revision>jobRevisionBefore && jobActive){awaitingTelemetry=false;disabled=true;status("assignment_mismatch_or_loaded_requires_review");}
        else if(now>verificationDeadline){awaitingTelemetry=false;disabled=true;status("assignment_returned_but_telemetry_unconfirmed_no_retry");}
        return;
    }
    if(usageDone)return;
    if(!assignmentCodeMatches()){disabled=true;status("disabled_native_code_changed");return;}
    uintptr_t registry=0;if(!registryReady(registry)){status("waiting_native_registry");return;}
    auto request=folder+L"\\request.txt";
    HANDLE file=CreateFileW(request.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE){status("ready_waiting_request");return;}
    char text[64]={};DWORD count=0;BOOL ok=ReadFile(file,text,sizeof(text)-1,&count,nullptr);CloseHandle(file);
    if(!ok)return;
    while(count && (text[count-1]=='\r'||text[count-1]=='\n'))text[--count]=0;
    if(strcmp(text,"assign_yakima_wenatchee_pallets")!=0){status("request_rejected_unknown_operation");return;}
    if(!driving||!lastSpeed||now-lastSpeed>2000||!std::isfinite(speed)||fabsf(speed)>0.1f){status("waiting_stationary_driving");return;}
    if(!trailerTime||now-trailerTime>2000||!trailerConnected){status("waiting_attached_trailer");return;}
    if(!jobSeen||!carSeen||!busSeen){status("waiting_all_job_configurations");return;}
    if(jobActive||carActive||busActive){status("blocked_active_job");return;}
    AssignmentObjects objects;const char* reason=findObjects(objects);
    if(reason){status(reason);return;}
    // Claim BEFORE either mutating call; no crash, restart or SDK reload replays it.
    if(!MoveFileExW(request.c_str(),(folder+L"\\request.consumed").c_str(),MOVEFILE_REPLACE_EXISTING)){status("request_claim_failed");return;}
    usageDone=true;offerAttempted=true;
    log("[MSDC_AUTO_DISPATCH] Creating fixed-route cargo offer, then attempting owned-trailer acceptance once.");
    if(!createOffer(registry)){disabled=true;status("offer_call_failed_no_retry");return;}
    uintptr_t selected=matchingOffer(objects);
    if(!selected){disabled=true;status("offer_candidate_absent_or_ambiguous_no_assignment");return;}
    // Recheck native no-job references after offer generation.
    AssignmentObjects checked;reason=findObjects(checked);
    if(reason || checked.economy!=objects.economy || checked.source!=objects.source || checked.trailer!=objects.trailer){disabled=true;status("native_state_changed_no_assignment");return;}
    assignmentAttempted=true;jobRevisionBefore=jobTelemetry.revision;
    char message[256];sprintf_s(message,"[MSDC_AUTO_DISPATCH] ACCEPT economy=%p company=%p offer=%p RVA=0x7891a0",reinterpret_cast<void*>(objects.economy),reinterpret_cast<void*>(objects.source),reinterpret_cast<void*>(selected));log(message);
    bool returned=acceptOffer(objects.economy,objects.source,selected);
    if(!returned){disabled=true;status("acceptance_rejected_or_faulted_no_retry");return;}
    awaitingTelemetry=true;verificationDeadline=now+15000;
    status("acceptance_returned_waiting_fresh_telemetry");
}
SCSAPI_RESULT scs_telemetry_init(const scs_u32_t version,const scs_telemetry_init_params_t* params) {
    if(version!=SCS_TELEMETRY_VERSION_1_01)return SCS_RESULT_unsupported;
    if(!params)return SCS_RESULT_invalid_parameter;
    auto p=static_cast<const scs_telemetry_init_params_v101_t*>(params);
    gameLog=p->common.log;
    if(!p->common.game_id || strcmp(p->common.game_id,"ats")!=0)return SCS_RESULT_unsupported;
    if(!exactHost()){log("[MSDC_AUTO_DISPATCH] Refused: executable SHA-256 does not match tested ATS build.");return SCS_RESULT_generic_error;}
    imageBase=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if(!assignmentCodeMatches()){log("[MSDC_AUTO_DISPATCH] Refused: handler bytes mismatch.");return SCS_RESULT_generic_error;}
    wchar_t local[32768];DWORD size=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
    if(!size||size>=32768)return SCS_RESULT_generic_error;
    folder=std::wstring(local)+L"\\MSDCAutoDispatch";
    if(!CreateDirectoryW(folder.c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS)return SCS_RESULT_generic_error;
    callbackThread=GetCurrentThreadId();lastPoll=lastSpeed=0;driving=disabled=usageDone=false;lastState.clear();
    jobSeen=carSeen=busSeen=jobActive=carActive=busActive=offerAttempted=false;
    assignmentAttempted=awaitingTelemetry=trailerConnected=false;trailerTime=verificationDeadline=0;jobTelemetry={};jobRevisionBefore=0;
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
    if(p->register_for_channel("trailer.connected",SCS_U32_NIL,SCS_VALUE_TYPE_bool,SCS_TELEMETRY_CHANNEL_FLAG_each_frame|SCS_TELEMETRY_CHANNEL_FLAG_no_value,onTrailer,nullptr)!=SCS_RESULT_ok){
        if(p->unregister_from_channel)p->unregister_from_channel("truck.speed",SCS_U32_NIL,SCS_VALUE_TYPE_float);
        for(auto event:events)p->unregister_from_event(event);
        return SCS_RESULT_generic_error;
    }
    status("loaded_automatic_assignment_test");return SCS_RESULT_ok;
}
SCSAPI_VOID scs_telemetry_shutdown() {
    if(!folder.empty())status("unloaded");gameLog=nullptr;disabled=true;
}
