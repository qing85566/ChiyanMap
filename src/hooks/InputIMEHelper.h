#pragma once
#include <windows.h>
#include <imm.h>
#pragma comment(lib, "imm32.lib")
#include <atomic>
#include <string>
#include <thread>
#include <mutex>
#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <imgui_internal.h>

namespace InputIMEHelper {
    inline HWND g_gameHwnd = nullptr;
    inline void SetGameHwnd(HWND hwnd) { g_gameHwnd = hwnd; }

    inline std::recursive_mutex s_stateMutex;
    inline std::atomic<bool> isTyping{false};
    inline std::atomic<bool> s_enterPressed{false};
    inline void* s_enterTargetBuf = nullptr;
    inline std::atomic<bool> s_valueChanged{false};
    inline void* s_valueChangeTargetBuf = nullptr;

    inline char* targetBuffer = nullptr;
    inline size_t targetBufferSize = 0;
    inline char localBuffer[4096] = {0};
    inline WNDPROC oEditProc = nullptr;
    inline HWND hImeWnd = nullptr;
    inline HWND hEdit = nullptr;
    inline std::string currentLabel = "";
    inline std::atomic<int> targetX{0}, targetY{0}, targetW{0}, targetH{0};
    inline std::atomic<int> s_lastSentX{0}, s_lastSentY{0}, s_lastSentW{0}, s_lastSentH{0};
    inline std::atomic<bool> updatePosPending{false};
    inline std::atomic<ImGuiInputTextFlags> s_activeFlags{0};
    inline std::atomic<uint64_t> s_lastActiveFrame{0};
    inline std::atomic<uint64_t> s_currentFrame{0};
    inline std::once_flag s_classRegisteredOnce;

    // 静态常驻 GDI 画刷缓存，彻底消除每帧与每次绘制消息的 GDI 句柄频繁创建/销毁开销
    inline HBRUSH GetBgBrush() {
        static HBRUSH hBrush = CreateSolidBrush(RGB(25, 29, 36));
        return hBrush;
    }

    inline HBRUSH GetBorderBrush() {
        static HBRUSH hBrush = CreateSolidBrush(RGB(65, 145, 235));
        return hBrush;
    }

