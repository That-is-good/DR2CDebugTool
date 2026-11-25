#include "spawndialog.h"

#include "spriteatlas.h"
#include "../Memory/gamedatareader.h"
#include "../Memory/memorymanager.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSet>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QVariant>

#include <limits>

namespace {

// kind：与 MainWindow::onSpawnEntity 一致；sheet：-1 表示该类别不使用精灵图，
// -2 表示使用武器列表，其余表示走精灵图页（图集内容随 mod 变化，不再按类别固定对应）。
struct CategoryDef {
    QString name;
    int kind;
    int sheet;
};

const QVector<CategoryDef> &categories()
{
    static const QVector<CategoryDef> defs = {
        {SpawnDialog::tr("人类"),     1, -1},
        {SpawnDialog::tr("僵尸"),     2, -1},
        {SpawnDialog::tr("物品"),     3,  0},
        {SpawnDialog::tr("抛射物"),   4, -1},
        {SpawnDialog::tr("家具"),     5,  4},
        {SpawnDialog::tr("拾取物"),   6,  2},
        {SpawnDialog::tr("武器"),     7, -2},
        {SpawnDialog::tr("车辆"),     8, 10},
        {SpawnDialog::tr("特殊拾取"), 9,  6},
    };
    return defs;
}

// 现有实体 → 类别行。ty==3 时按子类型区分家具/拾取物/武器/车辆/特殊拾取。
int categoryRowForThing(const ThingData &th)
{
    const int ty = th.type[0];
    const int sub = th.type[1];
    if (ty == 3) {
        switch (sub) {
        case 0: return 4;
        case 1: return 5;
        case 2: return 6;
        case 3: return 7;
        case 4: return 8;
        default: break;
        }
        return 4;
    }
    if (ty >= 1 && ty <= 4)
        return ty - 1; // 1→人类 2→僵尸 4→抛射物
    return 0;
}

} // namespace

SpawnDialog::SpawnDialog(MemoryManager *memMgr,
                         GameDataReader *gameData,
                         SpriteAtlas *atlas,
                         Mode mode,
                         const ThingData *initial,
                         QWidget *parent)
    : QDialog(parent)
    , m_memMgr(memMgr)
    , m_gameData(gameData)
    , m_atlas(atlas)
    , m_mode(mode)
{
    setWindowTitle(m_mode == Mode::Edit ? tr("修改实体属性") : tr("新建实体"));
    resize(920, 700);
    buildUi();

    if (m_atlas && m_atlas->isReady()) {
        for (const SpriteSheet &sheet : m_atlas->sheets())
            m_sheetCombo->addItem(sheet.title);
    }
    populateCategories();
    populateScripts();

    if (m_mode == Mode::Edit) {
        m_categoryList->setEnabled(false); // 修改时类型锁定
        m_categoryList->setToolTip(tr("修改实体时不能更改类型"));
        if (initial)
            applyInitial(*initial);
    } else {
        m_categoryList->setCurrentRow(0);
        onCategoryChanged(0);
    }
}

