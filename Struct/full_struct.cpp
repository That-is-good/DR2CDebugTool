// ============================================================
// 常量定义
// ============================================================

typedef char sbyte;
typedef unsigned char byte;
typedef unsigned short ushort;
typedef unsigned int uint;

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

struct Position3D
{
    float x;
    float y;
    float z;
};

union Vector3D
{
    struct Position3D Vec;
    float vec[3];
};

struct Position2D
{
    float x;
    float y;
};

union Vector2D
{
    struct Position2D Vec;
    float vec[2];
};

struct ColorRGBA
{
    float r;
    float g;
    float b;
    float a;
};

union RGBA
{
    struct ColorRGBA Color;
    float color[4];
};

struct CharacterStat
{
    sbyte morale;
    sbyte attitude;
    sbyte composure;
    sbyte charm;
    sbyte wits;
    sbyte loyalty;
    sbyte medical;
    sbyte mechanical;
    sbyte shooting;
    sbyte strength;
    sbyte dexterity;
    sbyte fitness;
    sbyte vitality;
};

union Stats
{
    sbyte stat[13];
    CharacterStat Stat;
};

struct CommonResource
{
    int none;
    int food;
    int gas;
    int medical;
    int bullet;
    int rifle;
    int shell;
    int junk;
};

union Resources
{
    int resource[8];
    CommonResource Resource;
};

struct WeaponSlot3D
{
    int Stack;  //数量
    int ID;     //武器ID
    int Lock;   //是否可丢
};

union WeaponSlots3D
{
    int weapon[3];
    struct WeaponSlot3D Weapon;
};

struct WeaponSlot2D
{
    int Stack;  //数量
    int ID;     //武器ID
};

union WeaponSlots2D
{
    int weapon[2];
    struct WeaponSlot2D Weapon;
};

// 实体 结构体
// 实体池偏移0x5632E0
// 大小: 0x304
// 最多: 0x262个实体
struct thing {
    ushort id;                  // 0x00 Thing ID (未注册)
    byte type;                  // 0x02 类型
    byte subtype;               // 0x03 子类型
    byte mapid;                 // 0x04 区域ID
    byte old_mapid;             // 0x05 上一个区域ID，如果没有则为FF
    ushort source_thing_id;     // 0x06 产生该thing的thing ID + 1
    byte facing;                // 0x08 朝向
    byte walkover;              // 0x09 可走过
    byte low_profile;           // 0x0a 低轮廓
    byte layer;                 // 0x0b 图层
    byte tileflags;             // 0x0c 图块标志
    byte nocollide;             // 0x0d 无碰撞
    byte bolted;                // 0x0e 固定
    byte boltstr;               // 0x0f 固定强度
    byte nopush;                // 0x10 不可推动
    byte nopick;                // 0x11 不可拾取
    byte unseen;                // 0x12 不渲染
    byte invisible;             // 0x13 不绘制
    byte noshadow;              // 0x14 无阴影
    byte nodust;                // 0x15 无灰尘
    byte _pad_0x15;             // 0x16 填充
    float wind;                 // 0x18 风力影响
    int flies;                  // 0x1c 苍蝇数
    Vector3D old_pos;           // 0x20 旧位置
    Vector3D pos;               // 0x2c 位置
    Vector3D vel;               // 0x38 速度
    float angle;                // 0x44 角度
    float rot;                  // 0x48 旋转
    float gravity;              // 0x4c 重力
    ushort overlap_id;          // 0x50 重叠ID
    ushort interact_id;         // 0x52 交互ID
    ushort inside_id;           // 0x54 内部ID
    ushort just_hit_id;         // 0x56 刚击中的ID
    float mass;                 // 0x58 质量
    float friction;             // 0x5c 阻力
    float bounce_friction;      // 0x60 弹跳阻力
    sbyte fade;                 // 0x64 淡出计时器
    byte collision_flag;        // 0x65 碰撞状态
        byte _pad_0x66[2];          // 0x66 填充
    int burning;                // 0x68 燃烧状态
    byte flammability;          // 0x6c 可燃性
    byte spreadability;         // 0x6d 蔓延性
    byte reburn;                // 0x6e 再燃烧
    byte no_lighting;           // 0x6f 无光照
    int glow;                   // 0x70 是否发光
    uint update_counter;        // 0x74 活动计时器 (递增)
    uint activity_timer;        // 0x78 活动计时器 (递减)
    ushort thingseed;           // 0x7c 种子
    short timer;                // 0x7e 递减计时器, 到 0 时触发脚本事件
    ushort timer_hint;          // 0x80 计时器提示
    ushort timer_base;          // 0x82 计时器基数
    float timer_float;          // 0x84 计时器浮点值
        byte  _pad_0x88[4];         // 0x88 填充
    void* handler;              // 0x8c 处理函数指针
    int leaveok;                // 0x90 允许离开当前区域
    void* cyoa;                 // 0x94 “Choose Your Own Adventure”脚本指针
    void* events;               // 0x98 事件脚本指针
    void* action;               // 0x9c 动作脚本指针
    uint user_word;             // 0xa0 用户自定义词
    uint action_icon;           // 0xa4 动作图标
    uint inaction_icon;         // 0xa8 非动作图标
    Vector3D rad;               // 0xac 半径
    Vector2D wallbox;           // 0xb8 碰撞箱
    RGBA clr;                   // 0xc0 颜色 RGBA
    Vector2D scale;             // 0xd0 缩放
    ushort spriteid;            // 0xd8 精灵ID
    ushort uncentered;          // 0xda 未居中
    ushort itemtype;            // 0xdc 物品类型
    byte openable;              // 0xde 可打开
    byte item_opened;           // 0xdf 已打开
    int item_locked;           // 0xe0 已锁定
    int amount;                 // 0xe4 数量
    byte loot;                  // 0xe8 战利品 数值对应Resources
    byte item_status;           // 0xe9 物品状态
        byte _pad_0xea[2];          // 0xea 填充
    uint item_hint;             // 0xec
    uint item_charges;          // 0xf0 物品充能
        byte _pad_0xf4[4];          // 0xf4 填充
    float hitcheck_mode;        // 0xf8 命中检测模式
    uint hitcheck_dist;         // 0xfc 命中检测距离
    uint hitcheck_hits_allowed; // 0x100 允许命中次数
    uint hitcheck_closest_dist; // 0x104 最近距离
    uint hitcheck_blast;        // 0x108 爆炸
    ushort hitcheck_flag;       // 0x10c 击中标志