    inline std::wstring Utf8ToUtf16(const std::string& utf8) {
        if (utf8.empty()) return L"";
        int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.length(), NULL, 0);
        if (sizeNeeded <= 0) return L"";
        std::wstring result(sizeNeeded, 0);
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.length(), &result[0], sizeNeeded);
        return result;
    }

    inline std::string Utf16ToUtf8(const std::wstring& utf16) {
        if (utf16.empty()) return "";
        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), (int)utf16.length(), NULL, 0, NULL, NULL);
        if (sizeNeeded <= 0) return "";
        std::string result(sizeNeeded, 0);
        WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), (int)utf16.length(), &result[0], sizeNeeded, NULL, NULL);
        return result;
    }

    inline bool IsActiveFor(const char* buf) {
        std::lock_guard<std::recursive_mutex> lock(s_stateMutex);
        return isTyping.load() && (targetBuffer == buf);
    }

    inline void Close() {
        std::lock_guard<std::recursive_mutex> lock(s_stateMutex);
        if (isTyping.load()) {
            // 确保在关闭和置空指针前，从原生编辑框获取最新文本，同步回目标缓冲区
            HWND hE = hEdit;
            char* tBuf = targetBuffer;
            size_t tSize = targetBufferSize;
            if (tBuf && tSize > 0) {
                if (hE && IsWindow(hE)) {
                    int len = GetWindowTextLengthW(hE);
                    if (len >= 0) {
                        std::wstring wStr(len + 1, 0);
                        GetWindowTextW(hE, &wStr[0], len + 1);
                        wStr.resize(len);
                        std::string u8Text = Utf16ToUtf8(wStr);
                        strncpy_s(tBuf, tSize, u8Text.c_str(), _TRUNCATE);
                        strncpy_s(localBuffer, sizeof(localBuffer), u8Text.c_str(), _TRUNCATE);
                    }
                } else {
                    strncpy_s(tBuf, tSize, localBuffer, _TRUNCATE);
                }
            }

            isTyping.store(false);
            targetBuffer = nullptr;
            targetBufferSize = 0;
            currentLabel.clear();
            s_lastSentX.store(0);
            s_lastSentY.store(0);
            s_lastSentW.store(0);
            s_lastSentH.store(0);
            HWND h = hImeWnd;
            hImeWnd = nullptr;
            hEdit = nullptr;
            if (h && IsWindow(h)) {
                PostMessageW(h, WM_CLOSE, 0, 0);
            }
            if (ImGui::GetCurrentContext()) {
                ImGui::ClearActiveID();
            }
        }
    }

    inline void Open(char*, size_t, const char*) {}

    inline void OnFrame() {
        uint64_t cur = s_currentFrame.fetch_add(1) + 1;
        if (isTyping.load() && cur > s_lastActiveFrame.load() + 10) {
            Close();
        }
    }

    inline LRESULT CALLBACK EditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        // 将 F1~F24 快捷键透传给游戏窗口 (例如 F11 全屏切换)
        if (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) {
            if (wParam >= VK_F1 && wParam <= VK_F24) {
                if (g_gameHwnd) PostMessageW(g_gameHwnd, msg, wParam, lParam);
                return 0;
            }
        }
        if (msg == WM_KILLFOCUS) {
            HWND hNewFocus = (HWND)wParam;
            if (hNewFocus != hwnd && hNewFocus != hEdit) {
                // 检查是否正在 IME 候选词输入中，若不在拼音输入状态则失焦脱离
                HIMC hImc = ImmGetContext(hwnd);
                bool isComposing = false;
                if (hImc) {
                    LONG compLen = ImmGetCompositionStringW(hImc, GCS_COMPSTR, NULL, 0);
                    if (compLen > 0) isComposing = true;
                    ImmReleaseContext(hwnd, hImc);
                }
                if (!isComposing) {
                    Close();
                }
            }
        }
        if (msg == WM_KEYDOWN) {
            if (wParam == VK_RETURN) { // 回车确认
                // 检查 IME 是否正在输入拼音/候选词中。如果是，回车用于确认拼音，不关闭输入框
                HIMC hImc = ImmGetContext(hwnd);
                bool isComposing = false;
                if (hImc) {
                    LONG compLen = ImmGetCompositionStringW(hImc, GCS_COMPSTR, NULL, 0);
                    if (compLen > 0) isComposing = true;
                    ImmReleaseContext(hwnd, hImc);
                }
                if (!isComposing) {
                    {
                        std::lock_guard<std::recursive_mutex> lock(s_stateMutex);
                        s_enterPressed.store(true);
                        s_enterTargetBuf = targetBuffer;
                    }
                    Close();
                    return 0;
                }
            } else if (wParam == VK_ESCAPE) { // Esc 取消/关闭
                if (s_activeFlags.load() & ImGuiInputTextFlags_EscapeClearsAll) {
                    SendMessageW(hwnd, WM_SETTEXT, 0, (LPARAM)L"");
                    std::lock_guard<std::recursive_mutex> lock(s_stateMutex);
                    localBuffer[0] = '\0';
                    if (targetBuffer) targetBuffer[0] = '\0';
                }
                Close();
                return 0;
            } else if (wParam == VK_TAB) { // Tab 完成
                Close();
                return 0;
            } else if (wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) { // Ctrl+A 全选
                SendMessageW(hwnd, EM_SETSEL, 0, -1);
                return 0;
            }
        }
        if (msg == WM_SETCURSOR) {
            SetCursor(LoadCursor(NULL, IDC_IBEAM));
            return TRUE;
        }
        return CallWindowProcW(oEditProc, hwnd, msg, wParam, lParam);
    }

    inline void UpdateItemPos(const ImVec2& pMin, const ImVec2& pMax) {
        if (!isTyping.load() || !g_gameHwnd) return;
        HWND h = hImeWnd;
        if (!h || !IsWindow(h)) return;

        static float s_cachedMinX = -9999.0f, s_cachedMinY = -9999.0f;
        static float s_cachedMaxX = -9999.0f, s_cachedMaxY = -9999.0f;
        static POINT s_cachedOrigin = { -9999, -9999 };

        POINT curOrigin = { 0, 0 };
        ClientToScreen(g_gameHwnd, &curOrigin);

        if (pMin.x == s_cachedMinX && pMin.y == s_cachedMinY &&
            pMax.x == s_cachedMaxX && pMax.y == s_cachedMaxY &&
            curOrigin.x == s_cachedOrigin.x && curOrigin.y == s_cachedOrigin.y)
        {
            return;
        }

        s_cachedMinX = pMin.x;
        s_cachedMinY = pMin.y;
        s_cachedMaxX = pMax.x;
        s_cachedMaxY = pMax.y;
        s_cachedOrigin = curOrigin;

        POINT pt = { curOrigin.x + (LONG)std::round(pMin.x), curOrigin.y + (LONG)std::round(pMin.y) };
        int w = (int)std::round(pMax.x - pMin.x);
        int h_box = (int)std::round(pMax.y - pMin.y);
        if (w > 0 && h_box > 0) {
            if (pt.x != s_lastSentX.load() || pt.y != s_lastSentY.load() ||
                w != s_lastSentW.load() || h_box != s_lastSentH.load())
            {
                s_lastSentX.store(pt.x);
                s_lastSentY.store(pt.y);
                s_lastSentW.store(w);
                s_lastSentH.store(h_box);
                targetX.store(pt.x);
                targetY.store(pt.y);
                targetW.store(w);
                targetH.store(h_box);
                updatePosPending.store(true);
                PostMessageW(h, WM_USER + 101, 0, 0);
            }
        }
    }

    inline void AttachToItem(const char* label, char* buf, size_t bufSize, const ImVec2& pMin, const ImVec2& pMax, const char* hint = nullptr, ImGuiInputTextFlags flags = 0) {
        if (!buf || bufSize == 0) return;
        if (!g_gameHwnd || !IsWindow(g_gameHwnd)) {
            g_gameHwnd = FindWindowW(L"Minecraft", NULL);
        }
        if (!g_gameHwnd) return;

        int w = 0, h = 0;
        POINT pt = {0, 0};
        {
            std::lock_guard<std::recursive_mutex> lock(s_stateMutex);
            if (isTyping.load()) {
                if (targetBuffer == buf && hImeWnd && IsWindow(hImeWnd)) return;
                Close();
            }

            pt = { (LONG)std::round(pMin.x), (LONG)std::round(pMin.y) };
            ClientToScreen(g_gameHwnd, &pt);
            POINT ptMax = { (LONG)std::round(pMax.x), (LONG)std::round(pMax.y) };
            ClientToScreen(g_gameHwnd, &ptMax);
            w = ptMax.x - pt.x;
            h = ptMax.y - pt.y;
            if (w < 10 || h < 10) return;

            targetBuffer = buf;
            targetBufferSize = bufSize;
            currentLabel = label ? label : "";
            s_activeFlags.store(flags);
            s_enterPressed.store(false);
            s_enterTargetBuf = nullptr;
            s_lastActiveFrame.store(s_currentFrame.load());
            strncpy_s(localBuffer, sizeof(localBuffer), buf ? buf : "", _TRUNCATE);
            isTyping.store(true);

            targetX.store(pt.x);
            targetY.store(pt.y);
            targetW.store(w);
            targetH.store(h);
            s_lastSentX.store(pt.x);
            s_lastSentY.store(pt.y);
            s_lastSentW.store(w);
            s_lastSentH.store(h);
        }

        // 立即中止 ImGui 内部对于此输入框的文本编辑状态机，
        // 防止 Dear ImGui 缓存初态并在失焦时将陈旧缓存回写覆写外部用户缓冲区
        if (ImGui::GetCurrentContext()) {
            ImGui::ClearActiveID();
        }

        std::thread([]() {
            HINSTANCE hInst = GetModuleHandle(NULL);
            std::call_once(s_classRegisteredOnce, [hInst]() {
                WNDCLASSW wc = {0};
                wc.style = CS_HREDRAW | CS_VREDRAW;
                wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
                    if (msg == WM_CREATE) {
                        RECT rc; GetClientRect(hwnd, &rc);
                        int cw = rc.right - rc.left;
                        int ch = rc.bottom - rc.top;

                        // 字体大小与高度设置：按输入框实际高度自适应计算，解除固定像素上限，完美适配高分辨率及 DPI 缩放
                        int fontH = -(int)std::max(13.0f, std::round((float)(ch - 4) * 0.72f));
                        HFONT font = CreateFontW(fontH, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
                        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)font);

                        // editY = 2 紧挨顶部 2px 边框；editH = ch - 4 刚好延伸至 ch - 2（留出底部 2px 边框）
                        // 为子控件提供充足高度，彻底解决文字削顶/削底显示不全的问题
                        int editY = 2;
                        int editH = std::max(12, ch - 4);
                        int editW = std::max(10, cw - 8);

                        hEdit = CreateWindowExW(0, L"EDIT", L"",
                            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT,
                            4, editY, editW, editH,
                            hwnd, (HMENU)1, NULL, NULL);

                        if (targetBufferSize > 1) {
                            SendMessageW(hEdit, EM_SETLIMITTEXT, (WPARAM)(targetBufferSize - 1), 0);
                        }

                        SendMessageW(hEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(1, 1));
                        SendMessageW(hEdit, WM_SETFONT, (WPARAM)font, MAKELPARAM(TRUE, 0));

                        std::wstring wInit;
                        {
                            std::lock_guard<std::recursive_mutex> lock(s_stateMutex);
                            wInit = Utf8ToUtf16(localBuffer);
                        }
                        SendMessageW(hEdit, WM_SETTEXT, 0, (LPARAM)wInit.c_str());
                        SendMessageW(hEdit, EM_SETSEL, (WPARAM)wInit.length(), (LPARAM)wInit.length());
                        oEditProc = (WNDPROC)SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)EditProc);

                    } else if (msg == WM_COMMAND) {
                        if (HIWORD(wParam) == EN_KILLFOCUS) {
                            HIMC hImc = ImmGetContext(hEdit ? hEdit : hwnd);
                            bool isComposing = false;
                            if (hImc) {
                                LONG compLen = ImmGetCompositionStringW(hImc, GCS_COMPSTR, NULL, 0);
                                if (compLen > 0) isComposing = true;
                                ImmReleaseContext(hEdit ? hEdit : hwnd, hImc);
                            }
                            if (!isComposing) {
                                Close();
                            }
                        } else if (HIWORD(wParam) == EN_CHANGE) {
                            static bool s_inChange = false;
                            if (s_inChange) return 0;
                            HWND hEditCtrl = (HWND)lParam;
                            int len = GetWindowTextLengthW(hEditCtrl);
                            if (len >= 0) {
                                std::wstring wStr(len + 1, 0);
                                GetWindowTextW(hEditCtrl, &wStr[0], len + 1);
                                wStr.resize(len);
                                std::string u8Text = Utf16ToUtf8(wStr);
                                std::lock_guard<std::recursive_mutex> lock(s_stateMutex);
                                if (targetBufferSize > 1 && u8Text.length() >= targetBufferSize) {
                                    s_inChange = true;
                                    while (!wStr.empty() && Utf16ToUtf8(wStr).length() >= targetBufferSize) {
                                        wStr.pop_back();
                                    }
                                    u8Text = Utf16ToUtf8(wStr);
                                    SendMessageW(hEditCtrl, WM_SETTEXT, 0, (LPARAM)wStr.c_str());
                                    SendMessageW(hEditCtrl, EM_SETSEL, (WPARAM)wStr.length(), (LPARAM)wStr.length());
                                    s_inChange = false;
                                }
                                strncpy_s(localBuffer, sizeof(localBuffer), u8Text.c_str(), _TRUNCATE);
                                if (targetBuffer && targetBufferSize > 0) {
                                    strncpy_s(targetBuffer, targetBufferSize, u8Text.c_str(), _TRUNCATE);
                                }
                                s_valueChanged.store(true);
                                s_valueChangeTargetBuf = targetBuffer;
                            }
                        }
                    } else if (msg == WM_USER + 101) {
                        if (updatePosPending.exchange(false)) {
                            int nx = targetX.load();
                            int ny = targetY.load();
                            int nw = targetW.load();
                            int nh = targetH.load();
                            SetWindowPos(hwnd, NULL, nx, ny, nw, nh, SWP_NOZORDER | SWP_NOACTIVATE);
                            if (hEdit) {
                                int editY = 2;
                                int editH = std::max(12, nh - 4);
                                int editW = std::max(10, nw - 8);
                                SetWindowPos(hEdit, NULL, 4, editY, editW, editH, SWP_NOZORDER | SWP_NOACTIVATE);
                            }
                            InvalidateRect(hwnd, NULL, TRUE);
                        }
                        return 0;
                    } else if (msg == WM_ERASEBKGND) {
                        return 1;
                    } else if (msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORSTATIC) {
                        HDC hdc = (HDC)wParam;
                        SetTextColor(hdc, RGB(240, 240, 240)); 
                        SetBkColor(hdc, RGB(25, 29, 36));      
                        return (LRESULT)GetBgBrush();
                    } else if (msg == WM_PAINT) {
                        PAINTSTRUCT ps;
                        HDC hdc = BeginPaint(hwnd, &ps);
                        RECT rc; GetClientRect(hwnd, &rc);
                        int cw = rc.right - rc.left;
                        int ch = rc.bottom - rc.top;

                        // 1. 填充深色背景 (复用静态常驻画刷)
                        FillRect(hdc, &rc, GetBgBrush());

                        // 2. 绘制 2px 实心加厚蓝色聚焦边框，确保四周 100% 闭合
                        HBRUSH borderBrush = GetBorderBrush();
                        RECT rTop = { 0, 0, cw, 2 };
                        RECT rBottom = { 0, ch - 2, cw, ch };
                        RECT rLeft = { 0, 0, 2, ch };
                        RECT rRight = { cw - 2, 0, cw, ch };
                        FillRect(hdc, &rTop, borderBrush);
                        FillRect(hdc, &rBottom, borderBrush);
                        FillRect(hdc, &rLeft, borderBrush);
                        FillRect(hdc, &rRight, borderBrush);

                        EndPaint(hwnd, &ps);
                        return 0;
                    } else if (msg == WM_KILLFOCUS || msg == WM_ACTIVATE) {
                        if (msg == WM_KILLFOCUS || (msg == WM_ACTIVATE && LOWORD(wParam) == WA_INACTIVE)) {
                            HWND hCurFocus = GetFocus();
                            if (hCurFocus != hwnd && hCurFocus != hEdit) {
                                Close();
                            }
                        }
                    } else if (msg == WM_SETCURSOR) {
                        SetCursor(LoadCursor(NULL, IDC_IBEAM));
                        return TRUE;
                    } else if (msg == WM_CLOSE) {
                        Close();
                        HFONT font = (HFONT)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
                        if (font) {
                            DeleteObject(font);
                            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                        }
                        DestroyWindow(hwnd);
                        PostQuitMessage(0);
                        return 0;
                    }
                    return DefWindowProcW(hwnd, msg, wParam, lParam);
                };

                wc.hInstance = hInst;
                wc.lpszClassName = L"ChiyanInPlaceIME";
                wc.hCursor = LoadCursor(NULL, IDC_IBEAM);
                wc.hbrBackground = NULL;
                RegisterClassW(&wc);
            });

            // 使用 WS_POPUP | WS_CLIPCHILDREN：
            // WS_CLIPCHILDREN 确保父窗口绘制时绝不覆盖子控件 EDIT，彻底消除文本重绘闪烁
            HWND hwnd = CreateWindowExW(
                WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                L"ChiyanInPlaceIME", L"",
                WS_POPUP | WS_CLIPCHILDREN,
                targetX.load(), targetY.load(), targetW.load(), targetH.load(),
                g_gameHwnd, NULL, hInst, NULL
            );

            hImeWnd = hwnd;
            ShowWindow(hwnd, SW_SHOW);
            UpdateWindow(hwnd);
            SetForegroundWindow(hwnd);
            SetFocus(GetDlgItem(hwnd, 1));

            MSG msg;
            while (GetMessageW(&msg, NULL, 0, 0)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }

            if (hImeWnd == hwnd) {
                hImeWnd = nullptr;
                hEdit = nullptr;
            }
        }).detach();
    }

    inline bool InputTextWithIME(const char* label, char* buf, size_t bufSize, const char* hint = nullptr, ImGuiInputTextFlags flags = 0) {
        bool enterPressed = false;
        if (s_enterPressed.load() && (s_enterTargetBuf == buf || s_enterTargetBuf == nullptr)) {
            std::lock_guard<std::recursive_mutex> lock(s_stateMutex);
            if (s_enterPressed.load() && (s_enterTargetBuf == buf || s_enterTargetBuf == nullptr)) {
                s_enterPressed.store(false);
                s_enterTargetBuf = nullptr;
                enterPressed = true;
            }
        }

        bool textChanged = false;
        if (s_valueChanged.load() && (s_valueChangeTargetBuf == buf || s_valueChangeTargetBuf == nullptr)) {
            std::lock_guard<std::recursive_mutex> lock(s_stateMutex);
            if (s_valueChanged.load() && (s_valueChangeTargetBuf == buf || s_valueChangeTargetBuf == nullptr)) {
                s_valueChanged.store(false);
                s_valueChangeTargetBuf = nullptr;
                textChanged = true;
            }
        }

        bool isActiveOnThis = IsActiveFor(buf);

        if (isActiveOnThis) {
            std::lock_guard<std::recursive_mutex> lock(s_stateMutex);
            // 实时保持外部 buf 与正在输入的 localBuffer 保持完全一致（单指令快速前缀对比加速）
            if (buf && bufSize > 0 && (buf[0] != localBuffer[0] || strcmp(buf, localBuffer) != 0)) {
                strncpy_s(buf, bufSize, localBuffer, _TRUNCATE);
            }
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.12f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.26f, 0.57f, 0.92f, 1.0f));
        }

        bool valChanged = false;
        if (isActiveOnThis) {
            // 当原生输入框激活时：
            // 1. 传入独立的虚拟空缓冲区，彻底杜绝 Dear ImGui 触碰外部 buf 或在其内部缓存陈旧文本
            // 2. 传入 ReadOnly 标志，确保 ImGui 仅作为尺寸与布局外观容器，绝不参与按键处理
            // 3. 不绘制 hint 占位符，避免在原生输入框绘制前产生任何文字重叠
            static char s_dummyActiveBuf[1] = {0};
            ImGuiInputTextFlags effectiveFlags = flags | ImGuiInputTextFlags_ReadOnly;
            valChanged = ImGui::InputText(label, s_dummyActiveBuf, sizeof(s_dummyActiveBuf), effectiveFlags);
            ImGui::PopStyleColor(2);
        } else {
            if (hint && hint[0] != '\0') {
                valChanged = ImGui::InputTextWithHint(label, hint, buf, bufSize, flags);
            } else {
                valChanged = ImGui::InputText(label, buf, bufSize, flags);
            }
        }

        // 精确获取输入框边框区域（排除右侧文字标签）
        ImVec2 pMin = ImGui::GetItemRectMin();
        ImVec2 pMax = ImGui::GetItemRectMax();
        if (ImGuiContext* ctx = ImGui::GetCurrentContext()) {
            if (ctx->LastItemData.NavRect.GetWidth() > 0.0f && ctx->LastItemData.NavRect.GetHeight() > 0.0f) {
                pMin = ctx->LastItemData.NavRect.Min;
                pMax = ctx->LastItemData.NavRect.Max;
            }
        }

        if (isActiveOnThis) {
            // 当用户在当前输入框以外的区域点击鼠标时，脱离输入框光标
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                ImVec2 mousePos = ImGui::GetMousePos();
                if (mousePos.x < pMin.x || mousePos.x > pMax.x || mousePos.y < pMin.y || mousePos.y > pMax.y) {
                    Close();
                    isActiveOnThis = false;
                }
            }
        }

        // 当用户点击或激活输入框时，就地无缝唤起原生输入控件
        if (!isActiveOnThis && (ImGui::IsItemActivated() || (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)))) {
            AttachToItem(label, buf, bufSize, pMin, pMax, hint, flags);
        }

        if (IsActiveFor(buf)) {
            s_lastActiveFrame.store(s_currentFrame.load());
            UpdateItemPos(pMin, pMax);
        }

        if (flags & ImGuiInputTextFlags_EnterReturnsTrue) {
            return enterPressed || valChanged;
        }
        return textChanged || valChanged;
    }

    inline bool InputTextWithIME(const char* label, char* buf, size_t bufSize, ImGuiInputTextFlags flags) {
        return InputTextWithIME(label, buf, bufSize, nullptr, flags);
    }
}