void SpawnDialog::buildUi()
{
    auto *mainLayout = new QHBoxLayout(this);

    // 左：类别
    auto *leftLayout = new QVBoxLayout();
    leftLayout->addWidget(new QLabel(tr("类别"), this));
    m_categoryList = new QListWidget(this);
    m_categoryList->setFixedWidth(120);
    leftLayout->addWidget(m_categoryList, 1);
    mainLayout->addLayout(leftLayout);

    // 中：内容
    m_stack = new QStackedWidget(this);

    auto *spritePage = new QWidget(this);
    auto *spriteLayout = new QVBoxLayout(spritePage);
    m_sheetCombo = new QComboBox(spritePage);
    spriteLayout->addWidget(m_sheetCombo);
    m_grid = new QListWidget(spritePage);
    m_grid->setViewMode(QListView::IconMode);
    m_grid->setResizeMode(QListView::Adjust);
    m_grid->setMovement(QListView::Static);
    m_grid->setUniformItemSizes(true);
    m_grid->setIconSize(QSize(64, 64));
    m_grid->setGridSize(QSize(92, 100));
    m_grid->setSpacing(2);
    m_grid->setWordWrap(false);
    spriteLayout->addWidget(m_grid, 1);
    m_stack->addWidget(spritePage);

    auto *weaponPage = new QWidget(this);
    auto *weaponLayout = new QVBoxLayout(weaponPage);
    m_weaponFilter = new QLineEdit(weaponPage);
    m_weaponFilter->setPlaceholderText(tr("按名称或索引筛选"));
    weaponLayout->addWidget(m_weaponFilter);
    m_weaponList = new QListWidget(weaponPage);
    weaponLayout->addWidget(m_weaponList, 1);
    m_stack->addWidget(weaponPage);

    auto *plainPage = new QWidget(this);
    auto *plainLayout = new QVBoxLayout(plainPage);
    plainLayout->addWidget(new QLabel(tr("该类别无需选择精灵图，直接设置参数后生成。"), plainPage));
    plainLayout->addStretch(1);
    m_stack->addWidget(plainPage);

    mainLayout->addWidget(m_stack, 1);

    // 右：预览与参数
    auto *rightLayout = new QVBoxLayout();
    m_preview = new QLabel(this);
    m_preview->setFixedSize(128, 128);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setFrameShape(QFrame::StyledPanel);
    rightLayout->addWidget(m_preview, 0, Qt::AlignHCenter);

    m_infoLabel = new QLabel(this);
    m_infoLabel->setWordWrap(true);
    m_infoLabel->setMinimumWidth(190);
    rightLayout->addWidget(m_infoLabel);

    auto *form = new QFormLayout();
    m_spriteSpin = new QSpinBox(this);
    m_spriteSpin->setRange(0, 65535);
    form->addRow(tr("精灵图 ID"), m_spriteSpin);

    m_hpSpin = new QSpinBox(this);
    m_hpSpin->setRange(-65535, 65535);
    form->addRow(tr("血量"), m_hpSpin);

    m_aiLabel = new QLabel(tr("AI状态"), this);
    m_aiSpin = new QSpinBox(this);
    m_aiSpin->setRange(0, 65535);
    form->addRow(m_aiLabel, m_aiSpin);
    rightLayout->addLayout(form);

    // 脚本（cyoa / events / action）：下拉列出当前可用的脚本对象
    auto *scriptBox = new QWidget(this);
    // 三个槽的通俗说明放在悬停提示里，避免占用界面空间
    scriptBox->setToolTip(tr("脚本槽说明：\n"
                             "cyoa：别人主动交互它时运行（选项/对话/商店）\n"
                             "events：游戏派发事件时运行（受击、死亡、拾取等）\n"
                             "action：别人对它做动作时运行（被使用/被操作）\n"
                             "三者互不覆盖，可同时填写；选“不修改”则不写入。拾取物不需要脚本。"));
    auto *scriptForm = new QFormLayout(scriptBox);
    scriptForm->setContentsMargins(0, 0, 0, 0);
    auto makeScriptCombo = [this, scriptForm](QComboBox *&combo, const QString &text,
                                              const QString &description) {
        auto *label = new QLabel(text, this);
        combo = new QComboBox(this);
        combo->setEditable(true);
        combo->setToolTip(description + tr("\n可直接输入脚本对象地址，例如 0x5A1B2C3D"));
        label->setToolTip(combo->toolTip());
        combo->lineEdit()->setPlaceholderText(tr("选择或输入地址 0x..."));
        scriptForm->addRow(label, combo);
    };
    makeScriptCombo(m_cyoaCombo, tr("cyoa 脚本"),
                    tr("主动交互脚本：角色主动与该实体交互时才运行（选项/对话/商店）。"
                       "不交互不会执行。"));
    makeScriptCombo(m_eventsCombo, tr("events 脚本"),
                    tr("事件回调脚本：游戏派发事件时运行（受击、死亡、到达目标、被拾取等）。"
                       "被动触发，无法主动执行。"));
    makeScriptCombo(m_actionCombo, tr("action 脚本"),
                    tr("动作脚本：其他角色对该实体执行动作时运行（被使用/被操作）。"
                       "需要 AI 真的对它执行动作才会触发。"));
    rightLayout->addWidget(scriptBox);

    // 拾取物：数量 / 战利品
    m_lootBox = new QWidget(this);
    auto *lootForm = new QFormLayout(m_lootBox);
    lootForm->setContentsMargins(0, 0, 0, 0);
    m_amountSpin = new QSpinBox(m_lootBox);
    m_amountSpin->setRange(-999999, 999999);
    m_amountSpin->setValue(1);
    lootForm->addRow(tr("数量"), m_amountSpin);
    m_lootCombo = new QComboBox(m_lootBox);
    // 战利品类型必须是 1~7：游戏交互逻辑要求 amount>0 且 loot!=0，
    // 否则靠近时只会提示“这里什么也没有”。因此不提供“无”选项。
    // 数值顺序来自脚本常量：1=LOOT_GAS 2=LOOT_FOOD 3=LOOT_MEDICAL
    // 4=LOOT_BULLET 5=LOOT_RIFLE_SHELL 6=LOOT_SHOTGUN_SHELL 7=LOOT_JUNK
    const QStringList lootNames = {
        tr("汽油"), tr("食物"), tr("医疗"),
        tr("子弹"), tr("步枪弹"), tr("霰弹"), tr("废料")
    };
    for (int i = 0; i < lootNames.size(); ++i)
        m_lootCombo->addItem(lootNames[i], i + 1);
    lootForm->addRow(tr("战利品"), m_lootCombo);
    m_lootBox->setVisible(false);
    rightLayout->addWidget(m_lootBox);

    // 车辆属性
    m_vehicleBox = new QWidget(this);
    auto *vehicleForm = new QFormLayout(m_vehicleBox);
    vehicleForm->setContentsMargins(0, 0, 0, 0);
    auto makePart = [this, vehicleForm](const QString &label) {
        QSpinBox *spin = new QSpinBox(this);
        spin->setRange(-128, 127);
        vehicleForm->addRow(label, spin);
        return spin;
    };
    m_chassis = makePart(tr("底盘"));
    m_chassisMax = makePart(tr("底盘上限"));
    m_engine = makePart(tr("引擎"));
    m_engineMax = makePart(tr("引擎上限"));
    m_armour = makePart(tr("装甲"));
    m_armourMax = makePart(tr("装甲上限"));
    m_speed = makePart(tr("速度"));
    m_speedMax = makePart(tr("速度上限"));

    m_repair = new QSpinBox(m_vehicleBox);
    m_repair->setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    vehicleForm->addRow(tr("修理值"), m_repair);

    m_mpg = new QDoubleSpinBox(m_vehicleBox);
    m_mpg->setDecimals(2);
    m_mpg->setRange(-65535.0, 65535.0);
    vehicleForm->addRow(tr("MPG"), m_mpg);

    // 新建载具时给一套“能开”的基础配置：部件全为 0 的车是一辆残骸。
    m_chassis->setValue(1);
    m_chassisMax->setValue(1);
    m_engine->setValue(1);
    m_engineMax->setValue(1);
    m_armour->setValue(1);
    m_armourMax->setValue(1);
    m_speed->setValue(1);
    m_speedMax->setValue(1);
    m_repair->setValue(10);
    m_mpg->setValue(15.0);

    m_vehicleBox->setToolTip(tr("改动任一车辆属性后会整组写入"));
    m_vehicleBox->setVisible(false);
    rightLayout->addWidget(m_vehicleBox);

    auto *buttons = new QDialogButtonBox(this);
    buttons->addButton(m_mode == Mode::Edit ? tr("应用") : tr("生成"),
                       QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    rightLayout->addWidget(buttons);
    rightLayout->addStretch(1);

    mainLayout->addLayout(rightLayout);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_categoryList, &QListWidget::currentRowChanged,
            this, &SpawnDialog::onCategoryChanged);
    connect(m_sheetCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (m_populating) return;
        populateGrid(index);
    });
    connect(m_grid, &QListWidget::itemSelectionChanged,
            this, &SpawnDialog::onGridSelectionChanged);
    connect(m_spriteSpin, &QSpinBox::valueChanged, this, [this](int) { updatePreview(); });
    connect(m_weaponList, &QListWidget::itemSelectionChanged,
            this, &SpawnDialog::onWeaponSelectionChanged);
    connect(m_weaponFilter, &QLineEdit::textChanged, this, [this](const QString &text) {
        for (int i = 0; i < m_weaponList->count(); ++i) {
            QListWidgetItem *item = m_weaponList->item(i);
            item->setHidden(!text.isEmpty()
                            && !item->text().contains(text, Qt::CaseInsensitive));
        }
    });

    // “已改动”标记：新建时未动过的分组保持游戏默认值
    connect(m_amountSpin, &QSpinBox::valueChanged, this, [this](int) {
        if (!m_filling) m_request.amountDirty = true;
    });
    connect(m_lootCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        if (!m_filling) m_request.lootDirty = true;
    });
    for (QSpinBox *spin : {m_chassis, m_chassisMax, m_engine, m_engineMax, m_armour,
                           m_armourMax, m_speed, m_speedMax, m_repair}) {
        connect(spin, &QSpinBox::valueChanged, this, [this](int) {
            if (!m_filling) m_request.vehicleDirty = true;
        });
    }
    connect(m_mpg, &QDoubleSpinBox::valueChanged, this, [this](double) {
        if (!m_filling) m_request.vehicleDirty = true;
    });
}