    ushort hitcheck_source_id;  // 0x10e 来源ID
    ushort hitcheck_lasthit_id; // 0x110 最后命中ID
    ushort hitcheck_closest_id; // 0x112 最近ID
    ushort hitcheck_pad_0x11a;  // 0x114 填充
    float hitcheck_rad;         // 0x118 半径
    float hitcheck_power;       // 0x11c 威力
    float hitcheck_power_max;   // 0x120 最大威力
    float hitcheck_knockback;   // 0x124 击退
    float hitcheck_shoot_thru;  // 0x128 穿透
    Vector2D hitcheck_dir;      // 0x12c 方向
    Vector2D hitcheck_pos;      // 0x134 位置
    Vector2D _pad_0x140;        // 0x13c 填充
    uint charid;                // 0x148 角色 ID
    uint zombietype;            // 0x14c 僵尸类型
    ushort shooterid;           // 0x150 射击者 ID
    byte legframe;              // 0x152 腿部动画
    byte turnframe;             // 0x153 转动动画
    byte faceframe;             // 0x154 脸部动画
    byte aiming_mode;           // 0x155 瞄准模式
    byte _pad_0x156;            // 0x156 填充
    byte _pad_0x157;            // 0x157 填充
    float move_angle;           // 0x158 移动角度
    float swing_angle;          // 0x15c 挥动角度
    float angle_pad_0x160;      // 0x160 填充
    float aim_angle;            // 0x164 瞄准角度
    float lock_angle;           // 0x168 锁定角度
    Vector2D move_dir;          // 0x16c 移动方向
        byte _pad_0x174[0x18];      // 0x174 填充
    uint offscreen_counter;     // 0x18c 偏移屏幕计数器
    sbyte shudder;              // 0x190 抖动强度
    Vector2D shudderpuff;       // 0x191 抖动偏移
        byte _pad_0x19c[6];         // 0x199 填充
    byte weapon_switch;         // 0x19f 武器切换
    byte weapon_slot_using;     // 0x1a0 当前使用的武器槽
    byte weapon_swap_select;    // 0x1a1 武器交换选择
    byte _pad_0x1a2[3];         // 0x1a2
    byte weapon_no_hit_human;   // 0x1a5 不对人类造成伤害
    ushort nearest_enemy_id;    // 0x1a6 最近敌人ID
    uint nearest_enemy_dist2;   // 0x1a8 最近敌人距离平方
        byte _pad_0x1ac[0x20];      // 0x1ac 填充
        
