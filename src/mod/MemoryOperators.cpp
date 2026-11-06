// This file makes the mod use LeviLamina's memory operators by default.
//
// [退出崩溃修复·第二轮] 与 LL 原版实现唯一的区别：进程退出流程开始后
// （ChiyanMap::disable() 置位 g_skipDeleteOnShutdown），所有 operator delete
// 都变成 no-op（有意泄漏）。
//
// 崩溃现场：exit() -> LdrShutdownProcess -> CRT 全局对象析构
//   -> ucrtbase!_free_base -> MallocRedirect -> ntdll!RtlpFreeHeapInternal
//   -> mov rax,[r15] 读已失效的 free-list Blink -> 0xC0000005。
// 本 DLL 有几十个 inline 全局 STL 对象（unordered_map / string / vector /
// mutex / thread / InlineHook ...），它们由 CRT 在 DLL_PROCESS_DETACH 阶段
// 自动析构；此时游戏的 IMemoryAllocator 与 LeviLauncher 的 MallocRedirect
// 已处于半拆卸状态，继续经 getDefaultAllocator()._release() 释放就会破坏堆。
// 上一轮移除 disable() 中的显式释放无效，正是因为真正的释放来自 CRT 自动析构
// （且被尾调用优化，栈上看不到本 DLL 的帧）。
// 进程终止时内存由操作系统统一回收，此处刻意不回收是安全的。

#include <atomic>
#include <cstddef>
#include <new>

#include "ll/api/memory/Memory.h"
#include "mc/deps/core/memory/IMemoryAllocator.h"

namespace ChiyanMapMem {
std::atomic<bool> g_skipDeleteOnShutdown{false};
}

extern "C" LL_SHARED_EXPORT int const ll_memory_operator_overrided{};

namespace {
inline bool SkipFreeOnShutdown() {
    return ChiyanMapMem::g_skipDeleteOnShutdown.load(std::memory_order_relaxed);
}
} // namespace

void operator delete(void* p) noexcept {
    if (p != nullptr && !SkipFreeOnShutdown()) {
        ::ll::memory::getDefaultAllocator()._release(p);
    }
}

void operator delete(void* p, std::nothrow_t const&) noexcept { operator delete(p); }

void operator delete[](void* p) noexcept { operator delete(p); }

void operator delete[](void* p, std::nothrow_t const&) noexcept { operator delete[](p); }

void operator delete(void* p, std::size_t) noexcept { operator delete(p); }

void operator delete[](void* p, std::size_t) noexcept { operator delete[](p); }

void operator delete(void* p, std::align_val_t) noexcept {
    if (p != nullptr && !SkipFreeOnShutdown()) {
        ::ll::memory::getDefaultAllocator()._alignedRelease(p);
    }
}

void operator delete(void* p, std::align_val_t alignment, std::nothrow_t const&) noexcept {
    operator delete(p, alignment);
}
void operator delete[](void* p, std::align_val_t alignment) noexcept { operator delete(p, alignment); }

void operator delete[](void* p, std::align_val_t alignment, std::nothrow_t const&) noexcept {
    operator delete[](p, alignment);
}
void operator delete(void* p, std::size_t, std::align_val_t alignment) noexcept { operator delete(p, alignment); }

void operator delete[](void* p, std::size_t, std::align_val_t alignment) noexcept {
    operator delete[](p, alignment);
}

[[nodiscard]] LL_ALLOCATOR void* operator new(std::size_t size) {
    if (void* const block = ::ll::memory::getDefaultAllocator()._allocate(size)) {
        return block;
    }
    ::ll::memory::throwMemoryException(size);
}
[[nodiscard]] LL_NOTHROW_ALLOCATOR void* operator new(std::size_t size, std::nothrow_t const&) noexcept {
    return ::ll::memory::getDefaultAllocator()._allocate(size);
}
[[nodiscard]] LL_ALLOCATOR void* operator new[](std::size_t size) { return operator new(size); }

[[nodiscard]] LL_NOTHROW_ALLOCATOR void* operator new[](std::size_t size, std::nothrow_t const& tag) noexcept {
    return operator new(size, tag);
}
[[nodiscard]] LL_ALLOCATOR void* operator new(std::size_t size, std::align_val_t alignment) {
    if (void* const block =
            ::ll::memory::getDefaultAllocator()._alignedAllocate(size, static_cast<size_t>(alignment))) {
        return block;
    }
    ::ll::memory::throwMemoryException(size);
}
[[nodiscard]] LL_NOTHROW_ALLOCATOR void*
operator new(std::size_t size, std::align_val_t alignment, std::nothrow_t const&) noexcept {
    return ::ll::memory::getDefaultAllocator()._alignedAllocate(size, static_cast<size_t>(alignment));
}
[[nodiscard]] LL_ALLOCATOR void* operator new[](std::size_t size, std::align_val_t alignment) {
    return operator new(size, alignment);
}
[[nodiscard]] LL_NOTHROW_ALLOCATOR void*
operator new[](std::size_t size, std::align_val_t alignment, std::nothrow_t const& tag) noexcept {
    return operator new(size, alignment, tag);
}
