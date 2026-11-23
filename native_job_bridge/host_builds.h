#pragma once
#include <cstdint>
#include <cstring>
#include "../dispatch_shared/game_target.h"

// Each supported executable is pinned by SHA-256. 1.62.2 addresses were checked
// against all seven complete native routines and 14 independent registry xrefs.
// See research/current-native-verification.json. This does not replace runtime
// prefix, registry-entry, object, stationary-truck and telemetry checks.
struct HostBuild {
    const char* hash;
    uintptr_t registry,economy,mainGame,handler,lookup,accept,companyTable,cargoOfferTable,routeReset,finishLoading,capacity,companyCity;
};
inline constexpr HostBuild hostBuilds[]={
#ifdef GT_TARGET_ETS2
    // Static verification: research/ets2-port-20261120/native-verification.json.
    {"055ec5343d53bd427de54b06eca74937f0d348dbb0c5d71862edddcf45e2db66",0x37150e0,0x30aa900,0x3714fb0,0x5bf3d0,0x12dae0,0x7899f0,0x2300200,0x2351f88,0x5ff520,0x6006b0,0x83f6c0,0x7bcff0}
#else
    {"28ca69a7eca99519371adcc3e78b978946ad28ce57332780e9fe4175be92f6a7",0x3719dd0,0x30afe60,0x3719ca8,0x5bebc0,0x12d6f0,0x7891a0,0x2300988,0x2352698,0x5fed20,0x5ffeb0,0x83ef50,0x7bc880},
    {"3381d583211e9418f6c32964c553877c29a392c473532956123ed8b7c5c89e82",0x371bdf8,0x30b1640,0x371bcc8,0x5c00d0,0x12e770,0x78a700,0x2301560,0x2353268,0x600230,0x6013c0,0x8403d0,0x7bdd00},
    // Full-function/control-flow and independent data-reference proof:
    // research/ats-16222-20261114/native-verification.json.
    {"63ecae85a070934530d9fd971d4d56e5479f1214d7077b403f79b8981c613e08",0x371be58,0x30b1680,0x371bd28,0x5c0110,0x12e7b0,0x78a730,0x2301560,0x23532f8,0x600260,0x6013f0,0x840400,0x7bdd30}
#endif
};
inline uintptr_t registryRva=0,economyRva=0,mainGameRva=0,handlerRva=0,lookupRva=0,acceptRva=0,companyVtableRva=0,cargoOfferVtableRva=0;
inline const char* activeHostHash=nullptr;
inline uintptr_t routeResetRva=0;
inline uintptr_t finishLoadingRva=0;
inline uintptr_t capacityRva=0,companyCityRva=0;
inline bool selectHost(const char* hash) {
    registryRva=economyRva=mainGameRva=handlerRva=lookupRva=acceptRva=companyVtableRva=cargoOfferVtableRva=routeResetRva=finishLoadingRva=capacityRva=companyCityRva=0;activeHostHash=nullptr;
    for(const auto& b:hostBuilds)if(strcmp(hash,b.hash)==0){registryRva=b.registry;economyRva=b.economy;mainGameRva=b.mainGame;handlerRva=b.handler;lookupRva=b.lookup;acceptRva=b.accept;companyVtableRva=b.companyTable;cargoOfferVtableRva=b.cargoOfferTable;routeResetRva=b.routeReset;finishLoadingRva=b.finishLoading;capacityRva=b.capacity;companyCityRva=b.companyCity;activeHostHash=b.hash;return true;}return false;
}
