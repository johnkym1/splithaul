#pragma once
static void log(const char* text) {
    if (gameLog) gameLog(SCS_LOG_TYPE_message,text);
}
static bool readMemory(uintptr_t address,void* out,size_t size) {
    SIZE_T count=0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),out,size,&count) && count==size;
}
static bool word(uintptr_t address,uintptr_t& value) { return readMemory(address,&value,sizeof(value)); }
static bool exactHost(std::string* observedHash=nullptr) {
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
    if(observedHash)*observedHash=ok?hex:"";
    return ok && selectHost(hex);
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
static bool stringEquals(uintptr_t stringObject,const char* text) {
    uintptr_t pointer=0;uint32_t length=0;char actual[128]={};size_t size=strlen(text);
    return size<sizeof(actual) && word(stringObject+8,pointer) && readMemory(stringObject+0x10,&length,4) && length==size && readMemory(pointer,actual,size+1) && memcmp(actual,text,size+1)==0;
}
static bool acceptOffer(uintptr_t economy,uintptr_t company,uintptr_t offer) {
    using Accept=unsigned char(__fastcall*)(uintptr_t,uintptr_t,uintptr_t);
    __try { return reinterpret_cast<Accept>(imageBase+acceptRva)(economy,company,offer)!=0; }
    __except(EXCEPTION_EXECUTE_HANDLER) { nativeFault=true;return false; }
}
static bool routingCodeMatches() {
    // Full old/new function and switch-table verification is recorded in
    // research/pickup-navigation-static.json; guard the entry again at runtime.
    const unsigned char expected[]={0x40,0x55,0x53,0x56,0x48,0x8d,0x6c,0x24,0xb9,0x48,0x81,0xec,0xd0,0,0,0};
    unsigned char actual[sizeof(expected)];
    return routeResetRva&&readMemory(imageBase+routeResetRva,actual,sizeof(actual))&&memcmp(actual,expected,sizeof(expected))==0;
}
static bool resetPickupRoute(uintptr_t player) {
    using Reset=void(__fastcall*)(uintptr_t,unsigned char);
    // False clears old custom waypoints and rebuilds the current job route.
    // The game's helper selects source for an unloaded job, destination once loaded.
    __try {reinterpret_cast<Reset>(imageBase+routeResetRva)(player,0);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER) {nativeFault=true;return false;}
}
static bool loadingCodeMatches() {
    const unsigned char expected[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x48};
    unsigned char actual[sizeof(expected)];
    return finishLoadingRva&&readMemory(imageBase+finishLoadingRva,actual,sizeof(actual))&&memcmp(actual,expected,sizeof(expected))==0;
}
static bool finishCarriedLoading(uintptr_t player,uintptr_t economy) {
    using Finish=void(__fastcall*)(uintptr_t,uintptr_t,unsigned char);
    // Same arguments as the game's ordinary-job completion caller. It updates
    // cargo/weight, clears loading state and routes to delivery; no position call.
    __try {reinterpret_cast<Finish>(imageBase+finishLoadingRva)(player,economy,0);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER) {nativeFault=true;return false;}
}
