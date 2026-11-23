#include <windows.h>
#include <cassert>
#include <cstdio>
#include "scssdk_telemetry.h"
#include "native_arguments.h"
#include "../dispatch_shared/game_target.h"
static int registrations=0;
SCSAPI_VOID testLog(scs_log_type_t,const scs_string_t text){puts(text);}
SCSAPI_RESULT testEvent(scs_event_t,scs_telemetry_event_callback_t,scs_context_t){++registrations;return SCS_RESULT_ok;}
int wmain(int argc,wchar_t**argv){
    auto text=borrowedInput("msdc_probe");assert(text.length==10 && strcmp(text.data,"msdc_probe")==0);
    NativeStringInput tokens[]={borrowedInput("cheat"),borrowedInput("job"),text,borrowedInput(""),borrowedInput("")};
    NativeParsedInput parsed={};parsed.data=tokens;parsed.count=5;
    const auto raw=reinterpret_cast<const unsigned char*>(&parsed);
    assert(*reinterpret_cast<NativeStringInput* const*>(raw+0x18)==tokens);
    assert(*reinterpret_cast<const uintptr_t*>(raw+0x20)==5);
    assert(parsed.data[2].length==10 && parsed.data[3].length==0 && parsed.data[4].length==0);
    assert(argc==2);HMODULE dll=LoadLibraryW(argv[1]);assert(dll);
    using Init=scs_result_t(*)(scs_u32_t,const scs_telemetry_init_params_t*);
    using Stop=void(*)();
    auto init=reinterpret_cast<Init>(GetProcAddress(dll,"scs_telemetry_init"));
    auto stop=reinterpret_cast<Stop>(GetProcAddress(dll,"scs_telemetry_shutdown"));
    assert(init&&stop);
    assert(init(0,nullptr)==SCS_RESULT_unsupported);
    assert(init(SCS_TELEMETRY_VERSION_1_01,nullptr)==SCS_RESULT_invalid_parameter);
    scs_telemetry_init_params_v101_t p={};p.common.game_id=strcmp(dispatchGame::sdkId,"ats")==0?"eut2":"ats";p.common.log=testLog;p.register_for_event=testEvent;
    assert(init(SCS_TELEMETRY_VERSION_1_01,&p)==SCS_RESULT_unsupported);
    p.common.game_id=dispatchGame::sdkId;
    assert(init(SCS_TELEMETRY_VERSION_1_01,&p)==SCS_RESULT_generic_error);
    assert(registrations==0);stop();assert(FreeLibrary(dll));
    puts("PASS: load/exports, unsupported API, null params, wrong game, wrong executable rejected before callbacks; unload.");return 0;
}
