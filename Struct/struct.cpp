// ============================================================
// 常量定义
// ============================================================

typedef char sbyte;
typedef unsigned char byte;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef void* Pointer;

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
    
    byte nocollide;             // 0x0d 无碰撞
    byte nopush;                // 0x10 不可推动
    byte nopick;                // 0x11 不可拾取
    byte unseen;                // 0x12 不渲染
    byte invisible;             // 0x13 不绘制
    Vector3D pos;               // 0x2c 位置
    Vector3D vel;               // 0x38 速度

    float mass;                 // 0x58 质量
    float friction;             // 0x5c 阻力
    float bounce_friction;      // 0x60 弹跳阻力

    int glow;                   // 0x70 是否发光

    Vector3D rad;               // 0xac 半径

    ushort spriteid;            // 0xd8 精灵ID

    int amount;                 // 0xe4 数量
    byte loot;                  // 0xe8 战利品 数值对应Resources

    uint charid;                // 0x148 角色 ID
    uint zombietype;            // 0x14c 僵尸类型

    ushort carryid;             // 0x200 携带的ID

    byte chassis;               // 0x208 底盘
    byte chassis_max;           // 0x209 最大底盘
    byte engine;                // 0x20a 引擎
    byte engine_max;            // 0x20b 最大底盘
    byte armour;                // 0x20c 装甲
    byte armour_max;            // 0x20d 最大装甲
    byte carspeed;              // 0x20e 速度
    byte carspeed_max;          // 0x20f 最大速度
    int repair;                 // 0x210 修理
    float mpg;                  // 0x214 油耗

    int hitpoints;              // 0x254 (角色: 流血计时器, 僵尸: 血量, 物品: 耐久, 投射物: 存活)

    byte no_hit;                // 0x27a 不可被击中
    byte no_do_damage;          // 0x27b 不造成伤害

    uint ai_state;              // 0x288 AI状态
    uint ai_wait;               // 0x2a8 AI等待
};

// 角色 结构体
// 角色偏移0x5E25D8
// 大小: 0x2e0
// 最多: 0x100个角色
struct character {
    uint id;                    // 0x00 角色ID (最多256)
    uint cur_thingid;           // 0x04 该角色对应的Thing ID

    char name[40];              // 0x1c 角色名
    char perk[40];              // 0x44 特质
    char trait[40];             // 0x6c 特长
    ushort female;              // 0x94 女性标志
    ushort pet;                 // 0x96 宠物

    uint health;                // 0x140 血量
    char description[120];      // 0x144 描述
    Stats displayStat;          // 0x1bc 属性是否已知
    Stats baseStat;             // 0x1c9 基础属性
    Stats tempStat;             // 0x1d6 临时属性
    Stats bonusStat;            // 0x1e3 附加属性
    float speed_bonus;          // 0x1f0 额外速度
    
    int mod_flags1;            // 0x1f8 状态标志
    int mod_flags2;            // 0x1fc 状态标志2
    
    Resources localResource;    // 0x288 资源

    WeaponSlots3D weapon_slots[3];  // 0x2b0 武器槽
};

// 武器 结构体
// 大小: 0x1c4
// 武器池偏移 0x004E0080;
// 最多0x401个武器
struct weapon {
    char name[40];              // 0x00 名字
};

// MISSION STATE STRUCTURE (本局游戏状态)
// 起始: 0x5E2238
// 大小: 因为没有数组，所以无所谓
struct mission_state {
    uint player_char[4];        // 0x18 当前队伍角色在角色中的顺序, 从1开始
    Resources storageResource;  // 0x28 资源
    WeaponSlots2D Storage_slots[15];    // 0x48 仓库武器槽
};

/*

// ============================================================
// THING 事件
// ============================================================
// THING_EVENT 常量 (来自 ScriptDefineThingConstants)
#define THING_EVENT_NEW          1   // 新建实体
#define THING_EVENT_PICKUP       2   // 被拾取
#define THING_EVENT_DROP         3   // 被丢弃
#define THING_EVENT_ACTION       4   // 执行动作
#define THING_EVENT_NO_LOOT      5   // 无战利品
#define THING_EVENT_BREAK        6   // 被破坏
#define THING_EVENT_DIED         7   // 死亡 (僵尸/人类)
#define THING_EVENT_DID_HIT      8   // 击中目标
#define THING_EVENT_WAS_HIT      9   // 被击中
#define THING_EVENT_DO_INSIDE    10  // 进入内部
#define THING_EVENT_BUMP         16  // 碰撞
#define THING_EVENT_PUSH         17  // 被推动
#define THING_EVENT_OVERLAP      18  // 重叠
#define THING_EVENT_SWING        19  // 挥动武器
#define THING_EVENT_SHOOT        20  // 射击
#define THING_EVENT_THROW        21  // 投掷
#define THING_EVENT_TIMER        30  // 计时器
#define THING_EVENT_FREE         31  // 被释放
#define THING_EVENT_DID_BURN     15  // 被燃烧
#define THING_EVENT_WAS_BLASTED  32  // 被爆炸
#define THING_EVENT_UNBOLTED     33  // 被解除固定
#define THING_EVENT_AI_ASSESS    34  // AI评估
#define THING_EVENT_CUSTOM       35  // 自定义事件

// 以下偏移都是 addr - 0x400000

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

*/

