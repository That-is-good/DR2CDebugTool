#include <stdint.h>

// ============================================================
// 常量定义
// ============================================================

// Thing 类型 ([thing+0x02])
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

// Thing 事件
#define THING_EVENT_NEW       1
#define THING_EVENT_PICKUP    2
#define THING_EVENT_DROP      3
#define THING_EVENT_ACTION    4
#define THING_EVENT_BREAK     6
#define THING_EVENT_DIED      7
#define THING_EVENT_WAS_HIT   9
#define THING_EVENT_BUMP      16
#define THING_EVENT_PUSH      17
#define THING_EVENT_TIMER     28

// 实体 结构体
// 实体池偏移0x5632E0
// 大小: 0x304
// 最多: 0x262个实体
struct thing {
    // -------- 0x00-0x1b: 标识与标志 --------
    uint16_t id;                 // 0x00: Thing ID (未注册)
    uint8_t  typeid;             // 0x02: 类型 (THING_HUMAN=1, ZOMBIE=2, ITEM=3, PROJECTILE=4)
    uint8_t  subtypeid;          // 0x03: 子类型 (ITEM专用: 0=拾取,1=特殊,2=武器,3=弹药,4=家具)
    uint8_t  mapid;              // 0x04: 区域ID
    
    uint8_t  _pad_0x05[3];       // 0x05-0x07: 填充

    uint8_t  flip;               // 0x08: 翻转
    uint8_t  walkover;           // 0x09: 可走过
    uint8_t  low_profile;        // 0x0a: 低轮廓
    uint8_t  layer;              // 0x0b: 图层
    uint8_t  tileflags;          // 0x0c: 图块标志
    uint8_t  nocollide;          // 0x0d: 无碰撞 (可穿墙)
    uint8_t  bolted;             // 0x0e: 固定
    uint8_t  boltstr;            // 0x0f: 固定强度
    uint8_t  nopush;             // 0x10: 不可推动
    uint8_t  nopick;             // 0x11: 不可拾取
    uint8_t  unseen;             // 0x12: 不渲染
    uint8_t  invisible;          // 0x13: 不绘制
    uint8_t  noshadow;           // 0x14: 无阴影

    uint8_t  _pad_0x15[1];       // 0x15: 填充
    
    uint8_t  nodust;             // 0x16: 无灰尘

    uint8_t  _pad_0x17[1];       // 0x17: 填充

    float    wind;               // 0x18: 风力影响
    uint8_t  flies;              // 0x1c: 苍蝇 

    uint8_t  _pad_0x1d[3];       // 0x1d-0x1f: 填充

    // -------- 0x20-0x2b: 旧位置 --------
    union
    {
       float oldpos[3];
       struct
       {
            float oldpos_x; // 0x20
            float oldpos_y; // 0x24
            float oldpos_z; // 0x24
       }OldPos;
       
    };

    // -------- 0x2c-0x43: 位置与速度 --------
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

    // -------- 0x44-0x57: 角度与物理 --------
    float    angle;              // 0x44
    float    rot;                // 0x48: 旋转
    float    gravity;            // 0x4c
    uint16_t overlap_id;         // 0x50
    uint16_t interact_id;        // 0x52
    uint16_t inside_id;          // 0x54
    uint16_t just_hit_id;        // 0x56

    // -------- 0x58-0x83: 物理属性与计时器 --------
    union
    {
        float phy[3]; // 0x58
        struct {
            float mass; // 0x58: 质量
            float friction; // 0x5c: 阻力
            float bounce_friction; // 0x60: 弹跳阻力
        } Phy;
    };
    uint8_t  fade;               // 0x64

    uint8_t  _pad_0x65[3];       // 0x65-0x67: 填充

    uint32_t burning;            // 0x68: 燃烧状态
    uint8_t  flammability;       // 0x6c
    uint8_t  spreadability;      // 0x6d
    uint8_t  reburn;             // 0x6e
    uint8_t  no_lighting;        // 0x6f
    uint8_t  glow;               // 0x70: 发光开关

