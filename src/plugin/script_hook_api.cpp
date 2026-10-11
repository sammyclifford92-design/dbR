#include "script_hook_api.hpp"

namespace {

template <typename Function>
Function getExport(HMODULE module, const char* name) {
    return reinterpret_cast<Function>(GetProcAddress(module, name));
}

}

bool ScriptHookApi::load() {
    if (module != nullptr) {
        return nativeInit != nullptr && nativePush64 != nullptr && nativeCall != nullptr &&
            scriptWait != nullptr && scriptRegister != nullptr && scriptUnregister != nullptr &&
            worldGetAllPeds != nullptr;
    }

    HMODULE scriptHook = GetModuleHandleW(L"ScriptHookRDR2.dll");
    bool loadedByPlugin = false;
    if (scriptHook == nullptr) {
        scriptHook = LoadLibraryW(L"ScriptHookRDR2.dll");
        loadedByPlugin = scriptHook != nullptr;
    }
    if (scriptHook == nullptr) {
        return false;
    }

    nativeInit = getExport<NativeInit>(scriptHook, "nativeInit");
    nativePush64 = getExport<NativePush64>(scriptHook, "nativePush64");
    nativeCall = getExport<NativeCall>(scriptHook, "nativeCall");
    scriptWait = getExport<ScriptWait>(scriptHook, "scriptWait");
    scriptRegister = getExport<ScriptRegister>(scriptHook, "scriptRegister");
    scriptUnregister = getExport<ScriptUnregister>(scriptHook, "scriptUnregister");
    worldGetAllPeds = getExport<WorldGetAllPeds>(scriptHook, "worldGetAllPeds");

    const bool complete = nativeInit != nullptr && nativePush64 != nullptr && nativeCall != nullptr &&
        scriptWait != nullptr && scriptRegister != nullptr && scriptUnregister != nullptr &&
        worldGetAllPeds != nullptr;
    if (complete) {
        module = scriptHook;
    } else if (loadedByPlugin) {
        FreeLibrary(scriptHook);
    }
    return complete;
}

ScriptHookApi& scriptHookApi() {
    static ScriptHookApi api;
    return api;
}