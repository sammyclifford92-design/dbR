#pragma once

#include <cstring>
#include <cstdint>
#include <type_traits>

#include "script_hook_api.hpp"

template <typename T>
void pushNativeArgument(const T& value) {
    static_assert(sizeof(T) <= sizeof(std::uint64_t), "RDR2 native arguments must fit in 64 bits.");
    std::uint64_t packed = 0;
    std::memcpy(&packed, &value, sizeof(T));
    scriptHookApi().nativePush64(packed);
}

template <typename... Args>
void pushNativeArguments(const Args&... args) {
    (pushNativeArgument(args), ...);
}

template <typename Result, typename... Args>
Result invokeNative(std::uint64_t hash, const Args&... args) {
    static_assert(!std::is_void_v<Result>, "Use a concrete Script Hook return type.");
    scriptHookApi().nativeInit(hash);
    pushNativeArguments(args...);
    return *reinterpret_cast<Result*>(scriptHookApi().nativeCall());
}