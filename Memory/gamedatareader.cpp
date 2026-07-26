#include "gamedatareader.h"
#include "memorymanager.h"

#include <cstring>

GameDataReader::GameDataReader(MemoryManager *memMgr, QObject *parent)
    : QObject(parent)
    , m_memMgr(memMgr)
{
}

void GameDataReader::setModuleBase(uintptr_t moduleBase)
{
    m_moduleBase = moduleBase;
    m_thingPoolBase = moduleBase + 0x5632E0;
    m_charPoolBase = moduleBase + 0x5E25D8;
    m_weaponPoolBase = moduleBase + 0x4E0080;
    m_missionStateBase = moduleBase + 0x5E2238;
}

uintptr_t GameDataReader::calcThingAddress(int index) const
{
    if (index < 0 || index >= maxThings()) return 0;
    return m_thingPoolBase + static_cast<uintptr_t>(index) * THING_SIZE;
}

uintptr_t GameDataReader::calcCharacterAddress(int index) const
{
    if (index < 0 || index >= maxCharacters()) return 0;
    return m_charPoolBase + static_cast<uintptr_t>(index) * CHARACTER_SIZE;
}

uintptr_t GameDataReader::calcWeaponAddress(int index) const
{
    if (index < 0) return 0;
    return m_weaponPoolBase + static_cast<uintptr_t>(index) * WEAPON_SIZE;
}

bool GameDataReader::isAttached() const
{
    return m_memMgr && m_memMgr->isAttached();
}

// ==================== 实体 ====================
ThingData GameDataReader::readThing(int index) const
{
    ThingData data;
    uintptr_t addr = calcThingAddress(index);
    if (!addr || !isAttached()) return data;

    m_memMgr->read<uint16_t>(addr + 0x00, data.id);
    m_memMgr->read<uint8_t>(addr + 0x02, data.type);
    m_memMgr->read<uint8_t>(addr + 0x03, data.subtype);
    m_memMgr->read<uint8_t>(addr + 0x04, data.mapid);
    m_memMgr->read<uint8_t>(addr + 0x0D, data.nocollide);
    m_memMgr->read<uint8_t>(addr + 0x12, data.unseen);
    m_memMgr->read<uint8_t>(addr + 0x13, data.invisible);

    // 批量读取 pos[3], vel[3], phy[3] - 利用union连续内存特性
    m_memMgr->readBytes(addr + 0x2C, data.pos, sizeof(float) * 3);
    m_memMgr->readBytes(addr + 0x38, data.vel, sizeof(float) * 3);
    m_memMgr->readBytes(addr + 0x58, data.phy, sizeof(float) * 3);

    m_memMgr->read<uint8_t>(addr + 0x64, data.fade);
    m_memMgr->read<uint8_t>(addr + 0x6F, data.no_lighting);
    m_memMgr->read<uint8_t>(addr + 0x70, data.glow);
    m_memMgr->read<int32_t>(addr + 0x254, data.hitpoints);
    m_memMgr->read<uint8_t>(addr + 0x27A, data.no_hit);
    m_memMgr->read<uint8_t>(addr + 0x27B, data.no_do_damage);
    m_memMgr->read<uint8_t>(addr + 0x27E, data.pause);
    m_memMgr->read<uint32_t>(addr + 0x288, data.ai_state);
    m_memMgr->read<uint32_t>(addr + 0x2A8, data.ai_wait);

    return data;
}

