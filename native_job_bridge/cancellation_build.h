#pragma once

// Separate from dispatch support: cancellation has its own exact-build allowlist.
// ETS2 static evidence: research/ets2-cancellation-20261123/native-verification.json.
// A matching build is eligible for experimental testing, not runtime proof.
struct CancellationBuild {const char* hash;uintptr_t wrapper,otherRoot;};
#if defined(GT_TARGET_ETS2)
static constexpr CancellationBuild cancellationBuild={"055ec5343d53bd427de54b06eca74937f0d348dbb0c5d71862edddcf45e2db66",0x123cac0,0x3715020};
#else
static constexpr CancellationBuild cancellationBuild={"63ecae85a070934530d9fd971d4d56e5479f1214d7077b403f79b8981c613e08",0x123d640,0x371bd98};
#endif
static const CancellationBuild* activeCancellationBuild() {
    return activeHostHash&&!strcmp(activeHostHash,cancellationBuild.hash)?&cancellationBuild:nullptr;
}
static bool cancellationCodeMatches() {
    const auto* build=activeCancellationBuild();
    const unsigned char expected[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x48};
    unsigned char actual[sizeof(expected)]={};
    return build&&readMemory(imageBase+build->wrapper,actual,sizeof(actual))&&!memcmp(actual,expected,sizeof(expected));
}