    uint8_t  _pad_0x71[7];       // 0x71-0x77: 填充

    uint32_t activity_timer;     // 0x78: 活动计时器 (递减)
    uint16_t thingseed;          // 0x7c
    uint16_t timer;              // 0x7e
    uint16_t timer_hint;         // 0x80
    uint16_t timer_base;         // 0x82

    // -------- 0x84-0xa3: 浮点计时器与UI --------
    float    timer_float;        // 0x84

    uint8_t  _pad_0x88[4];       // 0x88-0x8b: 填充

    uint32_t field_0x8c;         // 0x8c (handler)
    uint8_t  leaveok;            // 0x90

    uint8_t  _pad_0x91[3];       // 0x91-0x93: 填充

    uint16_t cyoa;               // 0x94
    uint16_t events;             // 0x98
    uint16_t action;             // 0x9c
    uint16_t user_word;          // 0xa0
    uint32_t action_icon;        // 0xa4
    uint32_t inaction_icon;      // 0xa8

    // -------- 0xac-0xdf: 碰撞箱、颜色、精灵 --------
    float    rad_x;              // 0xac
    float    rad_y;              // 0xb0
    float    rad_z;              // 0xb4
    float    wallbox_x;          // 0xb8
    float    wallbox_y;          // 0xbc
    float    clr_r;              // 0xc0
    float    clr_g;              // 0xc4
    float    clr_b;              // 0xc8
    float    clr_a;              // 0xcc
    float    scale_x;            // 0xd0
    float    scale_y;            // 0xd4
    uint16_t spriteid;           // 0xd8
    uint8_t  uncentered;         // 0xda
    uint8_t  itemtype;           // 0xdc
    uint8_t  openable;           // 0xde
    uint8_t  item_opened;        // 0xdf

    // -------- 0xe0-0xf7: 物品状态与命中检测模式 --------
    uint8_t  item_locked;        // 0xe0

    uint8_t  _pad_0xe1[3];       // 0xe1-0xe3: 填充

    int32_t  amount;             // 0xe4
    uint8_t  loot;               // 0xe8
    uint8_t  item_status;        // 0xe9

    uint8_t  _pad_0xea[2];       // 0xea-0xeb: 填充

    uint32_t item_hint;          // 0xec
    uint32_t item_charges;       // 0xf0
    
    uint8_t  _pad_0xf4[4];       // 0xf4-0xf7: 填充

    float    hitcheck_mode;      // 0xf8

    // -------- 0xfc-0x147: 命中检测 --------
    uint32_t hitcheck_dist;          // 0xfc
    uint32_t hitcheck_hits_allowed;  // 0x100
    uint32_t hitcheck_closest_dist;  // 0x104
    uint32_t hitcheck_blast;         // 0x108

    uint8_t  _pad_0x10c[8];          // 0x10c-0x113: 填充

    uint16_t hitcheck_source_id;     // 0x114
    uint16_t hitcheck_lasthit_id;    // 0x116
    uint16_t hitcheck_closest_id;    // 0x118

    uint8_t  _pad_0x11a[2];          // 0x11a-0x11b: 填充

    float    hitcheck_rad;           // 0x11c
    float    hitcheck_power;         // 0x120
    float    hitcheck_power_max;     // 0x124
    float    hitcheck_knockback;     // 0x128
    float    hitcheck_shoot_thru;    // 0x12c
    float    hitcheck_dir_x;         // 0x130
    float    hitcheck_dir_y;         // 0x134
    float    hitcheck_pos_x;         // 0x138
    float    hitcheck_pos_y;         // 0x13c

    uint8_t  _pad_0x140[4];          // 0x140-0x143: 填充

    uint32_t charid;                 // 0x148
    uint32_t zombietype;             // 0x14c
    uint16_t shooterid;              // 0x150
    uint8_t  legframe;               // 0x152
    uint8_t  turnframe;              // 0x153
    uint8_t  faceframe;              // 0x154
    uint8_t  aiming_mode;            // 0x155