QList<ThingData> GameDataReader::readAllThings() const
{
    QList<ThingData> list;
    if (!isAttached() || !m_thingPoolBase) return list;
    list.reserve(maxThings());

    // 一次批量读取整个实体池（0x262 * 0x304 ≈ 460KB），然后在本地解析
    size_t poolSize = static_cast<size_t>(maxThings()) * THING_SIZE;
    QByteArray pool(poolSize, '\0');
    if (!m_memMgr->readMemory(m_thingPoolBase, pool.data(), poolSize)) {
        // 批量读取失败，回退到逐条读取
        for (int i = 0; i < maxThings(); ++i)
            list.append(readThing(i));
        return list;
    }

    const uint8_t *base = reinterpret_cast<const uint8_t*>(pool.constData());
    for (int i = 0; i < maxThings(); ++i) {
        const uint8_t *p = base + static_cast<size_t>(i) * THING_SIZE;
        ThingData data;

        data.id       = *reinterpret_cast<const uint16_t*>(p + 0x00);
        data.type     = p[0x02];
        data.subtype  = p[0x03];
        data.mapid    = p[0x04];
        data.nocollide = p[0x0D];
        data.unseen   = p[0x12];
        data.invisible = p[0x13];

        memcpy(data.pos, p + 0x2C, sizeof(float) * 3);
        memcpy(data.vel, p + 0x38, sizeof(float) * 3);
        memcpy(data.phy, p + 0x58, sizeof(float) * 3);

        data.fade         = p[0x64];
        data.no_lighting  = p[0x6F];
        data.glow         = p[0x70];
        data.hitpoints    = *reinterpret_cast<const int32_t*>(p + 0x254);
        data.no_hit       = p[0x27A];
        data.no_do_damage = p[0x27B];
        data.pause        = p[0x27E];
        data.ai_state     = *reinterpret_cast<const uint32_t*>(p + 0x288);
        data.ai_wait      = *reinterpret_cast<const uint32_t*>(p + 0x2A8);

        list.append(data);
    }
    return list;
}

bool GameDataReader::writeThing(int index, const ThingData &data) const
{
    uintptr_t addr = calcThingAddress(index);
    if (!addr || !isAttached()) return false;

    bool ok = true;
    ok &= m_memMgr->write<uint8_t>(addr + 0x0D, data.nocollide);
    ok &= m_memMgr->write<uint8_t>(addr + 0x12, data.unseen);
    ok &= m_memMgr->write<uint8_t>(addr + 0x13, data.invisible);

    // 批量写入 pos[3], vel[3], phy[3]
    ok &= m_memMgr->writeBytes(addr + 0x2C, data.pos, sizeof(float) * 3);
    ok &= m_memMgr->writeBytes(addr + 0x38, data.vel, sizeof(float) * 3);
    ok &= m_memMgr->writeBytes(addr + 0x58, data.phy, sizeof(float) * 3);

    ok &= m_memMgr->write<uint8_t>(addr + 0x64, data.fade);
    ok &= m_memMgr->write<uint8_t>(addr + 0x6F, data.no_lighting);
    ok &= m_memMgr->write<uint8_t>(addr + 0x70, data.glow);
    ok &= m_memMgr->write<int32_t>(addr + 0x254, data.hitpoints);
    ok &= m_memMgr->write<uint8_t>(addr + 0x27A, data.no_hit);
    ok &= m_memMgr->write<uint8_t>(addr + 0x27B, data.no_do_damage);
    ok &= m_memMgr->write<uint8_t>(addr + 0x27E, data.pause);
    ok &= m_memMgr->write<uint32_t>(addr + 0x288, data.ai_state);
    ok &= m_memMgr->write<uint32_t>(addr + 0x2A8, data.ai_wait);

    return ok;
}

