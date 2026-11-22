#ifndef SPAWNDIALOG_H
#define SPAWNDIALOG_H

#include <QDialog>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QSpinBox;
class QStackedWidget;
class QWidget;
class MemoryManager;
class GameDataReader;
class SpriteAtlas;
struct ThingData;

// 新建 / 修改实体时收集到的参数。
// kind 与 MainWindow::onSpawnEntity 的 type 一致：
// 1 人类 / 2 僵尸 / 3 物品 / 4 抛射物 / 5 家具 / 6 拾取物 /
// 7 武器 / 8 车辆 / 9 特殊拾取
struct SpawnRequest {
    int kind = 1;
    quint32 spriteId = 0;
    int amount = 1;
    int hitpoints = 0;
    qint32 weaponId = -1;
    quint32 aiState = 0;

    // 用户是否改动过对应分组；新建时未改动的分组不写回，保留游戏默认值
    bool amountDirty = false;
    bool lootDirty = false;
    int loot = 0;
    bool vehicleDirty = false;

    qint8 chassis = 0;
    qint8 chassisMax = 0;
    qint8 engine = 0;
    qint8 engineMax = 0;
    qint8 armour = 0;
    qint8 armourMax = 0;
    qint8 speed = 0;
    qint8 speedMax = 0;
    int repair = 0;
    float mpg = 0.0f;
};

// 统一的“新建实体 / 修改实体属性”对话框。
// 新建模式可选择类别；修改模式锁定类别（不可切换），并用实体当前值预填。
class SpawnDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode { Create, Edit };

    SpawnDialog(MemoryManager *memMgr,
                GameDataReader *gameData,
                SpriteAtlas *atlas,
                Mode mode = Mode::Create,
                const ThingData *initial = nullptr,
                QWidget *parent = nullptr);

    SpawnRequest request() const { return m_request; }

private:
    void buildUi();
    void populateCategories();
    void populateGrid(int sheetIndex);
    void populateWeapons();
    void updatePreview();
    void updateCategorySpecific();
    void applyInitial(const ThingData &thing);
    void onCategoryChanged(int row);
    void onGridSelectionChanged();
    void onWeaponSelectionChanged();
    void accept() override;

    MemoryManager *m_memMgr;
    GameDataReader *m_gameData;
    SpriteAtlas *m_atlas;
    Mode m_mode = Mode::Create;

    QListWidget *m_categoryList = nullptr;
    QStackedWidget *m_stack = nullptr;

    QComboBox *m_sheetCombo = nullptr;
    QListWidget *m_grid = nullptr;

    QLineEdit *m_weaponFilter = nullptr;
    QListWidget *m_weaponList = nullptr;

    QLabel *m_preview = nullptr;
    QLabel *m_infoLabel = nullptr;
    QSpinBox *m_spriteSpin = nullptr;
    QSpinBox *m_hpSpin = nullptr;
    QSpinBox *m_aiSpin = nullptr;

    // 拾取物：数量 / 战利品
    QWidget *m_lootBox = nullptr;
    QSpinBox *m_amountSpin = nullptr;
    QComboBox *m_lootCombo = nullptr;

    // 车辆属性
    QWidget *m_vehicleBox = nullptr;
    QSpinBox *m_chassis = nullptr;
    QSpinBox *m_chassisMax = nullptr;
    QSpinBox *m_engine = nullptr;
    QSpinBox *m_engineMax = nullptr;
    QSpinBox *m_armour = nullptr;
    QSpinBox *m_armourMax = nullptr;
    QSpinBox *m_speed = nullptr;
    QSpinBox *m_speedMax = nullptr;
    QSpinBox *m_repair = nullptr;
    QDoubleSpinBox *m_mpg = nullptr;

    SpawnRequest m_request;
    int m_currentSheet = 0;
    bool m_populating = false;
    bool m_filling = false;   // 预填期间不记录“已改动”
};

#endif // SPAWNDIALOG_H