    uint nearest_interact_id; // 0x1cc 最近交互ID
    uint nearest_interact_dist2;// 0x1d0 最近交互距离平方
    ushort nearest_pickup_id;   // 0x1d4 最近拾取ID
    ushort _pad_0x1d6;          // 0x1d6 填充
    float threat_level;         // 0x1d8 威胁等级
    float threat_dist2;         // 0x1dc 威胁距离平方
    uint threat_count;          // 0x1e0 威胁计数
    Vector2D threat_pos;        // 0x1e4 威胁位置
    ushort targetid;            // 0x1ec 目标ID
        byte _pad_0x1ee[2];         // 0x1ee 填充
    Vector2D targetpos;         // 0x1f0 目标位置
    Vector2D destpos;           // 0x1f8 目的位置
    ushort carryid;             // 0x200 携带的ID
    ushort carrierid;           // 0x202 携带者的ID
    ushort throwerid;           // 0x204 投掷者的ID
        byte _pad_0x206[2];         // 0x206 填充
    byte chassis;               // 0x208 底盘
    byte chassis_max;           // 0x209 最大底盘
    byte engine;                // 0x20a 引擎
    byte engine_max;            // 0x20b 最大底盘
    byte armour;                // 0x20c 装甲
    byte armour_max;            // 0x20d 最大装甲
    byte carspeed;              // 0x20e 速度
    byte carspeed_max;          // 0x20f 最大速度
    int repair;                // 0x210 修理
    float mpg;                  // 0x214 油耗
    float weapon_state_angle_add;// 0x218 武器状态角度增加
    float weapon_state_reach;   // 0x21c 武器状态范围
    Vector2D weapon_state_off;  // 0x220 武器状态偏移
    uint anim_counter;          // 0x228 动画计数器
    uint anim_weapon_counter;   // 0x22c 武器动画计数器
    uint anim_swung_weaponid;   // 0x230 挥动的武器ID
    uint anim_hint;             // 0x234 动画提示
    byte anim_weapontop;        // 0x238 武器在顶部
    byte anim_flop;             // 0x239 动画翻转
    byte anim_state;            // 0x23a 动画状态
    byte anim_substate;         // 0x23b 动画子状态
    byte anim_info;             // 0x23c 动画信息
    byte anim_extra;            // 0x23d 动画额外
        byte _pad_0x23e[6];         // 0x23e 填充
    float anim_floatheight;     // 0x244 动画浮空高度
    float anim_floatval;        // 0x248 动画浮空值
    Vector2D anim_off;          // 0x24c 动画偏移
    int hitpoints;             // 0x254 (角色: 流血计时器, 僵尸: 血量, 物品: 耐久, 投射物: 存活)
    ushort cooldown;            // 0x258 冷却
    ushort cooldown_set;        // 0x25a 冷却设置
    ushort action_cooldown;     // 0x25c 动作冷却
    ushort stun;                // 0x25e 眩晕
    uint actioncmd;             // 0x260 动作命令
    uint actioncmd_old;         // 0x264 旧动作命令
        byte _pad_0x268[0x10];      // 0x268-0x277: 填充
    byte fatigue;               // 0x278 疲劳
    sbyte invincible_counter;   // 0x279 无敌计数器
    byte no_hit;                // 0x27a 不可被击中
    byte no_do_damage;          // 0x27b 不造成伤害
    byte lob_mode;              // 0x27c 投掷模式
    byte no_shoot;              // 0x27d 不可射击
    sbyte pause;                // 0x27e 暂停
    sbyte chatter;              // 0x27f 对话
        byte _pad_0x280[8];
    uint ai_state;              // 0x288 AI状态
    uint ai_persist;            // 0x28c AI持续
    uint ai_follow_flags;       // 0x290 AI跟随标志
    uint ai_follow;             // 0x294 AI跟随
    uint ai_move;               // 0x298 AI移动
    uint ai_action;             // 0x29c AI动作
    uint ai_assess;             // 0x2a0 AI评估
    uint _pad_0x2a4;            // 0x2a4 填充
    uint ai_wait;               // 0x2a8 AI等待
    uint _pad_0x2ac;            // 0x2ac 填充
    uint ai_countup;            // 0x2b0 AI计数
    uint ai_counter;            // 0x2b4 AI计数器
    uint ai_weapon_wanted;      // 0x2b8 AI想要的武器
    uint ai_threat_mode;        // 0x2bc AI威胁模式
    uint ai_threat_time;        // 0x2c0 AI威胁时间
    uint ai_safety_time;        // 0x2c4 AI安全时间
    uint _pad_0x2c8;            // 0x2c8 填充
    uint ai_wander_mode;        // 0x2cc AI游荡模式
    uint _pad_0x2d0;            // 0x2d0 填充
    ushort ai_followid;         // 0x2d4 AI跟随ID
    ushort ai_moveid;           // 0x2d6 AI移动ID
    ushort ai_fleeid;           // 0x2d8 AI逃跑ID
    ushort ai_actionid;         // 0x2da AI动作ID
    Vector2D ai_followpos;      // 0x2dc AI跟随位置

