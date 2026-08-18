#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QList>
#include <QVector>
#include <QHash>
#include <QStringList>
#include <QModelIndex>
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
#include <QMenu>
#include <QBrush>
#include <QColor>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPixmap>
#include <QPointF>
#include <QSet>
#include <QGraphicsItemGroup>
class QGraphicsPixmapItem;
class QGraphicsEllipseItem;
class QGraphicsItemGroup;

#include "Setting/addrsetting.h"
#include "Memory/gamedatareader.h"
#include "Memory/memorymanager.h"
#include "Delegates/spinboxdelegate.h"
#include "WeaponDialog/weapondialog.h"

namespace Ui { class MainWindow; }

class MemoryManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

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

    // 实体 - 过滤 / 模式
    void onEntityAreaFilterChanged(int index);
    void onEntityModeChanged();

    // 实体操作
    void onEntityMenu(const QPoint &);
    void onEditEntityPosition(quint64 addr);
    void onEditEntityVelocity(quint64 addr);
    void onEditEntityPhysics(quint64 addr);
    void onEditEntityOther(quint64 addr);
    void onTeleportToTarget();
    void onSwapEntityPositions();
    void onDestoryEntity();
    quint64 onSpawnEntity(uint);
    void onCloneEntity();

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
    void setupEntityView();
    void setupCharacterStatTable();
    void setupCharacterResourceTable();
    void setupCharacterWeaponTable();
    void setupMissionResourceTable();
    void setupMissionWeaponTable();

    void setBase();
    void refreshAll();
    void refreshProcessList();
    void refreshCharacterData(int charIndex);
    void refreshEntityView();

    // 辅助
    void setControlsEnabled(bool enabled);
    int selectedCharacterIndex() const;

    // 实体以内存地址为唯一标识
    quint64 selectedEntityAddr() const;          // 当前选中实体地址
    QList<quint64> selectedEntityAddrs() const;  // 所有选中实体地址
    ThingData *thingByAddr(quint64 addr);        // 地址 → m_thingCache 实体
    void applyEntityFlagsToAddrs(const QList<quint64> &addrs, bool set, quint8 flag);
    int iconIndexForThing(const ThingData &th) const;
    int regionFromScenePos(const QPointF &scenePos) const;
    void moveEntityToScenePos(quint64 addr, const QPointF &scenePos);
    quint64 spawnEntityAt(uint type, const QPointF &scenePos);

    bool isAttached() const;
    bool hasEditingFocus() const;

    Ui::MainWindow *ui;
    AddrSetting *setting;
    MemoryManager *m_memMgr;
    GameDataReader *m_gameData;
    QTimer *m_refreshTimer;

    QGraphicsScene *m_entityScene = nullptr;
    QVector<ThingData> m_thingCache;             // 固定大小实体池，下标即池索引
    QHash<int, QPointF> m_regionOffsets;         // 区域ID → 场景原点偏移
    QHash<int, QSizeF> m_regionSizes;            // 区域ID → 场景区域尺寸

    // 固定实体池图元（槽位 = 实体池索引，预创建后仅更新属性，不重建场景）
    QVector<QGraphicsPixmapItem*> m_thingItems;
    QVector<QGraphicsEllipseItem*> m_thingMarkers;
    QVector<int> m_thingIconCache;               // 每个槽位当前图标索引
    QVector<QPixmap> m_iconPixmaps;              // 图标磁盘加载缓存
    QGraphicsItemGroup *m_backgroundGroup = nullptr; // 区域背景/标签组
    QString m_layoutSignature;                   // 区域布局签名，变化时重建背景

    // 拖拽实体状态
    bool m_draggingEntity = false;
    quint64 m_dragAddr = 0;
    QGraphicsItem *m_dragItem = nullptr;
    QPointF m_dragOffset;

    QList<CharacterData> m_charCache;
    MissionStateData m_missionCache;
    QStringList m_weaponNames;

    enum EntityMode { SelectMode, ResizeMode, MoveMode };
    EntityMode m_entityMode = SelectMode;

    quint64 m_selectedThingAddr = 0;             // 唯一的中心实体地址
    int m_entityAreaFilter = -1;

    QList<QString> resourceNames;
    QList<QString> statNames;

    bool m_updatingUI = false;
};

#endif // MAINWINDOW_H