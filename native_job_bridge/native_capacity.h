#pragma once
#include <cstddef>

struct NativeCapacityRequest {
    uintptr_t economy=0;
    uint32_t units=0;float fill=0;
    float fillOverride=1;uint32_t reserved14=0;
    uintptr_t reserved18=0,definition=0;
    uint32_t truckAxles=2;float truckMassKg=10000;
    uintptr_t sourceCountry=0,targetCountry=0,cargo=0;
    unsigned char singleSpecial=0,warn=0,reserved4a[2]={};
    float volumeShortage=0,trailerMassShortage=0,combinationMassShortage=0;
};
static_assert(sizeof(NativeCapacityRequest)==0x58&&offsetof(NativeCapacityRequest,definition)==0x20&&offsetof(NativeCapacityRequest,cargo)==0x40&&offsetof(NativeCapacityRequest,volumeShortage)==0x4c,"Native capacity ABI changed");
static bool capacityCodeMatches() {
    {const unsigned char expected[]={0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7c,0x24,0x20,0x55,0x48,0x8d,0xac,0x24,0x10};unsigned char actual[sizeof(expected)];
     if(!capacityRva||!readMemory(imageBase+capacityRva,actual,sizeof(actual))||memcmp(actual,expected,sizeof(expected)))return false;}
    {const unsigned char expected[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0x81,0x00,0x01,0x00,0x00,0x48,0x8b,0xd9};unsigned char actual[sizeof(expected)];
     if(!companyCityRva||!readMemory(imageBase+companyCityRva,actual,sizeof(actual))||memcmp(actual,expected,sizeof(expected)))return false;}
    return true;
}
static uintptr_t nativeCompanyCity(uintptr_t company) {
    using GetCity=uintptr_t(__fastcall*)(uintptr_t);
    __try{return reinterpret_cast<GetCity>(imageBase+companyCityRva)(company);}
    __except(EXCEPTION_EXECUTE_HANDLER){nativeFault=true;return 0;}
}
static uint32_t nativeMaximumUnits(NativeCapacityRequest* request) {
    using Capacity=uint32_t(__fastcall*)(NativeCapacityRequest*);
    __try{return reinterpret_cast<Capacity>(imageBase+capacityRva)(request);}
    __except(EXCEPTION_EXECUTE_HANDLER){nativeFault=true;return 0;}
}
static bool setOfferUnits(uintptr_t offer,uint32_t units,float fill) {
    // Verified in payload-native-static.json: standard job initialization copies
    // these two adjacent fields to unit count and loading factor before pricing.
    struct Value {uint32_t count;float factor;};const Value value={units,fill};
    __try{memcpy(reinterpret_cast<void*>(offer+0xcc),&value,sizeof(value));return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){nativeFault=true;return false;}
}
