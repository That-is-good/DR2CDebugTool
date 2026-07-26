#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QList>
#include <QVector>
#include <QStringList>
#include <QModelIndex>
#include <cstdint>

#include "Memory/gamedatareader.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MemoryManager;
class QStandardItemModel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // 进程
    void onRefreshProcess();
    void onAttachProcess();
    void onFilterProcessChanged(const QString &text);

    // 角色
    void onCharacterSelected(int index);
    void onCharacterNameChanged();
    void onCharacterPerkChanged();
    void onCharacterTraitChanged();
    void onCharacterDescriptionChanged();
    void onCharacterHpChanged(int value);
    void onCharacterSpeedChanged(double value);
    void onCharacterStatusToggled();
    void onCharacterStatChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void onCharacterResourceChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void onCharacterWeaponChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void onMissionStorageStackChanged(int row, int col);

    // 实体
    void onEntityTypeFilterChanged(int index);
    void onEntityAreaFilterChanged(int index);
    void onEntityTableSelectionChanged();
    void onEntityFlagToggled();
    void onEntityPosChanged();
    void onEntityVelChanged();
    void onEntityPhysicsChanged();
    void onEntityHitpointsChanged(int value);
    void onEntityAiStateChanged(int value);
    void onEntityAiWaitChanged(int value);

    // 实体操作
    void onSetTargetEntity();
    void onTeleportToTarget();
    void onSwapEntityPositions();

    // 全局
    void onMissionResourceChanged(int row, int column);

    // 定时刷新
    void onRefreshTimerChara();
    void onRefreshTimerEntity();
    void onRefreshTimerMission();
    void onRefreshTimer();

    // 武器按钮点击
    void onWeaponButtonClicked(int charIndex, int slot);
    void onStorageWeaponClicked(int slotIndex);

private:
    void setupUI();
    void setupConnections();
    void setupEntityTable();
    void setupCharacterStatTable();
    void setupCharacterResourceTable();
    void setupCharacterWeaponTable();
    void setupMissionResourceTable();
    void setupMissionWeaponTable();

    // 刷新
    void refreshAll();
    void refreshProcessList();
    void refreshCharacterData(int charIndex);
    void refreshEntityList();
    void refreshEntityData(int entityIndex);

    // 写入
    void writeCharacterName(int index);
    void writeCharacterPerk(int index);
    void writeCharacterTrait(int index);
    void writeCharacterDescription(int index);
    void writeCharacterHp(int index);
    void writeCharacterSpeed(int index);
    void writeCharacterStatus(int index);
    void writeCharacterStats(int index);
    void writeCharacterResources(int index);
    void writeCharacterWeaponSlot(int charIndex, int slot);
    void writeMissionStorageWeapon(int slotIndex);
    void writeMissionStorageStack(int slotIndex);
    void writeEntityFlag();
    void writeEntityPos();
    void writeEntityVel();
    void writeEntityPhysics();
    void writeEntityHitpoints();
    void writeEntityAiState();
    void writeEntityAiWait();
    void writeMissionResource();

    // 辅助
    void setControlsEnabled(bool enabled);
    int selectedCharacterIndex() const;
    int selectedEntityIndex() const;
    bool isAttached() const;
    bool hasEditingFocus() const;

    Ui::MainWindow *ui;

    MemoryManager *m_memMgr;
    GameDataReader *m_gameData;
    QTimer *m_refreshTimer;

    // 缓存
    QList<ThingData> m_thingCache;
    QList<CharacterData> m_charCache;
    MissionStateData m_missionCache;
    QStringList m_weaponNames;

    // 过滤
    int m_entityTypeFilter = -1;
    int m_entityAreaFilter = -1;
    int m_targetEntityIndex = -1;

    // 防递归
    bool m_updatingUI = false;
};

#endif // MAINWINDOW_H