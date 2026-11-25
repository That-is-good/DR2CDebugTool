#ifndef DR2C_AGENT_SERVER_H
#define DR2C_AGENT_SERVER_H

// ============================================================================
// agent_server.h
// Agent 管道服务的启动入口（由 DllMain 调用）。
// ============================================================================

#include <windows.h>

namespace dr2c {

// 创建进程级互斥体并启动管道服务线程；若已有实例则直接返回。
void StartServer(HINSTANCE instance);

} // namespace dr2c

#endif // DR2C_AGENT_SERVER_H
