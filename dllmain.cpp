#include <windows.h>
#include <gl/GL.h>

#include "../MinHook/include/MinHook.h"
#include "dr2c_memory.h"
#include "imgui_ui.h"

namespace {

using namespace dr2c;

using WglSwapBuffers = BOOL (WINAPI *)(HDC);
WglSwapBuffers g_originalSwapBuffers = nullptr;
Address g_swapTarget = 0;                      // 统一用 dr2c::Address 表示裸地址
HWND g_window = nullptr;
WNDPROC g_originalWindowProc = nullptr;
HANDLE g_initThread = nullptr;
HINSTANCE g_instance = nullptr;

LRESULT CALLBACK OverlayWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (HandleInternalWindowMessage(window, message, wParam, lParam))
        return 1;
    return g_originalWindowProc
        ? CallWindowProcW(g_originalWindowProc, window, message, wParam, lParam)
        : DefWindowProcW(window, message, wParam, lParam);
}

bool AttachWindow(HDC deviceContext)
{
    HWND window = WindowFromDC(deviceContext);
    if (!window)
        return false;
    if (window == g_window)
        return true;

    if (g_window && g_originalWindowProc) {
        SetWindowLongPtrW(g_window, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(g_originalWindowProc));
        g_originalWindowProc = nullptr;
    }
    g_window = window;
    g_originalWindowProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
        g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(OverlayWindowProc)));
    return InitializeInternalUi(g_window, g_instance);
}

BOOL WINAPI HookedSwapBuffers(HDC deviceContext)
{
    AttachWindow(deviceContext);
    RenderInternalUi();
    return g_originalSwapBuffers(deviceContext);
}

DWORD WINAPI InitializeThread(LPVOID)
{
    while (!GetModuleHandleW(L"opengl32.dll"))
        Sleep(50);
    FARPROC target = GetProcAddress(GetModuleHandleW(L"opengl32.dll"), "wglSwapBuffers");
    if (!target)
        return 0;

    if (MH_Initialize() != MH_OK)
        return 0;
    g_swapTarget = FnAddr(target);
    if (MH_CreateHook(Ptr(g_swapTarget), AsPtr<void>(FnAddr(&HookedSwapBuffers)),
                      reinterpret_cast<void **>(&g_originalSwapBuffers)) != MH_OK
        || MH_EnableHook(Ptr(g_swapTarget)) != MH_OK) {
        MH_Uninitialize();
        return 0;
    }

    return 0;
}

} // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        g_instance = instance;
        DisableThreadLibraryCalls(instance);
        g_initThread = CreateThread(nullptr, 0, InitializeThread, nullptr, 0, nullptr);
        if (g_initThread)
            CloseHandle(g_initThread);
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_swapTarget) {
            MH_DisableHook(Ptr(g_swapTarget));
            MH_RemoveHook(Ptr(g_swapTarget));
            g_swapTarget = 0;
        }
        if (g_window && g_originalWindowProc)
            SetWindowLongPtrW(g_window, GWLP_WNDPROC,
                              reinterpret_cast<LONG_PTR>(g_originalWindowProc));
        ShutdownInternalUi();
        MH_Uninitialize();
    }
    return TRUE;
}