    uint8_t  _pad_0x156[2];          // 0x156-0x157: 填充

    float    move_angle;             // 0x158
    float    swing_angle;            // 0x15c

    uint8_t  _pad_0x160[4];          // 0x160-0x163: 填充

    float    aim_angle;              // 0x164
    float    lock_angle;             // 0x168
    float    move_dir_x;             // 0x16c
    float    move_dir_y;             // 0x170

    uint8_t  _pad_0x174[0x18];       // 0x174-0x18b: 填充

    uint32_t offscreen_counter;      // 0x18c
    uint8_t  shudder;                // 0x190

    uint8_t  _pad_0x191[3];          // 0x191-0x193: 填充

    float    shudderpuff_x;          // 0x194
    float    shudderpuff_y;          // 0x198

    uint8_t  _pad_0x19c[3];          // 0x19c-0x19e: 填充

    uint8_t  weapon_switch;          // 0x19f
    uint8_t  weapon_slot_using;      // 0x1a0
    uint8_t  weapon_swap_select;     // 0x1a1

    uint8_t  _pad_0x1a2[3];          // 0x1a2-0x1a4: 填充

    uint8_t  weapon_no_hit_human;    // 0x1a5
    uint16_t nearest_enemy_id;       // 0x1a6
    uint32_t nearest_enemy_dist2;    // 0x1a8

    uint8_t  _pad_0x1ac[0x20];       // 0x1ac-0x1cb: 填充
    
    uint16_t nearest_interact_id;    // 0x1cc
    uint32_t nearest_interact_dist2; // 0x1d0
    uint16_t nearest_pickup_id;      // 0x1d4

    uint8_t  _pad_0x1d6[2];          // 0x1d6-0x1d7: 填充

    float    threat_level;           // 0x1d8
    float    threat_dist2;           // 0x1dc
    uint32_t threat_count;           // 0x1e0
    float    threat_pos_x;           // 0x1e4
    float    threat_pos_y;           // 0x1e8
    uint16_t targetid;               // 0x1ec

    uint8_t  _pad_0x1ee[2];          // 0x1ee-0x1ef: 填充

    float    targetpos_x;            // 0x1f0
    float    targetpos_y;            // 0x1f4
    float    destpos_x;              // 0x1f8
    float    destpos_y;              // 0x1fc
    uint16_t carryid;                // 0x200
    uint16_t carrierid;              // 0x202
    uint16_t throwerid;              // 0x204

    uint8_t  _pad_0x206[2];          // 0x206-0x207: 填充

    // -------- 0x208-0x227: 武器状态/车辆属性 (共用) --------
    uint8_t  chassis;                // 0x208 (与 weapon_state.hint 共用)
    uint8_t  chassis_max;            // 0x209 (与 weapon_state.info 共用)
    uint8_t  engine;                 // 0x20a (与 weapon_state.frame 共用)
    uint8_t  engine_max;             // 0x20b
    uint8_t  armour;                 // 0x20c (与 weapon_state.val 共用)
    uint8_t  armour_max;             // 0x20d
    uint8_t  carspeed;               // 0x20e
    uint8_t  carspeed_max;           // 0x20f
    uint8_t  repair;                 // 0x210 (与 weapon_state.scale 共用)

    uint8_t  _pad_0x211[3];          // 0x211-0x213: 填充

    float    vehicle_state_mpg;      // 0x214 (与 weapon_state.user 共用)
    float    weapon_state_angle_add; // 0x218
    float    weapon_state_reach;     // 0x21c
    float    weapon_state_off_x;     // 0x220
    float    weapon_state_off_y;     // 0x224

    // -------- 0x228-0x253: 动画 --------
    uint32_t anim_counter;           // 0x228
    uint32_t anim_weapon_counter;    // 0x22c
    uint32_t anim_swung_weaponid;    // 0x230
    uint32_t anim_hint;              // 0x234
    uint8_t  anim_weapontop;         // 0x238
    uint8_t  anim_flop;              // 0x239
    uint8_t  anim_state;             // 0x23a
    uint8_t  anim_substate;          // 0x23b
    uint8_t  anim_info;              // 0x23c
    uint8_t  anim_extra;             // 0x23d