void SpawnDialog::populateCategories()
{
    QSignalBlocker blocker(m_categoryList);
    for (const CategoryDef &def : categories())
        m_categoryList->addItem(def.name);
}

// 脚本对象无法可靠地由文件名反查，这里直接列出“已知可用地址”：
// 游戏内置默认脚本 + 当前所有实体上挂载的脚本，去重后按地址显示。
void SpawnDialog::populateScripts()
{
    QComboBox *combos[3] = {m_cyoaCombo, m_eventsCombo, m_actionCombo};
    for (QComboBox *combo : combos) {
        combo->clear();
        combo->addItem(tr("不修改"), QVariant());
        combo->addItem(tr("0x00000000（无）"), 0u);
    }

    QSet<quint32> seen;
    auto addPointer = [&combos, &seen](quint32 ptr, const QString &label) {
        if (ptr == 0 || seen.contains(ptr))
            return;
        seen.insert(ptr);
        const QString text = QString("0x%1  %2").arg(ptr, 8, 16, QChar('0')).arg(label);
        for (QComboBox *combo : combos)
            combo->addItem(text, ptr);
    };

    addPointer(m_gameData ? m_gameData->GetDefaultScript(0) : 0, tr("默认 cyoa"));
    addPointer(m_gameData ? m_gameData->GetDefaultScript(1) : 0, tr("默认 events(人类)"));
    addPointer(m_gameData ? m_gameData->GetDefaultScript(2) : 0, tr("默认 events(僵尸)"));
    addPointer(m_gameData ? m_gameData->GetDefaultScript(3) : 0, tr("默认 action"));

    if (m_gameData && m_gameData->isAttached()) {
        const QVector<ThingData> things = m_gameData->readAllThings();
        for (const ThingData &thing : things) {
            if (thing.id == 0)
                continue;
            addPointer(thing.cyoa, tr("实体 %1 的 cyoa").arg(thing.id));
            addPointer(thing.events, tr("实体 %1 的 events").arg(thing.id));
            addPointer(thing.action, tr("实体 %1 的 action").arg(thing.id));
        }
    }
}

