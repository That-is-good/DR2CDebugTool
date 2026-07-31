#include <qtypes.h>

struct ThingData {
    quint16 id = 0; // 0x00

    // 0x02 类型 子类型 
    quint8 type[2];

    
    quint8 mapid = 0; // 0x04 地图ID

    quint8 nocollide = 0; // 0x0d 无碰撞

    // 0x12 不可见, 不绘制
    quint8 vision[2] = {0}; 

    // 0x2c 位置 速度
    float vec3d[2][3] = {0};

    // 0x58 mass, friction, bounce_friction
    float phy[3] = {0};

    quint8 glow = 0; // 0x70 发光

    qint32 amount; // 0xe4 数量
    quint8 loot; // 0xe8 战利品 数值对应Resources

    // 0x148 角色ID 僵尸类型
    quint32 charidAndZomtype[2] = {0};

    qint32 hitpoints = 0; // 0x254

    // 0x27a 不可被击中 不造成伤害
    quint8 hit[2] = {0};

    quint32 ai_state = 0; // 0x288 AI状态
    qint32 ai_wait = 0; // 0x2a8 AI等待

    quint64 addr = 0;
};

struct CharacterData {
    // 0x00 角色ID 该角色对应的ThingID
    quint32 id[2] = {0};

    QString name; // 0x1c 角色名
    QString perk; // 0x44 特质
    QString trait; // 0x6c 特长

    quint16 femalePet[2]; // 0x94 女性标志, 宠物
    qint32 health = 0; // 0x140 血量
    QString description; // 0x144 描述

    // 0x1BC 属性是否已知 基础属性 临时属性 附加属性
    qint8 stats[4][13] = {0};

    float speed_bonus = 0;        // 0x1F0 额外速度
    
    qint32 mod_flags[2];            // 0x1f8 状态标志 状态标志2

    // 资源
    qint32 resource[8] = {0}; // 0x288

    // 0x2b0
    // weaponslots[槽位][内容]
    // 武器数量 武器ID 武器锁
    qint32 weaponslots[3][3] = {0};

    quint64 addr = 0;
};

struct MissionStateData {
    quint32 player_char[4] = {0}; // 0x18 当前队伍角色在角色中的顺序, 从1开始
    qint32 resource[8] = {0}; // 0x28 资源

    // 0x48
    // weaponslots[槽位][内容]
    // 武器数量 武器ID
    qint32 storage_slots[15][2] = {0}; 
    // storage_slots[15]: 3行5列, 每列[2] ID+Stack
};