// ==================== 角色 ====================
CharacterData GameDataReader::readCharacter(int index) const
{
    CharacterData data;
    uintptr_t addr = calcCharacterAddress(index);
    if (!addr || !isAttached()) return data;

    m_memMgr->read<uint16_t>(addr + 0x04, data.cur_thingid);
    data.name = m_memMgr->readString(addr + 0x1C, 40);
    data.perk = m_memMgr->readString(addr + 0x44, 40);
    data.trait = m_memMgr->readString(addr + 0x6C, 40);
    m_memMgr->read<uint16_t>(addr + 0x94, data.female);
    m_memMgr->read<int32_t>(addr + 0x140, data.health);
    data.description = m_memMgr->readString(addr + 0x144, 120);

    // 批量读取属性块
    m_memMgr->readBytes(addr + 0x1BC, data.display_stat, 13);
    m_memMgr->readBytes(addr + 0x1C9, data.base_stats, 13);
    m_memMgr->readBytes(addr + 0x1E3, data.bonus_stats, 13);

    m_memMgr->read<float>(addr + 0x1F0, data.speed_bonus);
    m_memMgr->read<uint8_t>(addr + 0x1F8, data.status);

    // 资源
    m_memMgr->readBytes(addr + 0x288, data.resource, sizeof(int32_t) * 7);

    // 武器槽
    for (int i = 0; i < 3; ++i) {
        uintptr_t wAddr = addr + 0x2B0 + i * 12;
        m_memMgr->read<int32_t>(wAddr + 0x00, data.weapon_stack[i]);
        m_memMgr->read<int32_t>(wAddr + 0x04, data.weapon_id[i]);
        m_memMgr->read<int32_t>(wAddr + 0x08, data.weapon_lock[i]);
    }

    return data;
}

QList<CharacterData> GameDataReader::readAllCharacters() const
{
    QList<CharacterData> list;
    if (!isAttached() || !m_charPoolBase) return list;
    list.reserve(maxCharacters());

    // 一次批量读取整个角色池（0x100 * 0x2E0 ≈ 184KB），然后在本地解析
    size_t poolSize = static_cast<size_t>(maxCharacters()) * CHARACTER_SIZE;
    QByteArray pool(poolSize, '\0');
    if (!m_memMgr->readMemory(m_charPoolBase, pool.data(), poolSize)) {
        // 批量读取失败，回退到逐条读取
        for (int i = 0; i < maxCharacters(); ++i)
            list.append(readCharacter(i));
        return list;
    }

    const uint8_t *base = reinterpret_cast<const uint8_t*>(pool.constData());
    for (int i = 0; i < maxCharacters(); ++i) {
        const uint8_t *p = base + static_cast<size_t>(i) * CHARACTER_SIZE;
        CharacterData data;

        data.cur_thingid = *reinterpret_cast<const uint16_t*>(p + 0x04);

        // 定长字符串
        data.name        = QString::fromUtf8(reinterpret_cast<const char*>(p + 0x1C), qstrnlen(reinterpret_cast<const char*>(p + 0x1C), 40));
        data.perk        = QString::fromUtf8(reinterpret_cast<const char*>(p + 0x44), qstrnlen(reinterpret_cast<const char*>(p + 0x44), 40));
        data.trait       = QString::fromUtf8(reinterpret_cast<const char*>(p + 0x6C), qstrnlen(reinterpret_cast<const char*>(p + 0x6C), 40));
        data.female      = *reinterpret_cast<const uint16_t*>(p + 0x94);
        data.health      = *reinterpret_cast<const int32_t*>(p + 0x140);
        data.description = QString::fromUtf8(reinterpret_cast<const char*>(p + 0x144), qstrnlen(reinterpret_cast<const char*>(p + 0x144), 120));

        // 属性块
        memcpy(data.display_stat, p + 0x1BC, 13);
        memcpy(data.base_stats,   p + 0x1C9, 13);
        memcpy(data.bonus_stats,  p + 0x1E3, 13);

        data.speed_bonus = *reinterpret_cast<const float*>(p + 0x1F0);
        data.status      = p[0x1F8];

        // 资源
        memcpy(data.resource, p + 0x288, sizeof(int32_t) * 7);

        // 武器槽
        for (int slot = 0; slot < 3; ++slot) {
            const uint8_t *wp = p + 0x2B0 + slot * 12;
            data.weapon_stack[slot] = *reinterpret_cast<const int32_t*>(wp + 0x00);
            data.weapon_id[slot]    = *reinterpret_cast<const int32_t*>(wp + 0x04);
            data.weapon_lock[slot]  = *reinterpret_cast<const int32_t*>(wp + 0x08);
        }

        list.append(data);
    }
    return list;
}