    byte _pad_0x2e4[8];         // 0x2E4 填充
    Vector2D collision_vel;     // 0x2ec 临时速度缓冲
    byte _pad_0x2f4[8];         // 0x2F4 填充

    float ai_threat_avg_dist;   // 0x2fc AI平均威胁距离
    float ai_score;             // 0x300 AI分数
};

// 角色 结构体
// 角色偏移0x5E25D8
// 大小: 0x2e0
// 最多: 0x100个角色
struct character {
    uint id;                    // 角色ID (最多256)
    uint cur_thingid;           // 0x04 该角色对应的Thing ID
    uint seed;                  // 0x08 种子
    byte _pad_0x0c[3];          // 0x0c
    byte team_status;            // 0x0f 队伍状态
    uint location;              // 0x10 位置
    uint party;                 // 0x14 队伍
    uint temp;                  // 0x18 临时
    char name[40];              // 0x1c 角色名
    char perk[40];              // 0x44 特质
    char trait[40];             // 0x6c 特长
    ushort female;              // 0x94 女性标志
    ushort pet;                 // 0x96 宠物
    float voice_ex;             // 0x98 语音额外
    float voice_q;              // 0x9c 语音Q
    float voice_k;              // 0xa0 语音K
    float voice_pitch;          // 0xa4 语音音调
    float voice_duty;           // 0xa8 语音占空比
    float voice_flo;            // 0xac 语音低频
    float voice_fhi;            // 0xb0 语音高频
    float voice_vol;            // 0xb4 语音音量
        byte _pad_0xb8[4];          // 0xb8-0xbb
    ushort bodytype;            // 0xbc 身体类型
    ushort headtype;            // 0xbe 头部类型
    ushort torsotype;           // 0xc0 躯干类型
    ushort legstype;            // 0xc2 腿部类型
    ushort facetype;            // 0xc4 面部类型
    ushort hairtype;            // 0xc6 发型
    ushort hattype;             // 0xc8 帽子类型
    ushort glassestype;         // 0xca 眼镜类型
    uint skeleton_spriteid;     // 0xcc 骷髅精灵ID
    ushort specialmode;         // 0xd0
    ushort specialtype;         // 0xd2
    ushort specialhead;         // 0xd4
    ushort specialbody;         // 0xd6
    ushort skincolour;          // 0xd8
    ushort haircolour;          // 0xda
    RGBA tint_skin;             // 0xdc 肤色色调
    RGBA tint_hair;             // 0xec 发色色调
    RGBA tint_body;             // 0xfc 身体色调
    Vector2D scale_head;        // 0x10c 头部缩放
    Vector2D scale_body;        // 0x114 身体缩放
    Vector2D headoff;           // 0x11c 头部偏移
    Vector2D footoff;           // 0x124 脚步偏移
    float bounceval;            // 0x12c 弹跳值
    float floatheight;          // 0x130 浮空高度
    float floatval;             // 0x134 浮空值
    ushort floattoggle;         // 0x138 浮空切换
        byte  _pad_0x13a[2];        // 0x13a
    float breathescale;         // 0x13c 呼吸缩放
    uint health;                // 0x140
    char description[120];      // 0x144
    Stats displayStat;          // 0x1bc 属性是否已知
    Stats baseStat;             // 0x1c9 基础属性
    Stats tempStat;             // 0x1d6 临时属性
    Stats bonusStat;            // 0x1e3 附加属性
    float speed_bonus;          // 0x1f0 额外速度
        byte _pad_0x1f4[4];         // 0x1f4
    uint mod_flags1;            // 0x1f8 状态标志
    uint mod_flags2;            // 0x1fc 状态标志2
    uint use_filter;            // 0x200 使用过滤器
        byte _pad_0x204[5];         // 0x204
    byte ai_pickup_gun_max;     // 0x209
    byte ai_pickup_melee_max;   // 0x20a
    byte ai_attack_level;       // 0x20b AI攻击等级
    byte ai_attack_mode;        // 0x20c AI攻击模式
    byte ai_prefer_weapon_slot; // 0x20d AI首选武器槽
        byte _pad_0x20e[2];         // 0x20e
    uint ai_react_min;          // 0x210 AI反应最小
    uint ai_react_max;          // 0x214 AI反应最大
    uint ai_assess_min;         // 0x218 AI评估最小
    uint ai_assess_max;         // 0x21c AI评估最大
        byte _pad_0x220[8];         // 0x220
    float ai_rush_chance;       // 0x228
    float ai_attack_chance;     // 0x22c
    float ai_wander_chance;     // 0x230
    float ai_loot_chance;       // 0x234
    float ai_loot_dist;         // 0x238
    float ai_follow_leash;      // 0x23c
    float ai_ranged_leash;      // 0x240
    float ai_safety_leash;      // 0x244
    float ai_safety_threshold;  // 0x248
    float ai_flock_dist;        // 0x24c
    float ai_flee_dist;         // 0x250
    float ai_attack_dist;       // 0x254
    float ai_shoot_dist;        // 0x258
    float ai_shoot_obstacle_scan_step_size; // 0x25c
    float ai_shoot_obstacle_mass_min;   // 0x260
    float ai_threat_radius;     // 0x264
    float ai_threat_count_base; // 0x268
    float ai_threat_dist_base;  // 0x26c
    float ai_threat_threshold;  // 0x270
    float ai_threat_respond_chance; // 0x274
    float ai_threat_relax_chance;   // 0x278
    uint user_ival;             // 0x27c 用户整数值
    uint user_special_counter;  // 0x280 用户特殊计数器
    float user_fval;            // 0x284 用户浮点值
    Resources localResource;    // 0x288 资源
    WeaponSlots2D weapon_default;   // 0x2a8
    WeaponSlots3D weapon_slots[3];  // 0x2b0 武器槽
        byte _pad_0x2d4[4];     // 0x2d4
    uint main_events;           // 0x2dc 主事件指针
};

