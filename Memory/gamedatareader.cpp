#include "gamedatareader.h"

static inline QString readFixedUtf8String(const quint8* src, int maxLen) 
{
    int len = qstrnlen(reinterpret_cast<const char*>(src), maxLen);
    return QString::fromUtf8(reinterpret_cast<const char*>(src), len);
}

GameDataReader::GameDataReader(MemoryManager *memMgr, QObject *parent)
    : QObject(parent)
    , m_memMgr(memMgr)
{
}

void GameDataReader::setModuleBase(quint64 moduleBase)
{
    m_moduleBase = moduleBase;
}

void GameDataReader::SetOffset(QList<quint64> offset){
    m_thingPoolBase = offset[0];
    m_charPoolBase = offset[1];
    m_weaponPoolBase = offset[2];
    m_missionStateBase = offset[3];
}

void GameDataReader::SetSize(QList<quint32> size){
    THING_SIZE = size[0];
    CHARACTER_SIZE = size[1];
    WEAPON_SIZE = size[2];
}

void GameDataReader::SetLength(QList<quint16> length){
    THING_LENGTH = length[0];
    CHARACTER_LENGTH = length[1];
    WEAPON_LENGTH = length[2];
}

quint64 GameDataReader::calcThingAddress(qint32 index) const
{
    if (index < 0 || index >= maxThings()) return 0;
    return m_moduleBase + m_thingPoolBase + static_cast<quint64>(index) * THING_SIZE;
}

quint64 GameDataReader::calcCharacterAddress(qint32 index) const
{
    if (index < 0 || index >= maxCharacters()) return 0;
    return m_moduleBase + m_charPoolBase + static_cast<quint64>(index) * CHARACTER_SIZE;
}

quint64 GameDataReader::calcWeaponAddress(qint32 index) const
{
    if (index < 0) return 0;
    return m_moduleBase + m_weaponPoolBase + static_cast<quint64>(index) * WEAPON_SIZE;
}

bool GameDataReader::isAttached() const
{
    return m_memMgr && m_memMgr->isAttached();
}

// ==================== 辅助 ====================
qint32 GameDataReader::GetCurrentMapID(){
    qint32 mapid;
    if (!m_memMgr->read<qint32>(m_moduleBase + m_currentMapIdBase, mapid))
    {
        mapid = -1;
    }
    return mapid;
}

// ==================== 实体 ====================
ThingData GameDataReader::readThing(qint32 index) const
{
    ThingData data;
    quint64 addr = calcThingAddress(index);
    if (!addr || !isAttached()) return data;

    m_memMgr->read<quint16>(addr + 0x00, data.id);

    m_memMgr->readMemory(addr + 0x02, data.type, sizeof(quint8) * 2);

    m_memMgr->read<quint8>(addr + 0x04, data.mapid);

    m_memMgr->readMemory(addr + 0x2C, data.vec3d, sizeof(float) * 6);
    m_memMgr->readMemory(addr + 0x58, data.phy, sizeof(float) * 3);

    m_memMgr->read<quint8>(addr + 0x0D, data.nocollide);
    
    m_memMgr->readMemory(addr + 0x12, data.vision, sizeof(quint8) * 2);
    m_memMgr->readMemory(addr + 0x27A, data.hit, sizeof(quint8) * 2);

    m_memMgr->read<quint8>(addr + 0x70, data.glow);
    m_memMgr->read<qint32>(addr + 0x254, data.hitpoints);
    m_memMgr->read<quint32>(addr + 0x288, data.ai_state);
    m_memMgr->read<qint32>(addr + 0x2A8, data.ai_wait);

    data.addr = addr;

    return data;
}

QList<ThingData> GameDataReader::readAllThings() const
{
    QList<ThingData> list;
    if (!isAttached() || !m_moduleBase) return list;
    list.reserve(maxThings());

    qint64 poolSize = static_cast<qint64>(maxThings()) * THING_SIZE;
    QByteArray pool(poolSize, '\0');
    if (!m_memMgr->readMemory(m_moduleBase + m_thingPoolBase, pool.data(), poolSize)) {
        for (qint32 i = 0; i < maxThings(); ++i)
            list.append(readThing(i));
        return list;
    }

    const quint8 *base = reinterpret_cast<const quint8*>(pool.constData());
    for (qint32 i = 0; i < maxThings(); ++i) {
        const quint8 *p = base + static_cast<qint64>(i) * THING_SIZE;
        ThingData data;

        memcpy(&data.id, p + 0x00, sizeof(quint16));
        memcpy(data.type, p + 0x02, sizeof(quint8) * 2);   // type, subtype
        data.mapid = p[0x04];
        memcpy(data.vec3d, p + 0x2C, sizeof(float) * 6);    // pos + vel
        memcpy(data.phy, p + 0x58, sizeof(float) * 3);      // mass, friction, bounce
        data.nocollide = p[0x0D];
        memcpy(data.vision, p + 0x12, sizeof(quint8) * 2);  // unseen, invisible
        memcpy(data.hit, p + 0x27A, sizeof(quint8) * 2);    // no_hit, no_do_damage
        data.glow = p[0x70];
        memcpy(&data.hitpoints, p + 0x254, sizeof(qint32));
        memcpy(&data.ai_state, p + 0x288, sizeof(quint32));
        memcpy(&data.ai_wait, p + 0x2A8, sizeof(qint32));

        data.addr = m_moduleBase + m_thingPoolBase + static_cast<quint64>(i) * THING_SIZE;

        list.append(data);
    }
    return list;
}