bool GameDataReader::writeCharacter(int index, const CharacterData &data) const
{
    uintptr_t addr = calcCharacterAddress(index);
    if (!addr || !isAttached()) return false;

    bool ok = true;
    ok &= m_memMgr->write<uint16_t>(addr + 0x04, data.cur_thingid);
    ok &= m_memMgr->writeString(addr + 0x1C, data.name, 40);
    ok &= m_memMgr->writeString(addr + 0x44, data.perk, 40);
    ok &= m_memMgr->writeString(addr + 0x6C, data.trait, 40);
    ok &= m_memMgr->write<uint16_t>(addr + 0x94, data.female);
    ok &= m_memMgr->write<int32_t>(addr + 0x140, data.health);
    ok &= m_memMgr->writeString(addr + 0x144, data.description, 120);
    ok &= m_memMgr->writeBytes(addr + 0x1BC, data.display_stat, 13);
    ok &= m_memMgr->writeBytes(addr + 0x1C9, data.base_stats, 13);
    ok &= m_memMgr->writeBytes(addr + 0x1E3, data.bonus_stats, 13);
    ok &= m_memMgr->write<float>(addr + 0x1F0, data.speed_bonus);
    ok &= m_memMgr->write<uint8_t>(addr + 0x1F8, data.status);
    ok &= m_memMgr->writeBytes(addr + 0x288, data.resource, sizeof(int32_t) * 7);

    for (int i = 0; i < 3; ++i) {
        uintptr_t wAddr = addr + 0x2B0 + i * 12;
        ok &= m_memMgr->write<int32_t>(wAddr + 0x00, data.weapon_stack[i]);
        ok &= m_memMgr->write<int32_t>(wAddr + 0x04, data.weapon_id[i]);
        ok &= m_memMgr->write<int32_t>(wAddr + 0x08, data.weapon_lock[i]);
    }

    return ok;
}

bool GameDataReader::writeCharacterName(int index, const QString &name)
{
    uintptr_t addr = calcCharacterAddress(index);
    if (!addr || !isAttached()) return false;
    return m_memMgr->writeString(addr + 0x1C, name, 40);
}

bool GameDataReader::writeCharacterHealth(int index, int32_t health)
{
    uintptr_t addr = calcCharacterAddress(index);
    if (!addr || !isAttached()) return false;
    return m_memMgr->write<int32_t>(addr + 0x140, health);
}

bool GameDataReader::writeCharacterSpeedBonus(int index, float speed)
{
    uintptr_t addr = calcCharacterAddress(index);
    if (!addr || !isAttached()) return false;
    return m_memMgr->write<float>(addr + 0x1F0, speed);
}

bool GameDataReader::writeCharacterStatus(int index, uint8_t status)
{
    uintptr_t addr = calcCharacterAddress(index);
    if (!addr || !isAttached()) return false;
    return m_memMgr->write<uint8_t>(addr + 0x1F8, status);
}

bool GameDataReader::writeCharacterPerk(int index, const QString &perk)
{
    uintptr_t addr = calcCharacterAddress(index);
    if (!addr || !isAttached()) return false;
    return m_memMgr->writeString(addr + 0x44, perk, 40);
}

bool GameDataReader::writeCharacterTrait(int index, const QString &trait)
{
    uintptr_t addr = calcCharacterAddress(index);
    if (!addr || !isAttached()) return false;
    return m_memMgr->writeString(addr + 0x6C, trait, 40);
}

bool GameDataReader::writeCharacterDescription(int index, const QString &desc)
{
    uintptr_t addr = calcCharacterAddress(index);
    if (!addr || !isAttached()) return false;
    return m_memMgr->writeString(addr + 0x144, desc, 120);
}

bool GameDataReader::writeCharacterResource(int index, int resourceSlot, int32_t value)
{
    if (resourceSlot < 0 || resourceSlot >= 7) return false;
    uintptr_t addr = calcCharacterAddress(index);
    if (!addr || !isAttached()) return false;
    return m_memMgr->write<int32_t>(addr + 0x288 + resourceSlot * 4, value);
}

