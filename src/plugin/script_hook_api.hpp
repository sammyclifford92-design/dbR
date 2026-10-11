#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>

using Void = DWORD;
using Any = DWORD;
using Hash = DWORD;
using Entity = int;
using Player = int;
using Ped = int;

struct Vector3 {
    alignas(8) float x;
    alignas(8) float y;
    alignas(8) float z;
};

static_assert(sizeof(Vector3) == 24, "RDR2 native Vector3 must use 8-byte member alignment.");

struct ScriptHookApi {
    using ScriptMain = void(__cdecl*)();
    using NativeInit = void(__cdecl*)(std::uint64_t);
    using NativePush64 = void(__cdecl*)(std::uint64_t);
    using NativeCall = std::uint64_t*(__cdecl*)();
    using ScriptWait = void(__cdecl*)(DWORD);
    using ScriptRegister = void(__cdecl*)(HMODULE, ScriptMain);
    using ScriptUnregister = void(__cdecl*)(HMODULE);
    using WorldGetAllPeds = int(__cdecl*)(int*, int);

    HMODULE module = nullptr;
    NativeInit nativeInit = nullptr;
    NativePush64 nativePush64 = nullptr;
    NativeCall nativeCall = nullptr;
    ScriptWait scriptWait = nullptr;
    ScriptRegister scriptRegister = nullptr;
    ScriptUnregister scriptUnregister = nullptr;
    WorldGetAllPeds worldGetAllPeds = nullptr;

    bool load();
};

ScriptHookApi& scriptHookApi();