bool GameDataReader::writeThing(quint64 addr, const ThingData &data)
{
    if (!addr || !isAttached()) return false;

    bool ok = true;

    ok &= m_memMgr->write<quint8>(addr + 0x04, data.mapid);

    ok &= m_memMgr->writeMemory(addr + 0x2C, data.vec3d, sizeof(float) * 6);
    ok &= m_memMgr->writeMemory(addr + 0x58, data.phy, sizeof(float) * 3);

    ok &= m_memMgr->write<quint8>(addr + 0x0D, data.nocollide);
    
    ok &= m_memMgr->writeMemory(addr + 0x12, data.vision, sizeof(quint8) * 2);
    ok &= m_memMgr->writeMemory(addr + 0x27A, data.hit, sizeof(quint8) * 2);

    ok &= m_memMgr->write<quint8>(addr + 0x70, data.glow);
    ok &= m_memMgr->write<qint32>(addr + 0x254, data.hitpoints);
    ok &= m_memMgr->write<quint32>(addr + 0x288, data.ai_state);
    ok &= m_memMgr->write<qint32>(addr + 0x2A8, data.ai_wait);

    return ok;
}

ThingData GameDataReader::modifyThing(qint32 index, std::function<void(ThingData&)> modifier)
{
    ThingData data = readThing(index);
    modifier(data);
    writeThing(data.addr, data);
    return data;
}

// ==================== 角色 ====================
CharacterData GameDataReader::readCharacter(qint32 index) const
{
    CharacterData data;
    quint64 addr = calcCharacterAddress(index);
    if (!addr || !isAttached()) return data;

    m_memMgr->readMemory(addr + 0x00, data.id, sizeof(quint32) * 2);

    data.name = m_memMgr->readString(addr + 0x1C, 40);
    data.perk = m_memMgr->readString(addr + 0x44, 40);
    data.trait = m_memMgr->readString(addr + 0x6C, 40);

    m_memMgr->readMemory(addr + 0x94, data.femalePet, sizeof(quint16) * 2);

    m_memMgr->read<qint32>(addr + 0x140, data.health);
    data.description = m_memMgr->readString(addr + 0x144, 120);

    // 批量读取属性块
    m_memMgr->readMemory(addr + 0x1BC, data.stats, sizeof(qint8) * 52);

    m_memMgr->read<float>(addr + 0x1F0, data.speed_bonus);

    m_memMgr->readMemory(addr + 0x1F8, data.mod_flags, sizeof(qint32) * 2);
    m_memMgr->readMemory(addr + 0x288, data.resource, sizeof(qint32) * 8);
    m_memMgr->readMemory(addr + 0x2B0, data.weaponslots, sizeof(qint32) * 9);

    data.addr = addr;

    return data;
}

QList<CharacterData> GameDataReader::readAllCharacters() const
{
    QList<CharacterData> list;
    if (!isAttached() || !m_moduleBase) return list;
    list.reserve(maxCharacters());

    qint64 poolSize = static_cast<qint64>(maxCharacters()) * CHARACTER_SIZE;
    QByteArray pool(poolSize, '\0');
    if (!m_memMgr->readMemory(m_moduleBase + m_charPoolBase, pool.data(), poolSize)) {
        for (qint32 i = 0; i < maxCharacters(); ++i)
            list.append(readCharacter(i));
        return list;
    }

    const quint8 *base = reinterpret_cast<const quint8*>(pool.constData());
    const quint64 poolBase = m_moduleBase + m_charPoolBase;

    for (qint32 i = 0; i < maxCharacters(); ++i) {
        const quint8 *p = base + static_cast<qint64>(i) * CHARACTER_SIZE;
        CharacterData data;

        memcpy(data.id, p + 0x00, sizeof(quint32) * 2);

        data.name = readFixedUtf8String(p + 0x1C, 40);
        data.perk = readFixedUtf8String(p + 0x44, 40);
        data.trait = readFixedUtf8String(p + 0x6C, 40);

        memcpy(data.femalePet, p + 0x94, sizeof(quint16) * 2);
        memcpy(&data.health, p + 0x140, sizeof(qint32));

        data.description = readFixedUtf8String(p + 0x144, 120);

        memcpy(data.stats, p + 0x1BC, sizeof(qint8) * 52);
        memcpy(&data.speed_bonus, p + 0x1F0, sizeof(float));
        memcpy(data.mod_flags, p + 0x1F8, sizeof(qint32) * 2);
        memcpy(data.resource, p + 0x288, sizeof(qint32) * 8);
        memcpy(data.weaponslots, p + 0x2B0, sizeof(qint32) * 9);

        data.addr = poolBase + static_cast<quint64>(i) * CHARACTER_SIZE;

        list.append(data);
    }
    return list;
}

