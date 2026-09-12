#ifndef GAMEDATAREADER_H
#define GAMEDATAREADER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>

#include "memorymanager.h"

class MemoryManager;


class GameDataReader : public QObject
{
    Q_OBJECT

public:
    explicit GameDataReader(MemoryManager *memMgr, QObject *parent = nullptr);

    void setModuleBase(quint64);
    void SetOffset(QList<quint64>);
    void SetSize(QList<quint32>);
    void SetLength(QList<quint16>);

    // ---- 辅助 ----
    qint32 GetCurrentMapID();
    bool SetCurrentMapID(qint32);
    qint32 GetLeaderThingID();
    qint32 GetLastEntityID();

    QString GettypeName(qint8);
    QString GetsubName(qint8, qint8);

    qint32 maxThings() const { return THING_LENGTH; }
    qint32 maxCharacters() const { return CHARACTER_LENGTH;}

    quint64 calcCharacterAddress(qint32) const;

    ThingData readThing(quint64 addr);
    QList<ThingData> readAllThings();
    bool writeThing(quint64, const ThingData&);
    ThingData modifyThing(quint64 addr, std::function<void(ThingData&)>);
    bool copyThing(quint64, quint64);

    // 区域元数据 (MapLayerMeta: 0x46DBC0, 每个0x34, 按mapid索引)
    MapAreaData readMapArea(qint32 mapid) const;
    QList<MapAreaData> readAllMapAreas(qint32 maxMapId) const;

    CharacterData readCharacter(qint32) const;
    QList<CharacterData> readAllCharacters() const;
    bool writeCharacter(quint64, const CharacterData&);
    CharacterData modifyCharacter(qint32, std::function<void(CharacterData&)>);

    // ---- 本局游戏状态 ----
    MissionStateData readMissionState() const;
    quint32 readMissionStateLeaderChar() const;
    std::array<quint32, 4> readMissionStatePlayerChar() const;
    bool writeMission(const MissionStateData&);

    // ---- 武器池 ----
    QStringList readAllWeaponNames() const;
    QString readWeaponName(qint32 index) const;
    qint32 maxWeapons() const { return WEAPON_LENGTH; }
    quint64 calcWeaponAddress(qint32 index) const;

    bool isAttached() const;

signals:
    void dataChanged();

private:
    MemoryManager *m_memMgr;

    quint64 m_currentMapIdBase = 0x46DBA4;
    quint64 m_lastEntityIdBase = 0x3CCD24;
    quint64 m_mapLayerMetaBase = 0x46DBC0;
    quint32 MAP_META_SIZE = 0x34;

    quint64 m_thingPoolBase = 0x5632E0;
    // 角色池嵌入在 mission_state 中 (missionStateBase + 0xC0)，索引0不存储角色
    quint64 m_charPoolBase = 0x5E22F8;
    quint64 m_weaponPoolBase = 0x4E0080;
    quint64 m_missionStateBase = 0x5E2238;
    quint64 m_moduleBase = 0;

    quint32 THING_SIZE = 0x304;
    quint32 CHARACTER_SIZE = 0x2e0;
    quint32 WEAPON_SIZE = 0x1c4;

    quint16 THING_LENGTH = 610;
    quint16 CHARACTER_LENGTH = 255;
    quint16 WEAPON_LENGTH = 1024;
};

#endif // GAMEDATAREADER_H