bool SpawnDialog::scriptValueFromCombo(QComboBox *combo, quint32 *value) const
{
    if (!combo || !value)
        return false;
    const int index = combo->currentIndex();
    if (index >= 0) {
        const QVariant data = combo->itemData(index);
        if (!data.isValid())
            return false; // “不修改”
        *value = data.toUInt();
        return true;
    }
    // 用户手动输入的地址
    QString text = combo->currentText().trimmed();
    text = text.section(QChar(' '), 0, 0);
    bool ok = false;
    quint32 parsed = 0;
    if (text.startsWith("0x", Qt::CaseInsensitive))
        parsed = text.mid(2).toUInt(&ok, 16);
    else
        parsed = text.toUInt(&ok, 16);
    if (!ok)
        return false;
    *value = parsed;
    return true;
}

void SpawnDialog::setScriptComboValue(QComboBox *combo, quint32 value)
{
    if (!combo)
        return;
    int index = combo->findData(value);
    if (index < 0 && value != 0) {
        combo->addItem(QString("0x%1").arg(value, 8, 16, QChar('0')), value);
        index = combo->count() - 1;
    }
    if (index >= 0)
        combo->setCurrentIndex(index);
    else
        combo->setEditText(QString("0x%1").arg(value, 8, 16, QChar('0')));
}