    uint8_t  _pad_0x23e[6];          // 0x23e-0x243: 填充

    float    anim_floatheight;       // 0x244
    float    anim_floatval;          // 0x248
    float    anim_off_x;             // 0x24c
    float    anim_off_y;             // 0x250

    // -------- 0x254-0x287: 状态与标志 --------
    uint32_t hitpoints;              // 0x254 (角色: 流血计时器, 僵尸: 血量, 物品: 耐久, 投射物: 存活)
    uint16_t cooldown;               // 0x258
    uint16_t cooldown_set;           // 0x25a
    uint16_t action_cooldown;        // 0x25c
    uint16_t stun;                   // 0x25e
    uint32_t actioncmd;              // 0x260
    uint32_t actioncmd_old;          // 0x264

    uint8_t  _pad_0x268[0x10];       // 0x268-0x277: 填充

    uint8_t  fatigue;                // 0x278
    uint8_t  invincible_counter;     // 0x279 (受伤标志)
    uint8_t  no_hit;                 // 0x27a (无敌)
    uint8_t  no_do_damage;           // 0x27b
    uint8_t  lob_mode;               // 0x27c
    uint8_t  no_shoot;               // 0x27d
    uint8_t  pause;                  // 0x27e
    uint8_t  chatter;                // 0x27f

    // -------- 0x288-0x303: AI --------
    uint32_t ai_state;               // 0x288
    uint32_t ai_persist;             // 0x28c
    uint32_t ai_follow_flags;        // 0x290
    uint32_t ai_follow;              // 0x294
    uint32_t ai_move;                // 0x298
    uint32_t ai_action;              // 0x29c
    uint32_t ai_assess;              // 0x2a0

    uint8_t  _pad_0x2a4[4];          // 0x2a4-0x2a7: 填充

    uint32_t ai_wait;                // 0x2a8

    uint8_t  _pad_0x2ac[4];          // 0x2ac-0x2af: 填充

    uint32_t ai_countup;             // 0x2b0
    uint32_t ai_counter;             // 0x2b4
    uint32_t ai_weapon_wanted;       // 0x2b8
    uint32_t ai_threat_mode;         // 0x2bc
    uint32_t ai_threat_time;         // 0x2c0
    uint32_t ai_safety_time;         // 0x2c4

    uint8_t  _pad_0x2c8[4];          // 0x2c8-0x2cb: 填充

    uint32_t ai_wander_mode;         // 0x2cc

    uint8_t  _pad_0x2d0[4];          // 0x2d0-0x2d3: 填充

    uint16_t ai_followid;            // 0x2d4
    uint16_t ai_moveid;              // 0x2d6
    uint16_t ai_fleeid;              // 0x2d8
    uint16_t ai_actionid;            // 0x2da
    float    ai_followpos_x;         // 0x2dc
    float    ai_followpos_y;         // 0x2e0

    uint8_t  _pad_0x2e4[0x18];       // 0x2e4-0x2fb: 填充

    float    ai_threat_avg_dist;     // 0x2fc
    float    ai_score;               // 0x300
    // 大小到 0x304
};
// 角色 结构体
// 角色偏移0x5E25D8
// 大小: 0x2e0
// 最多: 0x100个角色
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
enum TeamStatus {
    TEAM_STATUS_NONE        = 0,
    TEAM_STATUS_FAMILIAR    = 1,
    TEAM_STATUS_RECRUITED   = 2,
    TEAM_STATUS_GOOD_LEFT   = 3,
    TEAM_STATUS_MISSING     = 4,
    TEAM_STATUS_BAD_LEFT    = 5,
    TEAM_STATUS_DIED        = 6,
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
struct character_data {
    
    uint8_t  _pad_0x00[4];           // 0x00-0x03

