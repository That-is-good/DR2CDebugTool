#ifndef DR2C_AGENT_RESOLVER_H
#define DR2C_AGENT_RESOLVER_H

// ============================================================================
// agent_resolver.h
// 特征码解析器（原 Qt 端 GameResolver 的等价实现），运行在游戏进程内部。
// 输入游戏主模块基址，输出 LayoutInfo（RVA 表、实体池、精灵表、脚本全局等）。
// ============================================================================

#include "../Common/protocol.h"

#include <cstdint>

namespace dr2c {

// 解析成功返回 true；失败时 layout->valid=0，并在 layout->error 中给出原因。
bool ResolveLayout(std::uint32_t moduleBase, LayoutInfo *layout);

} // namespace dr2c

#endif // DR2C_AGENT_RESOLVER_H