void SpawnDialog::onCategoryChanged(int row)
{
    const QVector<CategoryDef> &defs = categories();
    if (row < 0 || row >= defs.size())
        return;
    const CategoryDef &def = defs.at(row);
    m_request.kind = def.kind;
    m_request.weaponId = -1;
    m_request.spriteId = 0;
    m_spriteSpin->setValue(0); // 切换类别时清掉上一个类别的精灵

    // 修改模式下没有“武器列表”这一步，改用武器图集选择精灵
    int sheet = def.sheet;
    if (m_mode == Mode::Edit && sheet == -2)
        sheet = 0;

    if (sheet == -2) {
        m_stack->setCurrentIndex(1);
        populateWeapons();
    } else if (sheet < 0) {
        m_stack->setCurrentIndex(2);
        m_spriteSpin->setValue(0);
        updateInfoLabel();
        m_preview->setPixmap(QPixmap());
        m_preview->setText(tr("无"));
    } else {
        m_stack->setCurrentIndex(0);
        // 图集会随 mod 增删，类别与图集下标的固定对应已失效，不再按类别自动切换图集，
        // 直接沿用手动选择的当前项
        const int current = m_sheetCombo->currentIndex();
        populateGrid(current >= 0 ? current : 0);
        updateInfoLabel();
    }
    updateCategorySpecific();
}

void SpawnDialog::updateCategorySpecific()
{
    const QVector<CategoryDef> &defs = categories();
    const int row = m_categoryList->currentRow();
    const bool loot = row >= 0 && row < defs.size() && defs.at(row).kind == 6;
    const bool vehicle = row >= 0 && row < defs.size() && defs.at(row).kind == 8;
    const bool zombie = row >= 0 && row < defs.size() && defs.at(row).kind == 2;
    m_lootBox->setVisible(loot);
    m_vehicleBox->setVisible(vehicle);

    // AI 状态只有僵尸才真正生效，其它类型隐藏并清零
    m_aiLabel->setVisible(zombie);
    m_aiSpin->setVisible(zombie);
    if (!zombie)
        m_aiSpin->setValue(0);
}

void SpawnDialog::populateGrid(int sheetIndex)
{
    m_grid->clear();
    m_currentSheet = sheetIndex;
    if (!m_atlas || !m_atlas->isReady())
        return;
    const QVector<SpriteSheet> &sheets = m_atlas->sheets();
    if (sheetIndex < 0 || sheetIndex >= sheets.size())
        return;
    const SpriteSheet &sheet = sheets.at(sheetIndex);
    if (!sheet.loaded())
        return;
    // 静态表在 baseId 未读到时不能列帧；运行时段的 baseId 可以是 0
    if (sheet.baseId == 0 && sheet.path.isEmpty())
        return;

    // 超大的瓦片图集一次性生成几万个图标会卡死界面，这里限制预览数量
    const int count = qMin(sheet.frameCount(), 4000);
    m_grid->setUpdatesEnabled(false);
    for (int frame = 0; frame < count; ++frame) {
        const quint32 id = sheet.baseId + static_cast<quint32>(frame);
        auto *item = new QListWidgetItem(m_grid);
        item->setIcon(QIcon(m_atlas->pixmap(id, 64)));
        item->setText(QString::number(id));
        item->setData(Qt::UserRole, id);
        item->setToolTip(m_atlas->describe(id));
        item->setSizeHint(m_grid->gridSize());
    }
    m_grid->setUpdatesEnabled(true);
}