    // -------- 0x04-0x07: 当前 Thing ID --------
    uint16_t cur_thingid;            // 0x04 该角色对应的Thing ID
    
    uint8_t  _pad_0x06[2];           // 0x06-0x07

    // -------- 0x08-0x0b: 种子 --------
    uint32_t seed;                   // 0x08

    // -------- 0x0c-0x0f: 队伍状态 --------

    uint8_t  _pad_0x0c[3];           // 0x0c-0x0e

    uint8_t  team_status;            // 0x0f

    // -------- 0x10-0x1b: 位置与临时数据 --------
    uint8_t  location;               // 0x10

    uint8_t  _pad_0x11[3];           // 0x11-0x13

    uint32_t party;                  // 0x14
    uint32_t temp;                   // 0x18

    // -------- 0x1c: 名字职业特长 --------
    char name[40];                   // 0x1c
    char perk[40];                   // 0x44
    char trait[40];                  // 0x6c

    // -------- 0x94-0x97: 性别 --------
    uint16_t female;                 // 0x94

    uint8_t  _pad_0x96[2];           // 0x96-0x97

    // -------- 0x98-0xbb: 声音参数 --------
    float    voice_ex;               // 0x98
    float    voice_q;                // 0x9c
    float    voice_k;                // 0xa0
    float    voice_pitch;            // 0xa4
    float    voice_duty;             // 0xa8
    float    voice_flo;              // 0xac
    float    voice_fhi;              // 0xb0
    float    voice_vol;              // 0xb4

    uint8_t  _pad_0xb8[4];           // 0xb8-0xbb

    // -------- 0xbc-0xcf: 外观 --------
    uint16_t bodytype;               // 0xbc
    uint16_t headtype;               // 0xbe
    uint16_t torsotype;              // 0xc0
    uint16_t legstype;               // 0xc2
    uint16_t facetype;               // 0xc4
    uint16_t hairtype;               // 0xc6
    uint16_t hattype;                // 0xc8
    uint16_t glassestype;            // 0xca
    uint32_t skeleton_spriteid;      // 0xcc

    // -------- 0xd0-0xd7: 特殊模式 --------
    uint16_t specialmode;            // 0xd0
    uint16_t specialtype;            // 0xd2
    uint16_t specialhead;            // 0xd4
    uint16_t specialbody;            // 0xd6

    // -------- 0xd8-0xdb: 颜色 --------
    uint16_t skincolour;             // 0xd8
    uint16_t haircolour;             // 0xda

    // -------- 0xdc-0x10b: 颜色 (浮点) --------
    float    tint_skin_r;            // 0xdc
    float    tint_skin_g;            // 0xe0
    float    tint_skin_b;            // 0xe4
    float    tint_skin_a;            // 0xe8
    float    tint_hair_r;            // 0xec
    float    tint_hair_g;            // 0xf0
    float    tint_hair_b;            // 0xf4
    float    tint_hair_a;            // 0xf8
    float    tint_body_r;            // 0xfc
    float    tint_body_g;            // 0x100
    float    tint_body_b;            // 0x104
    float    tint_body_a;            // 0x108

    // -------- 0x10c-0x11b: 缩放 --------
    float    scale_head_x;           // 0x10c
    float    scale_head_y;           // 0x110
    float    scale_body_x;           // 0x114
    float    scale_body_y;           // 0x118

    // -------- 0x11c-0x13f: 偏移与浮动 --------
    float    headoff_x;              // 0x11c
    float    headoff_y;              // 0x120
    float    footoff_x;              // 0x124
    float    footoff_y;              // 0x128
    float    bounceval;              // 0x12c
    float    floatheight;            // 0x130
    float    floatval;               // 0x134
    uint16_t floattoggle;            // 0x138

    uint8_t  _pad_0x13a[2];          // 0x13a-0x13b

    float    breathescale;           // 0x13c

    // -------- 0x140-0x143: 生命值 --------
    uint32_t health;                 // 0x140

    // -------- 0x144-0x1bb: 描述文本 (120 字节，含终止符) --------
    char description[120];       // 0x144