// 武器 结构体
// 大小: 0x1c4
// 武器池偏移 0x004E0080;
// 最多0x401个武器
struct weapon {
    // -------- 0x00-0x28: 武器名称 (字符数组) --------
    char name[40];              // 0x00 名字
        byte _pad_0x28[0x34];       // 0x28
    uint sprite_id;             // 0x5c 武器精灵 ID
    int swoosh1id;              // 0x60
    int swoosh2id;              // 0x64
    float swoosh_range;         // 0x68
    float swoosh_dist;          // 0x6c
    float thrust_range;         // 0x70
    float swoosh_vel;           // 0x74
    float leaning;              // 0x78
    byte default_only;          // 0x7c
    byte no_held_sprite;        // 0x7d
    byte skin_colour;           // 0x7e
    byte no_zombie_break;       // 0x7f
    byte no_discard;            // 0x80
    byte unbreakable;           // 0x81
    byte flags6;                // 0x82
    byte flags7;                // 0x83
    float cooldown;             // 0x84
    float length;               // 0x88
    float _pad_0x8c;            // 0x8c
    float melee_start_angle;    // 0x90
    float melee_windup_angle;   // 0x94
    float melee_end_angle;      // 0x98
    float melee_start_reach;    // 0x9c
    float melee_windup_reach;   // 0xa0
    float melee_end_reach;      // 0xa4
    float melee_reach;          // 0xa8
    float melee_retract;        // 0xac
    float melee_range;          // 0xb0
    float melee_weapon_angle;   // 0xb4
    float melee_extra_chance;   // 0xb8
    float melee_thrown_speed;   // 0xbc
    float melee_thrown_lob;     // 0xc0
    float melee_range_hint;     // 0xc4
    float melee_range_min_hint; // 0xc8
    int melee_range_cooldown_hint; // 0xcc
    float melee_fatigue_scale;  // 0xd0
    float melee_fatigue_cooldown_scale; // 0xd4
    float melee_fatigue_power_scale;    // 0xd8
    float melee_fatigue_knockback_scale;    // 0xdc
    float melee_score_scale;    // 0xe0
    float melee_score_add;      // 0xe4
    float melee_pickup_scale;   // 0xe8
    float melee_pickup_add;     // 0xec
    float ranged_score_add;     // 0xf0
    void* melee_thrown_handler; // 0xf4
    void* wielder_handler;      // 0xf8
    int melee_extra_hits;       // 0xfc
    Vector2D melee_off;         // 0x100 近战偏移
    Vector2D melee_shift;       // 0x108 近战移动
    float gun_angle;            // 0x110
    float _pad_0x114;           // 0x114
    float shoot_thru;           // 0x118
    float power;                // 0x11c
    float special_power;        // 0x120
    float knockback;            // 0x124
    float special_knockback;    // 0x128
    float shot_knockback;       // 0x12c
    byte melee_skill;           // 0x130
    byte melee_thrown;          // 0x131
        byte _pad_0x132[2];         // 0x132
    float melee_break_scale;    // 0x134
    float melee_jam_scale;      // 0x138
    float thrown_lift;          // 0x13c
    byte ammo_type;             // 0x140
    byte fuel_type;             // 0x141
    byte melee_aiming;          // 0x142
    byte melee_aiming_offset;   // 0x143
    float burn_idle;            // 0x144
    float burn_active;          // 0x148
    int dropped_hp;             // 0x14c
    int ammo_max;               // 0x150
    int fuel_max;               // 0x154
    int stack_max;              // 0x158
        byte _pad_0x15c[4];         // 0x15c
    byte stack_as_charges;      // 0x160
    byte stack_no_show;         // 0x161
    byte no_ai_use;             // 0x162
    byte flammability;          // 0x163
    byte spreadability;         // 0x164
    byte cock_sound;            // 0x165
    byte gun_skill;             // 0x166
    byte targeting;             // 0x167
    byte projectiles;           // 0x168
    byte revolver;              // 0x169
    byte shell_count;           // 0x16a
    byte auto_eject;            // 0x16b
    float lock_drift;           // 0x16c
    float shot_power;           // 0x170
    float reload;               // 0x174
    float gun_muzzle_scale;     // 0x178
    int gun_muzzle_height;      // 0x17c
        byte _pad_0x180[8];         // 0x180
    int laser_type;             // 0x188
    float burn_scale;           // 0x18c
    float range_guess;          // 0x190
    float range_cone;           // 0x194
    float range_aim_scale;      // 0x198
    float _pad_0x19c;           // 0x19c
    float skill_angle_range;    // 0x1a0
    float skill_shoot_thru_scale;   // 0x1a4
    int point_blank_dist;       // 0x1a8
    float boom_factor;          // 0x1ac
    float spread_factor;        // 0x1b0
    float custom_retract;       // 0x1b4
        byte _pad_0x1b8[0xc];       // 0x1b8
};