/*
; int __cdecl ScriptEvaluateStringSafe(char *)  0x48FB60
ScriptEvaluateStringSafe proc near      ; CODE XREF: Evaluatescriptformatted+34↓p
                                        ; ScriptCompileWordDefinition+A2↓p

var_1C          = dword ptr -1Ch
var_18          = dword ptr -18h
arg_0           = dword ptr  4

                sub     esp, 1Ch
                mov     [esp+1Ch+var_1C], offset byte_A3FB20
                call    IsScriptInTry
                test    eax, eax
                jnz     short loc_48FBA0
                mov     eax, [esp+1Ch+arg_0]
                mov     dword_4D1550, 0
                mov     [esp+1Ch+var_1C], eax ; char *
                call    ScriptEvaluateString
                mov     dword_4D1550, 1
                add     esp, 1Ch
                retn
; ---------------------------------------------------------------------------
                align 10h

loc_48FBA0:                             ; CODE XREF: ScriptEvaluateStringSafe+11↑j
                mov     eax, [esp+1Ch+arg_0]
                mov     [esp+1Ch+var_1C], offset byte_A3FB20 ; int
                mov     [esp+1Ch+var_18], eax ; char *
                call    ScriptSetInput
                add     esp, 1Ch
                retn
ScriptEvaluateStringSafe endp

; void __cdecl PushThingEvent(int, _WORD *, int)    0x4546C0
PushThingEvent  proc near               ; CODE XREF: ThingCarry+40↓p
                                        ; Performcarryaction+321↓p ...

var_2C          = dword ptr -2Ch
var_28          = dword ptr -28h
var_24          = dword ptr -24h
var_20          = dword ptr -20h
var_1C          = dword ptr -1Ch
arg_0           = dword ptr  4
arg_4           = dword ptr  8
arg_8           = dword ptr  0Ch

                sub     esp, 2Ch
                mov     eax, [esp+2Ch+arg_8]
                mov     [esp+2Ch+var_1C], 1
                mov     [esp+2Ch+var_20], 0
                mov     [esp+2Ch+var_24], eax
                mov     eax, [esp+2Ch+arg_4]
                mov     [esp+2Ch+var_28], eax
                mov     eax, [esp+2Ch+arg_0]
                mov     [esp+2Ch+var_2C], eax
                call    PushCharacterEvent
                add     esp, 2Ch
                retn
PushThingEvent  endp

void __cdecl PushCharacterEvent(int a1, _WORD *a2, int a3, void *a4, int a5)
{
  BOOL v5; // eax
  __int16 v6; // bp
  __int16 v7; // di
  void *v8; // ecx
  int (__cdecl **v9)(int); // eax
  int v10; // eax
  char *CharacterData; // eax
  int StackDepth; // [esp+1Ch] [ebp-20h]

  if ( a1 != 0 )
  {
    v5 = IsScriptInTry(a1: (int)byte_A3FB20);
    v6 = v5;
    if ( !v5 )
    {
      v7 = *(_WORD *)(a1 + 82);
      *(_WORD *)(a1 + 82) = 0;
      if ( a2 != nullptr )
      {
        v6 = a2[41];
        *(_WORD *)(a1 + 82) = *a2;
        a2[41] = *(_WORD *)a1;
      }
      StackDepth = GetStackDepth(a1: (int)byte_A3FB20);
      Pushthingtoscript(a1);
      PushScriptValue(a1: (int)byte_A3FB20, a2: a3);
      v9 = *(int (__cdecl ***)(int))(a1 + 152);
      if ( v9 == nullptr )
      {
        v8 = a4;
        if ( a4 != nullptr )
        {
          v8 = (void *)*(unsigned __int8 *)(a1 + 2);
          v9 = (int (__cdecl **)(int))dword_A110BC;
          if ( (_BYTE)v8 != 1 )
          {
            v9 = (int (__cdecl **)(int))dword_A110B8;
            if ( (_BYTE)v8 != 2 )
              v9 = (int (__cdecl **)(int))dword_A110C0;
          }
        }
      }
      ExecuteScriptWord(this: v8, a2: v9);
      v10 = GetStackDepth(a1: (int)byte_A3FB20);
      ScriptPopN_0(a1: (int)byte_A3FB20, a2: v10 - StackDepth);
      if ( a5 != 0 && *(_DWORD *)(a1 + 328) != 0 )
      {
        CharacterData = GetCharacterData(a1);
        CharacterMainScriptEvent(a1: (int)CharacterData, a2: a3);
      }
      if ( a2 != nullptr )
        a2[41] = v6;
      *(_WORD *)(a1 + 82) = v7;
    }
  }
}
*/