bool GameDataReader::writeCharacter(quint64 addr, const CharacterData &data)
{
    if (!addr || !isAttached()) return false;

    bool ok = true;

    ok &= m_memMgr->writeMemory(addr + 0x00, data.id, sizeof(quint32) * 2);

    ok &= m_memMgr->writeString(addr + 0x1C, data.name, 40);
    ok &= m_memMgr->writeString(addr + 0x44, data.perk, 40);
    ok &= m_memMgr->writeString(addr + 0x6C, data.trait, 40);

    ok &= m_memMgr->writeMemory(addr + 0x94, data.femalePet, sizeof(quint16) * 2);

    ok &= m_memMgr->write<qint32>(addr + 0x140, data.health);

    ok &= m_memMgr->writeString(addr + 0x144, data.description, 120);

    ok &= m_memMgr->writeMemory(addr + 0x1BC, data.stats, sizeof(qint8) * 52);

    ok &= m_memMgr->write<float>(addr + 0x1F0, data.speed_bonus);

    ok &= m_memMgr->writeMemory(addr + 0x1F8, data.mod_flags, sizeof(qint32) * 2);
    ok &= m_memMgr->writeMemory(addr + 0x288, data.resource, sizeof(qint32) * 8);
    ok &= m_memMgr->writeMemory(addr + 0x2B0, data.weaponslots, sizeof(qint32) * 9);

    return ok;
}

CharacterData GameDataReader::modifyCharacter(qint32 index, std::function<void(CharacterData&)> modifier)
{
    CharacterData data = readCharacter(index);
    modifier(data);
    writeCharacter(data.addr, data);
    return data;
}

// ==================== 本局游戏状态 ====================
MissionStateData GameDataReader::readMissionState() const
{
    MissionStateData data;
    if (!isAttached() || !m_moduleBase) return data;

    quint64 addr = m_moduleBase + m_missionStateBase;
    m_memMgr->readMemory(addr + 0x18, data.player_char, sizeof(qint32) * 4);
    m_memMgr->readMemory(addr + 0x28, data.resource, sizeof(qint32) * 8);
    m_memMgr->readMemory(addr + 0x48, data.storage_slots, sizeof(qint32) * 30);
    return data;
}

bool GameDataReader::writeMission(const MissionStateData &data)
{
    if (!isAttached() || !m_moduleBase) return false;

    quint64 addr = m_moduleBase + m_missionStateBase;
    bool ok = true;
    
    ok &= m_memMgr->writeMemory(addr + 0x28, data.resource, sizeof(qint32) * 8);
    ok &= m_memMgr->writeMemory(addr + 0x48, data.storage_slots, sizeof(qint32) * 30);
    
    return ok;
}

// ==================== 武器池 ====================
QString GameDataReader::readWeaponName(qint32 index) const
{
    quint64 addr = calcWeaponAddress(index);
    if (!addr || !isAttached()) return QString();
    return m_memMgr->readString(addr, 40);
}

QStringList GameDataReader::readAllWeaponNames() const
{
    QStringList names;
    if (!isAttached() || !m_moduleBase) return names;
    names.reserve(maxWeapons());

    // 一次批量读取整个武器池（0x401 * 0x1C4 ≈ 452KB），然后在本地解析
    // 只读取每个 weapon 结构体的 name[40] 部分，但这里批量读整个池再解析
    qint64 poolSize = static_cast<qint64>(maxWeapons()) * WEAPON_SIZE;
    QByteArray pool(poolSize, '\0');
    if (!m_memMgr->readMemory(m_moduleBase + m_weaponPoolBase, pool.data(), poolSize)) {
        // 批量失败，回退逐条
        for (qint32 i = 0; i < maxWeapons(); ++i) {
            QString name = readWeaponName(i);
            if (name == "UNDEFINED")
                break;
            names.append(name);
        }
        return names;
    }

    const quint8 *base = reinterpret_cast<const quint8*>(pool.constData());
    for (qint32 i = 0; i < maxWeapons(); ++i) {
        const char *namePtr = reinterpret_cast<const char*>(base + static_cast<qint64>(i) * WEAPON_SIZE);
        // 寻找空终止符，最多40字节
        qint64 len = qstrnlen(namePtr, 40);
        if (len == 0) break; // name 为空，后续武器未定义
        QString name = QString::fromUtf8(namePtr, static_cast<qint32>(len));
        if (name == "UNDEFINED")
            return names;
        names.append(name);
    }
    return names;
}
