#include "agent_server.h"

#include <windows.h>

// ============================================================================
// dllmain.cpp
// 注入入口：只做线程库初始化和启动管道服务，不创建任何窗口/hook。
// ============================================================================

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        dr2c::StartServer(instance);
    }
    return TRUE;
}