    // -------- 0x1bc-0x1c8: 属性显示标志 (13 字节) --------
    union
    {
        int8_t display_stat[13]; // 0x1bc 属性是否已知
        CharacterStat displayStat;
    };

    // -------- 0x1c9-0x1e2: 属性基础值 (13 字节) --------
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

    // -------- 0x1e3-0x1ef: 属性加值 (13 字节) --------
    union
    {
        int8_t bonus_stats[13]; // 0x1e3 附加属性
        CharacterStat bonusStat;
    };

    // -------- 0x1f0-0x1f3: 速度加成 --------
    float    speed_bonus;            // 0x1f0

    uint8_t  _pad_0x1f4[4];          // 0x1f4-0x1f7

    // -------- 0x1f8-0x203: 修改标志 --------
    uint8_t status0;              // 0x1f8 状态 00000000,依次为SICK, INJURED, TIRED, SUPERDOG, DOGMALUS, COFFEE, DOGPAL, EXPLORER

    uint8_t _pad_0x1f9[1];        // 0x1f9

    uint8_t status2;              // 0x1fa 状态 大于等于127则为PETMALUS
    uint8_t status3;              // 0x1fb 状态 00000000,依次为PETMALUS_CAT, HAGGLER, PATHFINDER, SOUNDSLEEP,FIREPROOF, ZEALOUS, GWM 
    uint32_t mod_flags2;             // 0x1fc
    uint32_t use_filter;             // 0x200

    // -------- 0x208-0x27f: AI 参数 (0x208-0x20f 字节, 0x210+ dword) --------
    
    uint8_t  _pad_0x204[5];          // 0x204-0x208

    uint8_t  ai_pickup_gun_max;      // 0x209
    uint8_t  ai_pickup_melee_max;    // 0x20a
    uint8_t  ai_attack_level;        // 0x20b
    uint8_t  ai_attack_mode;         // 0x20c
    uint8_t  ai_prefer_weapon_slot;  // 0x20d

    uint8_t  _pad_0x20e[2];          // 0x20e-0x20f

    uint32_t ai_react_min;           // 0x210
    uint32_t ai_react_max;           // 0x214
    uint32_t ai_assess_min;          // 0x218
    uint32_t ai_assess_max;          // 0x21c

    uint8_t  _pad_0x220[8];          // 0x220-0x227

    float    ai_rush_chance;         // 0x228
    float    ai_attack_chance;       // 0x22c
    float    ai_wander_chance;       // 0x230
    float    ai_loot_chance;         // 0x234
    float    ai_loot_dist;           // 0x238
    float    ai_follow_leash;        // 0x23c
    float    ai_ranged_leash;        // 0x240
    float    ai_safety_leash;        // 0x244
    float    ai_safety_threshold;    // 0x248
    float    ai_flock_dist;          // 0x24c
    float    ai_flee_dist;           // 0x250
    float    ai_attack_dist;         // 0x254
    float    ai_shoot_dist;          // 0x258
    float    ai_shoot_obstacle_scan_step_size; // 0x25c
    float    ai_shoot_obstacle_mass_min;       // 0x260
    float    ai_threat_radius;       // 0x264
    float    ai_threat_count_base;   // 0x268
    float    ai_threat_dist_base;    // 0x26c
    float    ai_threat_threshold;    // 0x270
    float    ai_threat_respond_chance; // 0x274
    float    ai_threat_relax_chance; // 0x278
    uint32_t user_ival;              // 0x27c
    uint32_t user_special_counter;   // 0x280
    float    user_fval;              // 0x284

    // -------- 0x288-0x2a7: 资源  --------
    union
    {
        int32_t resource[7]; // 0x288 资源
        CommonResource Resource;
    };

    // -------- 0x2a8-0x2b3: 默认武器 --------
    uint32_t weapon_default;         // 0x2a8

    uint8_t  _pad_0x2ac[4];          // 0x2ac-0x2af