bool GameDataReader::writeCharacterWeapon(int charIndex, int weaponSlot, int32_t id, int32_t stack, int32_t lock)
{
    if (weaponSlot < 0 || weaponSlot >= 3) return false;
    uintptr_t addr = calcCharacterAddress(charIndex);
    if (!addr || !isAttached()) return false;
    uintptr_t wAddr = addr + 0x2B0 + weaponSlot * 12;
    bool ok = true;
    ok &= m_memMgr->write<int32_t>(wAddr + 0x00, stack);
    ok &= m_memMgr->write<int32_t>(wAddr + 0x04, id);
    ok &= m_memMgr->write<int32_t>(wAddr + 0x08, lock);
    return ok;
}

bool GameDataReader::writeCharacterStat(int charIndex, int statIndex, int8_t baseVal, int8_t bonusVal)
{
    if (statIndex < 0 || statIndex >= 13) return false;
    uintptr_t addr = calcCharacterAddress(charIndex);
    if (!addr || !isAttached()) return false;
    bool ok = true;
    ok &= m_memMgr->write<int8_t>(addr + 0x1C9 + statIndex, baseVal);
    ok &= m_memMgr->write<int8_t>(addr + 0x1E3 + statIndex, bonusVal);
    return ok;
}

// ==================== 本局游戏状态 ====================
MissionStateData GameDataReader::readMissionState() const
{
    MissionStateData data;
    if (!isAttached() || !m_missionStateBase) return data;

    uintptr_t addr = m_missionStateBase;
    m_memMgr->readBytes(addr + 0x18, data.player_char, sizeof(uint32_t) * 4);
    m_memMgr->readBytes(addr + 0x28, data.resource, sizeof(int32_t) * 7);

    for (int i = 0; i < 15; ++i) {
        uintptr_t sAddr = addr + 0x48 + i * 8;
        m_memMgr->read<int32_t>(sAddr + 0x00, data.storage_id[i]);
        m_memMgr->read<int32_t>(sAddr + 0x04, data.storage_stack[i]);
    }

    return data;
}

bool GameDataReader::writeMissionResource(int slot, int32_t value)
{
    if (slot < 0 || slot >= 7) return false;
    if (!isAttached() || !m_missionStateBase) return false;
    return m_memMgr->write<int32_t>(m_missionStateBase + 0x28 + slot * 4, value);
}

// ==================== 武器池 ====================
QString GameDataReader::readWeaponName(int index) const
{
    uintptr_t addr = calcWeaponAddress(index);
    if (!addr || !isAttached()) return QString();
    return m_memMgr->readString(addr, 40);
}

QStringList GameDataReader::readAllWeaponNames() const
{
    QStringList names;
    if (!isAttached() || !m_weaponPoolBase) return names;
    names.reserve(maxWeapons());

    // 一次批量读取整个武器池（0x401 * 0x1C4 ≈ 452KB），然后在本地解析
    // 只读取每个 weapon 结构体的 name[40] 部分，但这里批量读整个池再解析
    size_t poolSize = static_cast<size_t>(maxWeapons()) * WEAPON_SIZE;
    QByteArray pool(poolSize, '\0');
    if (!m_memMgr->readMemory(m_weaponPoolBase, pool.data(), poolSize)) {
        // 批量失败，回退逐条
        for (int i = 0; i < maxWeapons(); ++i) {
            QString name = readWeaponName(i);
            if (name == "UNDEFINED")
                break;
            names.append(name);
        }
        return names;
    }

    const uint8_t *base = reinterpret_cast<const uint8_t*>(pool.constData());
    for (int i = 0; i < maxWeapons(); ++i) {
        const char *namePtr = reinterpret_cast<const char*>(base + static_cast<size_t>(i) * WEAPON_SIZE);
        // 寻找空终止符，最多40字节
        size_t len = qstrnlen(namePtr, 40);
        if (len == 0) break; // name 为空，后续武器未定义
        QString name = QString::fromUtf8(namePtr, static_cast<int>(len));
        if (name == "UNDEFINED")
            return names;
        names.append(name);
    }
    return names;
}
