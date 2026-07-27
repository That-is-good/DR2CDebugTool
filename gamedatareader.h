#ifndef GAMEDATAREADER_H
#define GAMEDATAREADER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <cstdint>

class MemoryManager;

// 对应 Struct/struct.cpp 的结构体数据

struct ThingData {
    uint16_t id = 0;
    uint8_t type = 0;
    uint8_t subtype = 0;
    uint8_t mapid = 0;

    uint8_t nocollide = 0;
    uint8_t unseen = 0;
    uint8_t invisible = 0;

    float pos[3] = {0}; // x, y, z
    float vel[3] = {0}; // vel_x, vel_y, vel_z
    float phy[3] = {0}; // mass, friction, bounce_friction

    uint8_t fade = 0;
    uint8_t no_lighting = 0;
    uint8_t glow = 0;
    int32_t hitpoints = 0;
    uint8_t no_hit = 0;
    uint8_t no_do_damage = 0;
    uint8_t pause = 0;
    uint32_t ai_state = 0;
    uint32_t ai_wait = 0;
};

struct CharacterData {
    uint16_t cur_thingid = 0;
    QString name;
    QString perk;
    QString trait;
    uint16_t female = 0;
    int32_t health = 0;
    QString description;

    // 属性块
    int8_t display_stat[13] = {0}; // 0x1BC
    int8_t base_stats[13] = {0};   // 0x1C9
    int8_t bonus_stats[13] = {0};  // 0x1E3
    float speed_bonus = 0;          // 0x1F0
    uint8_t status = 0;             // 0x1F8

    // 资源
    int32_t resource[7] = {0}; // 0x288

    // 武器槽 [3]
    int32_t weapon_stack[3] = {0};
    int32_t weapon_id[3] = {0};
    int32_t weapon_lock[3] = {0};
};

struct MissionStateData {
    uint32_t player_char[4] = {0};
    int32_t resource[7] = {0};
    // Storage_slots[15]: 3行5列, 每列 ID+Stack
    int32_t storage_id[15] = {0};
    int32_t storage_stack[15] = {0};
};

class GameDataReader : public QObject
{
    Q_OBJECT

public:
    explicit GameDataReader(MemoryManager *memMgr, QObject *parent = nullptr);

    void setModuleBase(uintptr_t moduleBase);

    void SetOffset(QList<intptr_t>);
    void SetSize(QList<uint>);
    void SetLength(QList<ushort>);

    // ---- 实体 ----
    ThingData readThing(int index) const;
    QList<ThingData> readAllThings() const;
    bool writeThing(int index, const ThingData &data) const;
    int maxThings() const { return THING_LENGTH; }
    uintptr_t calcThingAddress(int index) const;

    // ---- 角色 ----
    CharacterData readCharacter(int index) const;
    QList<CharacterData> readAllCharacters() const;
    bool writeCharacter(int index, const CharacterData &data) const;
    int maxCharacters() const { return CHARACTER_LENGTH;}
    uintptr_t calcCharacterAddress(int index) const;

    // 角色单个字段写入
    bool writeCharacterName(int index, const QString &name);
    bool writeCharacterHealth(int index, int32_t health);
    bool writeCharacterSpeedBonus(int index, float speed);
    bool writeCharacterStatus(int index, uint8_t status);
    bool writeCharacterPerk(int index, const QString &perk);
    bool writeCharacterTrait(int index, const QString &trait);
    bool writeCharacterDescription(int index, const QString &desc);
    bool writeCharacterResource(int index, int resourceSlot, int32_t value);
    bool writeCharacterWeapon(int charIndex, int weaponSlot, int32_t id, int32_t stack, int32_t lock);
    bool writeCharacterStat(int charIndex, int statIndex, int8_t baseVal, int8_t bonusVal);

    // ---- 本局游戏状态 ----
    MissionStateData readMissionState() const;
    bool writeMissionResource(int slot, int32_t value);

    // ---- 武器池 ----
    QStringList readAllWeaponNames() const;
    QString readWeaponName(int index) const;
    int maxWeapons() const { return WEAPON_LENGTH; }
    uintptr_t calcWeaponAddress(int index) const;

    bool isAttached() const;

signals:
    void dataChanged();

private:
    MemoryManager *m_memMgr;

    uintptr_t m_thingPoolBase = 0x5632E0;
    uintptr_t m_charPoolBase = 0x5E25D8;
    uintptr_t m_weaponPoolBase = 0x4E0080;
    uintptr_t m_missionStateBase = 0x5E2238;
    uintptr_t m_moduleBase = 0;

    uint THING_SIZE = 0x304;
    uint CHARACTER_SIZE = 0x2e0;
    uint WEAPON_SIZE = 0x1c4;

    ushort THING_LENGTH = 610;
    ushort CHARACTER_LENGTH = 256;
    ushort WEAPON_LENGTH = 1024;
};

#endif // GAMEDATAREADER_H