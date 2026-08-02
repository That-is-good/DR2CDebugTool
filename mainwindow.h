#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QList>
#include <QVector>
#include <QStringList>
#include <QModelIndex>
#include <QTableWidget>
#include <QTableView>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QApplication>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QWidget>
#include <QScrollBar>
#include <QCoreApplication>
#include <QDir>

#include "Setting/addrsetting.h"
#include "Memory/gamedatareader.h"
#include "Memory/memorymanager.h"
#include "Delegates/spinboxdelegate.h"
#include "WeaponDialog/weapondialog.h"
#include "ui_mainwindow.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MemoryManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // 设置 / 进程
    void onSetting();
    void onRefreshProcess();
    void onAttachProcess();
    void onFilterProcessChanged(const QString &text);

    // 角色 - 合并槽
    void onCharacterSelected(int index);
    void onCharacterEditChanged();
    void onCharacterSpinBoxChanged();
    void onCharacterCheckBoxToggled();
    void onCharacterStatChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void onCharacterResourceChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void onCharacterWeaponChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);

    // 实体 - 合并槽
    void onEntityTypeFilterChanged(int index);
    void onEntityAreaFilterChanged(int index);
    void onEntityTableSelectionChanged();
    void onEntityCheckBoxToggled();
    void onEntityDoubleSpinBoxChanged();
    void onEntitySpinBoxChanged();

    // 实体操作
    void onSetTargetEntity();
    void onTeleportToTarget();
    void onSwapEntityPositions();
    void onDestoryEntity();
    void onSpawnEntity();

    // 全局
    void onMissionChanged();
    void onCmdSend();

    // 定时刷新
    void onRefreshTimer();
    void onRefreshTimerChara();
    void onRefreshTimerEntity();
    void onRefreshTimerMission();

    // 武器按钮
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

    void setBase();
    void refreshAll();
    void refreshProcessList();
    void refreshCharacterData(int charIndex);
    void refreshEntityList();
    void refreshEntityData(int entityIndex);

    // 辅助
    void setControlsEnabled(bool enabled);
    int selectedCharacterIndex() const;
    int selectedEntityIndex() const;
    bool isAttached() const;
    bool hasEditingFocus() const;

    Ui::MainWindow *ui;
    AddrSetting *setting;
    MemoryManager *m_memMgr;
    GameDataReader *m_gameData;
    QTimer *m_refreshTimer;

    QList<ThingData> m_thingCache;
    QList<CharacterData> m_charCache;
    MissionStateData m_missionCache;
    QStringList m_weaponNames;

    int m_entityTypeFilter = -1;
    int m_entityAreaFilter = -1;
    int m_targetEntityId = -1;

    QList<QString> resourceNames;
    QList<QString> statNames;

    bool m_updatingUI = false;
};

#endif // MAINWINDOW_H
