#ifndef DR2C_AGENT_OFFSETS_H
#define DR2C_AGENT_OFFSETS_H

// ============================================================================
// agent_offsets.h
// 与游戏版本无关的固定偏移（RVA，模块基址默认 0x400000）。
// 全部数值来自 Inner 工程 dr2c_offsets.h，未做任何修改。
// 只保留 Agent 实际用到的部分（相机、缩放、实体字段）。
// ============================================================================

#include <cstdint>

namespace dr2c {
namespace Offset {

constexpr std::uint32_t CameraX     = 0x610340u; // float 相机 X
constexpr std::uint32_t CameraY     = 0x610344u; // float 相机 Y
constexpr std::uint32_t ScreenScale = 0x0D14ACu; // float 屏幕缩放

namespace Thing {
constexpr std::uint32_t Id       = 0x00; // ushort
constexpr std::uint32_t Type     = 0x02; // byte
constexpr std::uint32_t Subtype  = 0x03; // byte
constexpr std::uint32_t MapId    = 0x04; // byte
constexpr std::uint32_t Position = 0x2C; // float[3]
constexpr std::uint32_t SpriteId = 0xD8; // ushort
constexpr std::uint32_t Stride   = 0x304u;
constexpr std::uint32_t MaxSlots = 610u;
}

// 悬停拾取半径（世界单位），与 Inner 的 g_pickRadiusWorld 默认值一致
constexpr float PickRadiusWorld = 8.0f;
// 高亮方框最小屏幕像素半径
constexpr float MinHighlightPx = 18.0f;

} // namespace Offset
} // namespace dr2c

#endif // DR2C_AGENT_OFFSETS_H
