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

    qint32 maxThings() const { return THING_LENGTH; }
    qint32 maxCharacters() const { return CHARACTER_LENGTH;}

    quint64 calcThingAddress(qint32) const;
    quint64 calcCharacterAddress(qint32) const;

    ThingData readThing(qint32) const;
    QList<ThingData> readAllThings() const;
    bool writeThing(quint64, const ThingData&);
    ThingData modifyThing(qint32, std::function<void(ThingData&)>);

    CharacterData readCharacter(qint32) const;
    QList<CharacterData> readAllCharacters() const;
    bool writeCharacter(quint64, const CharacterData&);
    CharacterData modifyCharacter(qint32, std::function<void(CharacterData&)>);

    // ---- 本局游戏状态 ----
    MissionStateData readMissionState() const;
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

    quint64 m_thingPoolBase = 0x5632E0;
    quint64 m_charPoolBase = 0x5E25D8;
    quint64 m_weaponPoolBase = 0x4E0080;
    quint64 m_missionStateBase = 0x5E2238;
    quint64 m_moduleBase = 0;

    quint32 THING_SIZE = 0x304;
    quint32 CHARACTER_SIZE = 0x2e0;
    quint32 WEAPON_SIZE = 0x1c4;

    quint16 THING_LENGTH = 610;
    quint16 CHARACTER_LENGTH = 256;
    quint16 WEAPON_LENGTH = 1024;
};

#endif // GAMEDATAREADER_H