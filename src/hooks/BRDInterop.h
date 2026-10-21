#pragma once
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <ll/api/mod/NativeMod.h>
#include <mc/client/game/ClientInstance.h>

class BRDInterop {
public:
    static BRDInterop& getInstance() {
        static BRDInterop instance;
        return instance;
    }

    typedef void (*BRD_ToggleMenu_t)();
    typedef bool (*BRD_IsMenuOpen_t)();

    BRD_ToggleMenu_t            pfnBRD_ToggleMenu            = nullptr;
    BRD_IsMenuOpen_t            pfnBRD_IsMenuOpen            = nullptr;

    void**                      pResourcePackManager         = nullptr;
    void**                      pResourcePackManager_load    = nullptr;
    std::atomic_bool*           pReloadShaders               = nullptr;
    void**                      pReloadMaterial              = nullptr;

    bool m_brdLoaded              = false;
    bool m_initialized            = false;
    bool m_initialReloadTriggered = false;

    void init() {
        if (m_initialized) return;
        m_initialized = true;

        HMODULE hBRD = GetModuleHandleW(L"BetterRenderDragon.dll");
        if (!hBRD) return;
        m_brdLoaded = true;

        pfnBRD_ToggleMenu = reinterpret_cast<BRD_ToggleMenu_t>(GetProcAddress(hBRD, "BRD_ToggleMenu"));
        pfnBRD_IsMenuOpen = reinterpret_cast<BRD_IsMenuOpen_t>(GetProcAddress(hBRD, "BRD_IsMenuOpen"));

        uintptr_t brdBase = reinterpret_cast<uintptr_t>(hBRD);
        auto* dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(brdBase);
        if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return;
        auto* ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(brdBase + dosHeader->e_lfanew);
        if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return;

        uint8_t* pImage = reinterpret_cast<uint8_t*>(brdBase);
        size_t imageSize = ntHeaders->OptionalHeader.SizeOfImage;

        // 1. Locate resourcePackManager & ResourcePackManager_load via unique byte sequence:
        // Pattern: 48 8B CB FF D0 48 89 05
        // Disasm:
        //   mov rcx, rbx
        //   call rax
        //   mov qword ptr [rip + disp1], rax  <-- resourcePackManager
        //   mov rax, [rax]
        //   mov r8, [rax + 0x18]
        //   mov qword ptr [rip + disp2], r8   <-- ResourcePackManager_load
        const uint8_t pat1[] = {0x48, 0x8B, 0xCB, 0xFF, 0xD0, 0x48, 0x89, 0x05};
        for (size_t i = 0; i + sizeof(pat1) + 20 <= imageSize; ++i) {
            if (std::memcmp(pImage + i, pat1, sizeof(pat1)) == 0) {
                uintptr_t inst1 = reinterpret_cast<uintptr_t>(pImage + i + 5); // 48 89 05 [disp32]
                int32_t disp1 = *reinterpret_cast<int32_t*>(inst1 + 3);
                pResourcePackManager = reinterpret_cast<void**>(inst1 + 7 + disp1);

                uintptr_t instLoad = reinterpret_cast<uintptr_t>(pImage + i + 12 + 3 + 4); // 4C 89 05 [disp32]
                int32_t disp2 = *reinterpret_cast<int32_t*>(instLoad + 3);
                pResourcePackManager_load = reinterpret_cast<void**>(instLoad + 7 + disp2);

                break;
            }
        }

        // Fallback to known offsets for BRD v1.7.5 if pattern was modified
        if (!pResourcePackManager) {
            pResourcePackManager = reinterpret_cast<void**>(brdBase + 0x1ab7a8);
            pResourcePackManager_load = reinterpret_cast<void**>(brdBase + 0x1ab7c0);
        }

        // 2. Locate reloadShaders & reloadMaterial:
        // Pattern: 84 C0 74 0E 33 C0 86 05
        // Disasm:
        //   test al, al
        //   je +14
        //   xor eax, eax
        //   xchg [rip + disp3], al            <-- reloadShaders (std::atomic_bool)
        //   call qword ptr [rip + disp4]      <-- reloadMaterial
        const uint8_t pat2[] = {0x84, 0xC0, 0x74, 0x0E, 0x33, 0xC0, 0x86, 0x05};
        for (size_t i = 0; i + sizeof(pat2) + 20 <= imageSize; ++i) {
            if (std::memcmp(pImage + i, pat2, sizeof(pat2)) == 0) {
                uintptr_t instXchg = reinterpret_cast<uintptr_t>(pImage + i + 6); // 86 05 [disp32]
                int32_t disp3 = *reinterpret_cast<int32_t*>(instXchg + 2);
                pReloadShaders = reinterpret_cast<std::atomic_bool*>(instXchg + 6 + disp3);

                uintptr_t instCall = reinterpret_cast<uintptr_t>(pImage + i + 6 + 6); // FF 15 [disp32]
                int32_t disp4 = *reinterpret_cast<int32_t*>(instCall + 2);
                pReloadMaterial = reinterpret_cast<void**>(instCall + 6 + disp4);
                break;
            }
        }

        if (!pReloadShaders) {
            pReloadShaders = reinterpret_cast<std::atomic_bool*>(brdBase + 0x1ab531);
            pReloadMaterial = reinterpret_cast<void**>(brdBase + 0x1ab7e0);
        }
    }

    void update(ClientInstance* ci) {
        if (!m_initialized) init();
        if (!m_brdLoaded || !ci) return;

        // 1. 若 BetterRenderDragon 的 resourcePackManager 尚未被捕获，直接获取并注入
        if (pResourcePackManager && *pResourcePackManager == nullptr) {
            try {
                void* rpm = reinterpret_cast<void*>(&ci->getResourcePackManager());
                if (rpm) {
                    *pResourcePackManager = rpm;
                    if (pResourcePackManager_load) {
                        auto vptr = *reinterpret_cast<void***>(rpm);
                        if (vptr) {
                            *pResourcePackManager_load = vptr[3];
                        }
                    }
                }
            } catch (...) {}
        }

        // 2. 一旦 ResourcePackManager 就绪，且尚未触发初次着色器重载，立即激活重载
        // 使已加载材质库 (materials) 重读光影包中的 .material.bin，让光影立即生效！
        if (pResourcePackManager && *pResourcePackManager != nullptr && !m_initialReloadTriggered) {
            m_initialReloadTriggered = true;
            if (pReloadShaders) {
                *pReloadShaders = true;
            }
        }
    }

    void toggleMenu() {
        if (!m_initialized) init();
        if (pfnBRD_ToggleMenu) {
            try {
                pfnBRD_ToggleMenu();
            } catch (...) {}
        }
    }

    bool isMenuOpen() {
        if (!m_initialized) init();
        if (pfnBRD_IsMenuOpen) {
            try {
                return pfnBRD_IsMenuOpen();
            } catch (...) {}
        }
        return false;
    }
};