// MISSION STATE STRUCTURE (本局游戏状态)
// 起始: 0x5E2238
// 大小: 因为没有数组，所以无所谓
struct mission_state {
    uint player_char[4];        // 0x18 当前队伍角色在角色中的顺序, 从1开始
    Resources storageResource;  // 0x28 资源
    WeaponSlots2D Storage_slots[15];    // 0x48 仓库武器槽
};

// ============================================================
// THING 全局变量
// ============================================================

int dword_7CC314;   // 僵尸实体计数
int dword_7CC318;   // 总存活实体计数
int dword_7CCD24;   // 上次分配的ID+1
int dword_86DBA4;   // 当前地图ID

// ============================================================
// THING 分配与销毁函数
// ============================================================

// -------- 核心分配函数 --------
thing* AllocateEntity(byte type);   // @0x452710 核心分配
int GetCurrentMapId()   // @0x47D310 获取当前地图ID
{
  return dword_86DBA4;
}

thing* Allocatehumanthing();                // @0x45c480 分配人类实体
thing* CreateRandomEntity();                // @0x4c35d0 分配僵尸实体
thing* AllocateThing(byte subtype);         // @0x46deb0 分配物品实体
thing* CreateWeaponEntity(uint weapon_id);  // @0x4beb30 分配武器实体