    // -------- 0x2b4-0x2d3: 武器槽 --------
    struct
    {
        union
        {
            int32_t weapon[3];
            struct
            {
                int32_t Stack; //数量
                int32_t ID; //武器ID
                int32_t Lock; //是否可丢
            } Weapon;
        };
    } Weapon_slots[3];      // 0x2b0

    // -------- 0x2dc-0x2df: 主事件 --------
    uint16_t main_events;            // 0x2dc

    uint8_t  _pad_0x2de[2];          // 0x2de-0x2df    // 大小到 0x2e0
};
// 武器 结构体
// 大小: 0x1c4
// 武器池偏移 0x004E0080;
// 最多0x401个武器
struct weapon_data {
    // -------- 0x00-0x28: 武器名称 (字符数组) --------
    char     name[40];              // 0x00: 名字

    uint8_t  _pad_0x28[0x34];       // 0x28-0x5b: 填充

    // -------- 0x5c-0x5f: 精灵 ID (用于 thing 的 spriteid) --------
    uint32_t sprite_id;               // 0x5c: 武器精灵 ID (实际是 word，但对齐为 dword)

    // -------- 0x60-0x63: Swoosh ID 1 --------
    int32_t  swoosh1id;               // 0x60

    // -------- 0x64-0x67: Swoosh ID 2 --------
    int32_t  swoosh2id;               // 0x64

    // -------- 0x68-0x77: Swoosh 物理 --------
    float    swoosh_range;            // 0x68
    float    swoosh_dist;             // 0x6c
    float    thrust_range;            // 0x70
    float    swoosh_vel;              // 0x74

    // -------- 0x78-0x7f: 近战属性 --------
    float    leaning;                 // 0x78
    uint8_t  default_only;            // 0x7c
    uint8_t  no_held_sprite;          // 0x7d
    uint8_t  skin_colour;             // 0x7e
    uint8_t  no_zombie_break;         // 0x7f

    // -------- 0x80-0x87: 状态标志 --------
    uint8_t  no_discard;              // 0x80
    uint8_t  unbreakable;             // 0x81

    uint8_t  _pad_0x82[2];            // 0x82

    float    cooldown;                // 0x84

    // -------- 0x88-0x8f: 长度 --------
    float    length;                  // 0x88

    uint8_t  _pad_0x8c[4];            // 0x8c-0x8f

    // -------- 0x90-0xaf: 近战角度与范围 --------
    float    melee_start_angle;       // 0x90
    float    melee_windup_angle;      // 0x94
    float    melee_end_angle;         // 0x98
    float    melee_start_reach;       // 0x9c
    float    melee_windup_reach;      // 0xa0
    float    melee_end_reach;         // 0xa4
    float    melee_reach;             // 0xa8
    float    melee_retract;           // 0xac
    float    melee_range;             // 0xb0
    float    melee_weapon_angle;      // 0xb4
    float    melee_extra_chance;      // 0xb8
    float    melee_thrown_speed;      // 0xbc
    float    melee_thrown_lob;        // 0xc0
    float    melee_range_hint;        // 0xc4
    float    melee_range_min_hint;    // 0xc8
    uint8_t  melee_range_cooldown_hint; // 0xcc

    uint8_t  _pad_0xcd[3];            // 0xcd-0xcf

    // -------- 0xd0-0xe3: 近战疲劳与得分 --------
    float    melee_fatigue_scale;     // 0xd0
    float    melee_fatigue_cooldown_scale; // 0xd4
    float    melee_fatigue_power_scale;    // 0xd8
    float    melee_fatigue_knockback_scale; // 0xdc
    float    melee_score_scale;       // 0xe0
    float    melee_score_add;         // 0xe4
    float    melee_pickup_scale;      // 0xe8
    float    melee_pickup_add;        // 0xec
    float    ranged_score_add;        // 0xf0

    // -------- 0xf4-0xff: 处理器 --------
    uint32_t melee_thrown_handler;    // 0xf4
    uint32_t wielder_handler;         // 0xf8
    uint8_t  melee_extra_hits;        // 0xfc

