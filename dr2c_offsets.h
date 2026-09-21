#ifndef DR2C_OFFSETS_H
#define DR2C_OFFSETS_H

// ============================================================================
// dr2c_offsets.h
// 游戏内存布局的“唯一声明处”。所有数值都从原 imgui_ui.cpp 中的字面量原样搬运，
// 不修改任何偏移值。依据 Struct/full_struct.cpp、Struct/struct.cpp 命名。
//
// 地址约定：模块基址默认 0x400000。
//   RVA = 相对模块基址的偏移（本工程使用，等于“实际地址 - 模块基址”）
//   VA  = 0x400000 + RVA（文档 Struct/ 中函数地址用的是 VA，已逐项标注）
// ============================================================================

#include "dr2c_memory.h"

namespace dr2c {
namespace Offset {

// ============================ 全局变量（均为 RVA） ============================
namespace Global {
constexpr Rva ThingPool   = 0x5632E0u;   // thing 实体池（stride 0x304，最多 0x262，索引 0 不使用）
constexpr Rva WeaponPool  = 0x4E0080u;   // weapon 武器池（stride 0x1C4，最多 0x401）
constexpr Rva CameraX     = 0x610340u;   // 相机 X
constexpr Rva CameraY     = 0x610344u;   // 相机 Y
constexpr Rva ScreenScale = 0x0D14ACu;   // 屏幕缩放
constexpr Rva MapLayer    = 0x5D6294u;   // 当前地图层（读 1 字节 / 写 4 字节，保持现状不变）
}

// ============================ 游戏函数（均为 RVA） ============================
namespace Fn {
constexpr Rva AllocateEntity        = 0x52710u;    // 文档 VA 0x452710
constexpr Rva FreeThing             = 0x52AA0u;    // 文档 VA 0x452AA0
constexpr Rva GetCharacterData      = 0x28AE0u;
constexpr Rva GetFrameRate          = 0x04C50u;
constexpr Rva SetFrameRate          = 0x04B30u;
constexpr Rva SetCurrentPlayerThing = 0x054A20u;
}

// ============================ struct thing ============================
// 池偏移 0x5632E0、大小 0x304、最多 0x262 个、索引 0 不使用
namespace Thing {
constexpr Rva Id         = 0x00;    // ushort
constexpr Rva Type       = 0x02;    // byte
constexpr Rva Subtype    = 0x03;    // byte
constexpr Rva MapId      = 0x04;    // byte（区域 ID）
constexpr Rva NoCollide  = 0x0D;    // byte
constexpr Rva NoPick     = 0x11;    // byte
constexpr Rva Unseen     = 0x12;    // byte
constexpr Rva Invisible  = 0x13;    // byte
constexpr Rva Position   = 0x2C;    // Vector3D
constexpr Rva Velocity   = 0x38;    // Vector3D
constexpr Rva Physics    = 0x58;    // mass / friction / bounce_friction 连续 3 个 float
constexpr Rva Glow       = 0x70;    // 本工程按 1 字节读写（保持现状不变）
constexpr Rva SpriteId   = 0xD8;    // ushort
constexpr Rva CharId     = 0x148;   // uint
constexpr Rva ZombieType = 0x14C;   // uint
constexpr Rva Hitpoints  = 0x254;   // int
constexpr Rva NoHit      = 0x27A;   // byte
constexpr Rva AiState    = 0x288;   // uint

constexpr std::size_t Stride   = 0x304u;
constexpr int         MaxSlots = 610;   // 0x262
}

// ============================ struct thing：车辆字段 ============================
namespace Vehicle {
constexpr Rva Chassis    = 0x208;   // sbyte
constexpr Rva ChassisMax = 0x209;   // sbyte
constexpr Rva Engine     = 0x20A;   // sbyte
constexpr Rva EngineMax  = 0x20B;   // sbyte
constexpr Rva Armour     = 0x20C;   // sbyte
constexpr Rva ArmourMax  = 0x20D;   // sbyte
constexpr Rva Speed      = 0x20E;   // sbyte
constexpr Rva SpeedMax   = 0x20F;   // sbyte
constexpr Rva Repair     = 0x210;   // int
constexpr Rva Mpg        = 0x214;   // float
}

// ============================ struct thing：物品字段 ============================
namespace Item {
constexpr Rva Amount = 0xE4;        // int
constexpr Rva Loot   = 0xE8;        // byte（数值对应 Resources）
}

// ============================ struct character ============================
// 大小 0x2E0、最多 0x100 个
namespace Char {
constexpr Rva Name        = 0x01C;  // char[40]
constexpr Rva Perk        = 0x044;  // char[40]
constexpr Rva Trait       = 0x06C;  // char[40]
constexpr Rva Female      = 0x094;  // ushort
constexpr Rva Pet         = 0x096;  // ushort
constexpr Rva Health      = 0x140;  // uint
constexpr Rva Description = 0x144;  // char[120]
constexpr Rva BaseStat    = 0x1C9;  // sbyte[13]
constexpr Rva BonusStat   = 0x1E3;  // sbyte[13]
constexpr Rva SpeedBonus  = 0x1F0;  // float
constexpr Rva Resource    = 0x288;  // int[8]（localResource）
constexpr Rva WeaponSlot  = 0x2B0;  // WeaponSlots3D[3]

constexpr std::size_t NameLength        = 40;
constexpr std::size_t PerkLength        = 40;
constexpr std::size_t TraitLength       = 40;
constexpr std::size_t DescriptionLength = 120;
constexpr int         StatCount         = 13;
constexpr int         ResourceCount     = 8;
constexpr int         WeaponSlotCount   = 3;
}

// ============================ struct WeaponSlot3D ============================
namespace WeaponSlot {
constexpr Rva Stack  = 0x00;        // int  数量
constexpr Rva Id     = 0x04;        // int  武器 ID
constexpr Rva Lock   = 0x08;        // int  是否可丢
constexpr std::size_t Size = 12;
}

// ============================ struct weapon ============================
// 池偏移 0x4E0080、大小 0x1C4、最多 0x401 个
namespace Weapon {
constexpr Rva Name = 0x00;          // char[40]
constexpr std::size_t NameLength = 40;
constexpr std::size_t Stride     = 0x1C4u;
constexpr int         MaxCount   = 0x401;
}

} // namespace Offset
} // namespace dr2c

#endif