// -------- 人类 --------
int Assigncharactertothing(thing* t, uint charSlot); // @0x45c440 将thing绑定到角色槽
uint AllocateCharacterSlot();               // @0x429a10 分配角色槽 (返回角色ID)
void RecruitCharacter(uint charId, byte location); // @0x4492f0 招募角色

// -------- 销毁 --------
void FreeThing(thing* t);                   // @0x452aa0 释放实体
void ThingDropCarry(thing* t);              // @0x459f60 释放被携带的实体
character* GetCharacterData(thing* t);           // @0x45c500 获取实体对应的角色数据
// ============================================================
// 生成 Thing 的伪代码（使用内置函数）
// ============================================================

// ---------- 1. 生成 Human ----------
thing* CreateHuman(Vector3D lpos) {
    // 1. 分配实体槽
    thing* t = Allocatehumanthing();  // 内部调用 AllocateEntity(1)
    if (!t) return nullptr;
    
    // 2. 分配角色槽并绑定
    uint charSlot = AllocateCharacterSlot();
    if (!charSlot) {
        FreeThing(t);
        return nullptr;
    }
    Assigncharactertothing(t, charSlot);
    
    // 3. 设置出生位置
    t->pos = lpos;
    // 5. 加入队伍 (可选)
    RecruitCharacter(t->charid, 1);  // location=1 (mission)
    
    return t;
}

// ---------- 2. 生成 Zombie ----------
thing* CreateZombie(Vector3D lpos) {
    // 1. 分配实体槽
    thing* t = CreateRandomEntity();  // 内部调用 AllocateEntity(2)
    if (!t) return nullptr;
    // 2.设置出生位置
    t->pos = lpos;
    
    return t;
}

// ---------- 3. 生成 Item (武器) ----------
thing* CreateWeaponItem(Vector3D lpos, uint weapon_id) {
    // 1. 分配实体槽 + 初始化武器
    thing* t = CreateWeaponEntity(weapon_id);  // 内部: AllocateEntity(3) + InitWeaponThing
    if (!t) return nullptr;
    // 2. 设置位置
    t->pos = lpos;
    return t;
}

// ---------- 4. 生成 Item (拾取物) ----------
// 底层: AllocateEntity(3)
thing* CreatePickupItem(Vector3D lpos, uint loot, uint amount) {
    // 1. 分配实体槽
    thing* t = AllocateThing(1);  // subtype = PICKUP
    if (!t) return nullptr;
    // 2. 设置位置
    t->pos = lpos;
    // 3. 设置物品属性
    t->amount = amount; // 物品数量
    t->loot = loot; // 获取战利品种类
    t->nopick = 0; // 可拾取
    return t;
}

// ---------- 5. 生成 Item (家具/静态物) ----------
// 底层: AllocateEntity(3)
thing* CreateFurniture(Vector3D lpos, float w, float h) {
    // 1. 分配实体槽
    thing* t = AllocateThing(0);  // subtype = FURNITURE
    if (!t) return nullptr;
    // 2. 设置位置
    t->pos = lpos;
    return t;
}

// ---------- 9. 释放 Thing ----------
void DestroyThing(thing* t) {
    if (!t) return;
    
    // 如果是人类，先解绑角色
    if (t->type == 1 && t->charid != 0) {
        uint charId = t->charid;
        // 清理角色绑定
        character* charData = GetCharacterData(t);
        charData->cur_thingid = 0;  // cur_thingid = 0
    }
    
    // 如果是载具/携带物，先释放
    if (t->carryid != 0) {
        ThingDropCarry(t);
    }
    
    // 释放实体
    FreeThing(t);
}