    uint8_t  _pad_0xfd[3];            // 0xfd-0xff

    // -------- 0x100-0x113: 近战偏移 --------
    float    melee_off_x;             // 0x100
    float    melee_off_y;             // 0x104
    float    melee_shift_x;           // 0x108
    float    melee_shift_y;           // 0x10c
    float    gun_angle;               // 0x110

    // -------- 0x114-0x11b: 穿透 --------

    uint8_t  _pad_0x114[4];           // 0x114-0x117

    float    shoot_thru;              // 0x118

    // -------- 0x11c-0x12b: 威力 --------
    float    power;                   // 0x11c
    float    special_power;           // 0x120
    float    knockback;               // 0x124
    float    special_knockback;       // 0x128
    float    shot_knockback;          // 0x12c

    // -------- 0x130-0x13f: 近战技能 --------
    uint8_t  melee_skill;             // 0x130
    uint8_t  melee_thrown;            // 0x131

    uint8_t  _pad_0x132[2];           // 0x132-0x133

    float    melee_break_scale;       // 0x134
    float    melee_jam_scale;         // 0x138
    float    thrown_lift;             // 0x13c

    // -------- 0x140-0x14b: 弹药/燃料 --------
    uint8_t  ammo_type;               // 0x140
    uint8_t  fuel_type;               // 0x141
    uint8_t  melee_aiming;            // 0x142
    uint8_t  melee_aiming_offset;     // 0x143
    float    burn_idle;               // 0x144
    float    burn_active;             // 0x148

    // -------- 0x14c-0x15f: 弹药/堆叠 --------
    int32_t  dropped_hp;              // 0x14c
    int32_t  ammo_max;                // 0x150
    int32_t  fuel_max;                // 0x154
    int32_t  stack_max;               // 0x158

    uint8_t  _pad_0x15c[4];           // 0x15c-0x15f

    uint8_t  stack_as_charges;        // 0x160
    uint8_t  stack_no_show;           // 0x161
    uint8_t  no_ai_use;               // 0x162

    uint8_t  _pad_0x163[1];           // 0x163

    // -------- 0x163-0x16f: 武器属性 --------
    uint8_t  flammability;            // 0x163
    uint8_t  spreadability;           // 0x164
    uint8_t  cock_sound;              // 0x165
    uint8_t  gun_skill;               // 0x166
    uint8_t  targeting;               // 0x167
    uint8_t  projectiles;             // 0x168
    uint8_t  revolver;                // 0x169
    uint8_t  shell_count;             // 0x16a
    uint8_t  auto_eject;              // 0x16b
    float    lock_drift;              // 0x16c
    float    shot_power;              // 0x170
    float    reload;                  // 0x174
    float    gun_muzzle_scale;        // 0x178
    int32_t  gun_muzzle_height;       // 0x17c

    // -------- 0x180-0x18b: 激光与燃烧 --------
    uint8_t  _pad_0x180[8];           // 0x180-0x187

    int32_t  laser_type;              // 0x188
    float    burn_scale;              // 0x18c

    // -------- 0x190-0x1af: 射程与瞄准 --------
    float    range_guess;             // 0x190
    float    range_cone;              // 0x194
    float    range_aim_scale;         // 0x198

    uint8_t  _pad_0x19c[4];           // 0x19c-0x19f

    float    skill_angle_range;       // 0x1a0
    float    skill_shoot_thru_scale;  // 0x1a4
    int32_t  point_blank_dist;        // 0x1a8
    float    boom_factor;             // 0x1ac
    float    spread_factor;           // 0x1b0
    float    custom_retract;          // 0x1b4

    uint8_t  _pad_0x1b8[0xc];         // 0x1b8-0x1c3
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
    struct {
        union
        {
            int32_t weapon[2];
            struct
            {
                int32_t ID; // 武器 ID (0=空)
                int32_t Stack; // 数量
            } Weapon;
        };
    } Storage_slots[15];                // 0x48
};