void SpawnDialog::populateWeapons()
{
    if (m_weaponList->count() > 0)
        return;
    if (!m_gameData || !m_gameData->isAttached())
        return;

    const QVector<WeaponInfo> weapons = m_gameData->readAllWeaponInfo();
    if (weapons.isEmpty()) {
        const QStringList names = m_gameData->readAllWeaponNames();
        for (int i = 0; i < names.size(); ++i) {
            auto *item = new QListWidgetItem(QStringLiteral("[%1] %2").arg(i).arg(names.at(i)),
                                             m_weaponList);
            item->setData(Qt::UserRole, i);
            item->setData(Qt::UserRole + 1, 0);
        }
        return;
    }

    m_weaponList->setUpdatesEnabled(false);
    for (const WeaponInfo &weapon : weapons) {
        auto *item = new QListWidgetItem(
            QStringLiteral("[%1] %2").arg(weapon.index).arg(weapon.name), m_weaponList);
        item->setData(Qt::UserRole, weapon.index);
        item->setData(Qt::UserRole + 1, weapon.spriteId);
    }
    m_weaponList->setUpdatesEnabled(true);
}

void SpawnDialog::onGridSelectionChanged()
{
    QListWidgetItem *item = m_grid->currentItem();
    if (!item)
        return;
    const quint32 id = item->data(Qt::UserRole).toUInt();
    m_request.spriteId = id;
    m_spriteSpin->setValue(static_cast<int>(id));
    updatePreview();
}

void SpawnDialog::onWeaponSelectionChanged()
{
    QListWidgetItem *item = m_weaponList->currentItem();
    if (!item)
        return;
    m_request.weaponId = item->data(Qt::UserRole).toInt();
    const quint32 spriteId = item->data(Qt::UserRole + 1).toUInt();
    m_request.spriteId = spriteId;
    m_spriteSpin->setValue(static_cast<int>(spriteId));
    updatePreview();
    updateInfoLabel(tr("武器索引 %1（生成时由游戏直接初始化武器数据）")
                        .arg(m_request.weaponId));
}

void SpawnDialog::updatePreview()
{
    updateInfoLabel();
    const quint32 id = static_cast<quint32>(m_spriteSpin->value());
    m_request.spriteId = id;
    if (!m_atlas || !m_atlas->isReady() || id == 0) {
        m_preview->setPixmap(QPixmap());
        m_preview->setText(tr("无预览"));
        return;
    }
    const QPixmap pixmap = m_atlas->pixmap(id, 120);
    if (pixmap.isNull()) {
        m_preview->setPixmap(QPixmap());
        m_preview->setText(tr("无预览"));
        return;
    }
    m_preview->setPixmap(pixmap);
}

// 右侧信息文字始终显示“类别”，有精灵时再追加精灵描述，
// 避免切换类别后仍停留在上一个类别的文字。
void SpawnDialog::updateInfoLabel(const QString &extra)
{
    const QVector<CategoryDef> &defs = categories();
    const int row = m_categoryList->currentRow();
    QStringList lines;
    if (row >= 0 && row < defs.size())
        lines << tr("类别：%1").arg(defs.at(row).name);
    const quint32 id = static_cast<quint32>(m_spriteSpin->value());
    if (m_atlas && m_atlas->isReady() && id != 0) {
        const QString desc = m_atlas->describe(id);
        if (!desc.isEmpty())
            lines << desc;
    }
    if (!extra.isEmpty())
        lines << extra;
    m_infoLabel->setText(lines.join(QStringLiteral("\n")));
}

