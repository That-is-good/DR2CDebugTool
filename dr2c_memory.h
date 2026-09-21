#ifndef DR2C_MEMORY_H
#define DR2C_MEMORY_H

// ============================================================================
// dr2c_memory.h
// 统一本工程对“游戏内存”的访问方式：
//   - 所有裸地址统一为 dr2c::Address（std::uintptr_t），不再混用 void* /
//     unsigned char* / uintptr_t
//   - 所有字段读写统一走 Load / Store / LoadArray / StoreArray 模板
//   - 指针 <-> 整数转换统一走 Ptr / Addr / AsPtr / AsConstPtr / AsFn
//
// 重要约定：
//   本文件只统一“类型与访问方式”，不改变任何偏移量，也不改变任何字段的读写宽度。
//   地址说明：模块基址默认 0x400000。RVA = 相对模块基址的偏移；
//   VA  = 0x400000 + RVA。本工程内记录的一律是 RVA。
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace dr2c {

using Address      = std::uintptr_t;         // 进程内游戏内存地址（32 位进程）
using Rva          = std::uintptr_t;         // 相对模块基址的偏移（基址 0x400000）
using BytePtr      = unsigned char *;
using ConstBytePtr = const unsigned char *;

inline BytePtr  Ptr(Address a)                        { return reinterpret_cast<BytePtr>(a); }
inline Address  Addr(const void *p)                   { return reinterpret_cast<Address>(p); }

template <typename T> inline T       *AsPtr(Address a)      { return reinterpret_cast<T *>(a); }
template <typename T> inline const T *AsConstPtr(Address a)  { return reinterpret_cast<const T *>(a); }
template <typename Fn> inline Fn      AsFn(Address a)        { return reinterpret_cast<Fn>(a); }

// 函数指针 -> 整数地址（GetProcAddress / MinHook 边界使用）
template <typename Fn> inline Address FnAddr(Fn fn)          { return reinterpret_cast<Address>(fn); }

// ---------- 单值读写（宽度由 T 决定） ----------
template <typename T> inline T Load(Address a)
{
    T value{};
    std::memcpy(&value, Ptr(a), sizeof(T));
    return value;
}

template <typename T> inline void Store(Address a, T value)
{
    std::memcpy(Ptr(a), &value, sizeof(T));
}

// ---------- 定长数组读写（宽度由数组长度决定） ----------
template <typename T, std::size_t N> inline void LoadArray(Address a, T (&dst)[N])
{
    std::memcpy(dst, Ptr(a), sizeof(dst));
}

template <typename T, std::size_t N> inline void StoreArray(Address a, const T (&src)[N])
{
    std::memcpy(Ptr(a), src, sizeof(src));
}

} // namespace dr2c

#endif
