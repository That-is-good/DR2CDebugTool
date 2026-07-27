#include <stdint.h>

enum THING_TYPE{
    HUMAN = 1,
    ZOMBIE,
    ITEM,
    PROJECTILE
};
enum THING_SUBTYPE{
    FURNITURE = 0,
    PICKUP,
    WEAPON,
    VEHICLE,
    PICKUP_SPEC
};

// 实体 结构体
// 实体池偏移0x5632E0
// 大小: 0x304
// 最多: 0x262个实体
struct thing {
    // -------- 标识与标志 --------
    uint16_t id; // 0x00
    union
    {
        uint8_t filter[3];
        struct
        {
            uint8_t  type; // 0x02 类型
            uint8_t  subtype; // 0x03 子类型
            uint8_t  mapid; // 0x04 地图ID
        } Filter;
    };
    uint8_t  nocollide; // 0x0D 无碰撞
    uint8_t  unseen; // 0x12 不可见
    uint8_t  invisible; // 0x13 不绘制

    // -------- 位置与速度 --------
    union
    {
        float pos[3]; // 0x2c
        struct {
            float x; // 0x2c
            float y; // 0x30
            float z; // 0x34
        } Pos;
    };
    
    union
    {
        float vel[3]; // 0x38
        struct {
            float vel_x; // 0x38
            float vel_y; // 0x3c
            float vel_z; // 0x40
        } Vel;
    };

    union
    {
        float phy[3]; // 0x58
        struct {
            float mass; // 0x58: 质量
            float friction; // 0x5c: 阻力
            float bounce_friction; // 0x60: 弹跳阻力
        } Phy;
    };

    uint8_t fade; // 0x64 淡去
    uint8_t no_lighting; // 0x6f 不发光
    uint8_t glow; // 0x70 发光

    // -------- 状态与标志 --------
    int32_t hitpoints; // 0x254 生命值
    uint8_t  no_hit; // 0x27A 不可被击中
    uint8_t  no_do_damage; // 0x27B 不可被伤害
    uint8_t  pause; // 0x27E 暂停

    // -------- AI --------
    uint32_t ai_state; // 0x288 AI状态
    uint32_t ai_wait; // 0x2A8 AI发呆
};

enum Status0{
    SICK = 1,
    INJURED = 2,
    TIRED = 4,
    SUPERDOG = 8,
    DOGMALUS = 16,
    COFFEE = 32,
    DOGPAL = 64,
    EXPLORER = 128,
};

struct CommonResource
{
    int32_t resource0;
    int32_t food;
    int32_t gas;
    int32_t medical;
    int32_t bullet;
    int32_t rifle;
    int32_t shell;
};

struct CharacterStat
{
    int8_t morale;
    int8_t attitude;
    int8_t composure;
    int8_t charm;
    int8_t wits;
    int8_t loyalty;
    int8_t medical;
    int8_t mechanical;
    int8_t shooting;
    int8_t strength;
    int8_t dexterity;
    int8_t fitness;
    int8_t vitality;
};
// 角色 结构体
// 角色偏移0x5E25D8
// 大小: 0x2e0
// 最多: 0x100个角色
struct character {
    // -------- 当前 Thing ID --------
    uint16_t cur_thingid;            // 0x04 该角色对应的Thing ID
    // -------- 基础 --------
    char name[40];                   // 0x1c 名字
    char perk[40];                   // 0x44 特征
    char trait[40];                  // 0x6c 特长
    uint16_t female;                 // 0x94 性别
    int32_t health;                 // 0x140 血量
    char description[120];           // 0x144 描述

    // --------  属性 --------
    union
    {
        int8_t display_stat[13]; // 0x1bc 属性是否已知
        CharacterStat displayStat;
    };
    
    union
    {
        int8_t base_stats[13]; // 0x1c9 基础属性
        CharacterStat baseStat;
    };

    union
    {
        int8_t temp_stats[13]; // 0x1d6 临时属性
        CharacterStat tempStat;
    };
    
    union
    {
        int8_t bonus_stats[13]; // 0x1e3 附加属性
        CharacterStat bonusStat;
    };
    float    speed_bonus; // 0x1f0 额外速度
    uint8_t status; // 0x1f8 状态

    // -------- 资源  --------
    union
    {
        int32_t resource[7]; // 0x288 资源
        CommonResource Resource;
    };
    
    union
    {
        int32_t weapon[3];
        struct
        {
            int32_t Stack; //数量
            int32_t ID; //武器ID
            int32_t Lock; //是否可丢
        } Weapon;
    }Weapon_slots[3];// 0x2b0
};
// 武器 结构体
// 大小: 0x1c4
// 武器池偏移 0x4E0080;
// 最多0x401个武器
// 因为武器只有名字有修改意义，所以不如直接读取名字
struct weapon {
    char name[40]; // 0x00 武器名称
};

// MISSION STATE STRUCTURE (本局游戏状态)
// 起始: 0x5E2238
// 大小: 因为没有数组，所以无所谓
struct mission_state {
    // -------- 玩家/队伍信息 --------
    uint32_t player_char[4]; // 0x18 当前队伍角色在角色中的顺序, 从1开始
    union
    {
        int32_t resource[7]; // 0x28 资源
        CommonResource Resource;
    };
    
    // -------- 仓库武器槽 --------
    union
    {
        int32_t weapon[2];
        struct
        {
            int32_t ID; // 武器 ID (0=空)
            int32_t Stack; // 数量
        } Weapon;
    }Storage_slots[15];                // 0x48
};