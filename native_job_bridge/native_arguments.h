#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

// Borrowed INPUT views only. The native copy routine reads +8 (data) and
// +0x10 (length); it never owns or destroys these input buffers. The native
// handler constructs/destroys its own output strings using engine allocators.
struct NativeStringInput {
    uintptr_t unusedVtable;
    const char* data;
    uint32_t length;
    uint32_t unusedCapacity;
    uintptr_t unusedAllocator;
};
struct NativeParsedInput {
    uintptr_t reserved[2];
    uintptr_t unusedArrayVtable;
    NativeStringInput* data;
    uintptr_t count;
    uintptr_t unusedCapacity;
    uintptr_t reservedTail[2];
};
static_assert(sizeof(NativeStringInput)==32);
static_assert(offsetof(NativeStringInput,data)==8);
static_assert(offsetof(NativeStringInput,length)==0x10);
static_assert(offsetof(NativeParsedInput,data)==0x18);
static_assert(offsetof(NativeParsedInput,count)==0x20);
static_assert(sizeof(NativeParsedInput)==64);

inline NativeStringInput borrowedInput(const char* text) {
    return {0,text,static_cast<uint32_t>(strlen(text)),0,0};
}