void SpawnDialog::applyInitial(const ThingData &thing)
{
    m_filling = true;

    const int row = categoryRowForThing(thing);
    m_categoryList->setCurrentRow(row);
    onCategoryChanged(row);

    m_request.kind = categories().at(row).kind;
    m_spriteSpin->setValue(thing.spriteid);
    m_hpSpin->setValue(thing.hitpoints);
    m_aiSpin->setValue(static_cast<int>(thing.ai_state));
    m_amountSpin->setValue(thing.amount);
    const int lootIndex = m_lootCombo->findData(static_cast<int>(thing.loot));
    m_lootCombo->setCurrentIndex(lootIndex >= 0 ? lootIndex : 0);

    setScriptComboValue(m_cyoaCombo, thing.cyoa);
    setScriptComboValue(m_eventsCombo, thing.events);
    setScriptComboValue(m_actionCombo, thing.action);

    m_chassis->setValue(thing.chassis);
    m_chassisMax->setValue(thing.chassis_max);
    m_engine->setValue(thing.engine);
    m_engineMax->setValue(thing.engine_max);
    m_armour->setValue(thing.armour);
    m_armourMax->setValue(thing.armour_max);
    m_speed->setValue(thing.speed);
    m_speedMax->setValue(thing.speed_max);
    m_repair->setValue(thing.repair);
    m_mpg->setValue(static_cast<double>(thing.mpg));

    // 在图集中定位并选中当前精灵
    int sheet = categories().at(row).sheet;
    if (m_mode == Mode::Edit && sheet == -2)
        sheet = 0;
    if (sheet >= 0 && thing.spriteid != 0 && m_atlas && m_atlas->isReady()) {
        int sheetIndex = -1;
        int frameIndex = -1;
        if (m_atlas->locate(thing.spriteid, &sheetIndex, &frameIndex)) {
            if (sheetIndex != m_currentSheet) {
                m_populating = true;
                m_sheetCombo->setCurrentIndex(sheetIndex);
                m_populating = false;
                populateGrid(sheetIndex);
            }
            for (int i = 0; i < m_grid->count(); ++i) {
                QListWidgetItem *item = m_grid->item(i);
                if (item->data(Qt::UserRole).toUInt() == thing.spriteid) {
                    m_grid->setCurrentItem(item);
                    m_grid->scrollToItem(item);
                    break;
                }
            }
        }
    }

    m_filling = false;
    updatePreview();
}

void SpawnDialog::accept()
{
    const QVector<CategoryDef> &defs = categories();
    const int row = m_categoryList->currentRow();
    const bool zombie = row >= 0 && row < defs.size() && defs.at(row).kind == 2;

    m_request.spriteId = static_cast<quint32>(m_spriteSpin->value());
    m_request.hitpoints = m_hpSpin->value();
    // AI 状态只有僵尸有效，其它类型一律写 0
    m_request.aiState = zombie ? static_cast<quint32>(m_aiSpin->value()) : 0u;

    QComboBox *scriptCombos[3] = {m_cyoaCombo, m_eventsCombo, m_actionCombo};
    for (int i = 0; i < 3; ++i) {
        quint32 value = 0;
        if (scriptValueFromCombo(scriptCombos[i], &value)) {
            m_request.scriptDirty[i] = true;
            m_request.scripts[i] = value;
        }
    }

    if (row >= 0 && row < defs.size()) {
        if (defs.at(row).kind == 6) {
            // 拾取物必须 amount>0 且 loot!=0，否则游戏交互时只会提示“这里什么也没有”
            m_request.amount = qMax(1, m_amountSpin->value());
            m_request.loot = m_lootCombo->currentData().toInt();
            if (m_request.loot <= 0)
                m_request.loot = 1;
            m_request.amountDirty = true;
            m_request.lootDirty = true;
        } else if (defs.at(row).kind == 8) {
            m_request.chassis = static_cast<qint8>(m_chassis->value());
            m_request.chassisMax = static_cast<qint8>(m_chassisMax->value());
            m_request.engine = static_cast<qint8>(m_engine->value());
            m_request.engineMax = static_cast<qint8>(m_engineMax->value());
            m_request.armour = static_cast<qint8>(m_armour->value());
            m_request.armourMax = static_cast<qint8>(m_armourMax->value());
            m_request.speed = static_cast<qint8>(m_speed->value());
            m_request.speedMax = static_cast<qint8>(m_speedMax->value());
            m_request.repair = m_repair->value();
            m_request.mpg = static_cast<float>(m_mpg->value());
            m_request.vehicleDirty = true;
            // 血量留空时给 100，与游戏自身的载具生成一致
            if (m_request.hitpoints <= 0)
                m_request.hitpoints = 100;
        }
    }

    // 修改模式：应用全部字段
    if (m_mode == Mode::Edit) {
        m_request.amountDirty = true;
        m_request.lootDirty = true;
        m_request.vehicleDirty = true;
    }

    QDialog::accept();
}
