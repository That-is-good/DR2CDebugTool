#ifndef DR2C_AGENT_MEMORY_H
#define DR2C_AGENT_MEMORY_H

// ============================================================================
// agent_memory.h
// Agent 位于游戏进程内部，这里提供“不会把进程打崩”的读写封装。
//
// 为什么不用裸 memcpy：
//   GCC/MinGW 32 位不支持 MSVC 的 __try/__except，直接用指针访问非法地址会
//   崩溃整个游戏。ReadProcessMemory / WriteProcessMemory 作用于自身进程时，
//   地址非法只会返回 FALSE，不会抛异常，因此统一走它们。
// ============================================================================

#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace dr2c {

using Address = std::uintptr_t;

inline bool SafeRead(Address address, void *buffer, std::uint32_t size)
{
    if (!address || !buffer || !size)
        return false;
    SIZE_T done = 0;
    return ReadProcessMemory(GetCurrentProcess(),
                             reinterpret_cast<LPCVOID>(address),
                             buffer, size, &done) != FALSE
           && done == size;
}

inline bool SafeWrite(Address address, const void *buffer, std::uint32_t size)
{
    if (!address || !buffer || !size)
        return false;
    SIZE_T done = 0;
    return WriteProcessMemory(GetCurrentProcess(),
                              reinterpret_cast<LPVOID>(address),
                              buffer, size, &done) != FALSE
           && done == size;
}

template <typename T>
inline bool SafeLoad(Address address, T &value)
{
    return SafeRead(address, &value, sizeof(T));
}

template <typename T>
inline bool SafeStore(Address address, const T &value)
{
    return SafeWrite(address, &value, sizeof(T));
}

} // namespace dr2c

#endif // DR2C_AGENT_MEMORY_H
