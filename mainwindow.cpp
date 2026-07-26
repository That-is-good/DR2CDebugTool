#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "Memory/memorymanager.h"
#include "Memory/spinboxdelegate.h"
#include "Memory/weapondialog.h"

#include <QTableWidget>
#include <QTableView>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QApplication>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QWidget>
#include <QScrollBar>

// 状态位
enum StatusBits {
    STATUS_SICK     = 1, STATUS_INJURED  = 2, STATUS_TIRED   = 4,
    STATUS_SUPERDOG = 8, STATUS_DOGMALUS = 16, STATUS_COFFEE = 32,
    STATUS_DOGPAL   = 64, STATUS_EXPLORER = 128
};

static const char* resourceNames[] = {
    "资源0", "食物", "汽油", "医疗", "子弹", "步枪", "霰弹"
};

static const char* statNames[] = {
    "士气", "态度", "镇静", "魅力", "智慧", "忠诚", "医疗技能",
    "机械技能", "射击", "力量", "灵巧", "体能", "活力"
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_memMgr(new MemoryManager(this))
    , m_gameData(new GameDataReader(m_memMgr, this))
    , m_refreshTimer(new QTimer(this))
{
    ui->setupUi(this);
    setupUI();
    setupConnections();
    setWindowTitle("DR2C 调试工具");
}

MainWindow::~MainWindow()
{
    // 必须在delete ui之前主动断开，避免m_memMgr析构时emit signal访问已销毁的ui
    m_memMgr->detachProcess();
    disconnect(m_memMgr, &MemoryManager::processDetached, 0, 0);
    setControlsEnabled(false);
    m_refreshTimer->stop();
    delete ui;
}

// ==================== 辅助 ====================
bool MainWindow::isAttached() const { return m_memMgr && m_memMgr->isAttached(); }

int MainWindow::selectedCharacterIndex() const
{
    return ui->charaSelcomboBox->currentData().toInt();
}

int MainWindow::selectedEntityIndex() const
{
    auto sel = ui->entitytableWidget->selectedItems();
    if (sel.isEmpty()) return -1;
    return ui->entitytableWidget->item(sel.first()->row(), 0)->data(Qt::UserRole).toInt();
}

bool MainWindow::hasEditingFocus() const
{
    // 检查当前焦点控件是否是可编辑的输入控件
    QWidget *w = QApplication::focusWidget();
    if (!w) return false;
    if (qobject_cast<QPlainTextEdit*>(w)){
        return true;
    }
    if (qobject_cast<QLineEdit*>(w)){
        return true;
    }
    if (qobject_cast<QSpinBox*>(w)){
        return true;
    }
    if (qobject_cast<QDoubleSpinBox*>(w)){
        return true;
    }
    // 向上遍历祖先链，检查是否属于某个 QComboBox（包括下拉列表 popup）
    // QComboBox 弹出下拉列表后，焦点转移到 popup 内部的 QComboBoxListView，
    // 此时 qobject_cast<QComboBox*>(w) 会失败，需要通过祖先链检测
    QWidget *p = w;
    while (p) {
        // QComboBox 自身或其下拉 popup 内的控件
        if (qobject_cast<QComboBox*>(p))
            return true;
        // 检查焦点是否在 QTableView / QTableWidget 的编辑器内部
        if (p == ui->charaStattableView || p == ui->charaResourcetableView || p == ui->charaWeapontableView
            || p == ui->missionResourcetableWidget || p == ui->missionWeapon)
            return true;
        p = p->parentWidget();
    }
    return false;
}

// ==================== UI 初始化 ====================
void MainWindow::setupUI()
{
    ui->filterProcessText->setPlaceholderText("输入进程名过滤...");
    setupEntityTable();
    setupCharacterStatTable();
    setupCharacterResourceTable();
    setupCharacterWeaponTable();
    setupMissionResourceTable();
    setupMissionWeaponTable();
    if (m_weaponNames.isEmpty())
        m_weaponNames = m_gameData->readAllWeaponNames();

    ui->entityTypecomboBox->addItem("全部", -1);
    ui->entityTypecomboBox->addItem("人类", 1);
    ui->entityTypecomboBox->addItem("僵尸", 2);
    ui->entityTypecomboBox->addItem("物品", 3);
    ui->entityTypecomboBox->addItem("抛射物", 4);
    ui->entityTypecomboBox->addItem("家具", 5);
    ui->entityTypecomboBox->addItem("拾取物", 6);
    ui->entityTypecomboBox->addItem("武器", 7);
    ui->entityTypecomboBox->addItem("车辆", 8);
    ui->entityTypecomboBox->addItem("特殊拾取", 9);
    ui->entityAreacomboBox->addItem("全部", -1);
    for (int i = 0; i < 16; ++i)
        ui->entityAreacomboBox->addItem(QString("区域%1").arg(i), i);

    setControlsEnabled(false);
    m_refreshTimer->setInterval(500);
}

void MainWindow::setControlsEnabled(bool enabled)
{
    ui->tabWidget->setEnabled(enabled);
    if (!enabled) {
        ui->processcomboBox->setEnabled(true);
        ui->refreshProcessBtn->setEnabled(true);
        ui->attachProceesBtn->setEnabled(true);
        ui->filterProcessText->setEnabled(true);
    }
}

void MainWindow::setupConnections()
{
    // 进程
    connect(ui->refreshProcessBtn, &QPushButton::clicked, this, &MainWindow::onRefreshProcess);
    connect(ui->attachProceesBtn, &QPushButton::clicked, this, &MainWindow::onAttachProcess);
    connect(ui->filterProcessText, &QPlainTextEdit::textChanged, this, [this]() {
        onFilterProcessChanged(ui->filterProcessText->toPlainText());
    });

    // 角色
    connect(ui->charaSelcomboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onCharacterSelected);
    connect(ui->charaNameplainTextEdit, &QPlainTextEdit::textChanged, this, [this]() {
        if (!m_updatingUI) onCharacterNameChanged();
    });
    connect(ui->charaPerkplainTextEdit, &QPlainTextEdit::textChanged, this, [this]() {
        if (!m_updatingUI) onCharacterPerkChanged();
    });
    connect(ui->charaTraitplainTextEdit, &QPlainTextEdit::textChanged, this, [this]() {
        if (!m_updatingUI) onCharacterTraitChanged();
    });
    connect(ui->charaDescplainTextEdit, &QPlainTextEdit::textChanged, this, [this]() {
        if (!m_updatingUI) onCharacterDescriptionChanged();
    });
    connect(ui->charaHpspinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onCharacterHpChanged);
    connect(ui->charaSpeeddoubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onCharacterSpeedChanged);

    // 属性表变化 - 监听 dataChanged 信号
    {
        QStandardItemModel *sm = static_cast<QStandardItemModel*>(ui->charaStattableView->model());
        connect(sm, &QStandardItemModel::dataChanged, this, &MainWindow::onCharacterStatChanged);
    }
    // 资源表变化
    {
        QStandardItemModel *rm = static_cast<QStandardItemModel*>(ui->charaResourcetableView->model());
        connect(rm, &QStandardItemModel::dataChanged, this, &MainWindow::onCharacterResourceChanged);
    }
    // 武器表锁定列变化
    {
        QStandardItemModel *wm = static_cast<QStandardItemModel*>(ui->charaWeapontableView->model());
        connect(wm, &QStandardItemModel::dataChanged, this, &MainWindow::onCharacterWeaponChanged);
    }

    // 状态复选框
    QList<QCheckBox*> statusBoxes = {
        ui->charaStatuscheckBox128, ui->charaStatuscheckBox064,
        ui->charaStatuscheckBox032, ui->charaStatuscheckBox016,
        ui->charaStatuscheckBox008, ui->charaStatuscheckBox004,
        ui->charaStatuscheckBox002, ui->charaStatuscheckBox001
    };
    for (auto *cb : statusBoxes)
        connect(cb, &QCheckBox::toggled, this, &MainWindow::onCharacterStatusToggled);

    // 实体过滤
    connect(ui->entityTypecomboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onEntityTypeFilterChanged);
    connect(ui->entityAreacomboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onEntityAreaFilterChanged);
    connect(ui->entitytableWidget, &QTableWidget::itemSelectionChanged,
            this, &MainWindow::onEntityTableSelectionChanged);

    // 实体标志
    QList<QCheckBox*> ef = {ui->noCollidecheckBox, ui->unSeencheckBox, ui->inVisiblecheckBox,
        ui->fadecheckBox, ui->noLightingcheckBox, ui->glowcheckBox,
        ui->noHitcheckBox, ui->noDamagecheckBox, ui->pausecheckBox};
    for (auto *cb : ef)
        connect(cb, &QCheckBox::toggled, this, &MainWindow::onEntityFlagToggled);

    // 实体坐标/速度/物理
    for (auto *sb : {ui->posXdoubleSpinBox, ui->posYdoubleSpinBox, ui->posZdoubleSpinBox})
        connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onEntityPosChanged);
    for (auto *sb : {ui->velXdoubleSpinBox, ui->velYdoubleSpinBox, ui->velZdoubleSpinBox})
        connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onEntityVelChanged);
    for (auto *sb : {ui->massdoubleSpinBox, ui->frictiondoubleSpinBox, ui->bounceFrictiondoubleSpinBox})
        connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onEntityPhysicsChanged);

    connect(ui->hitpointsspinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onEntityHitpointsChanged);
    connect(ui->aiStatespinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onEntityAiStateChanged);
    connect(ui->aiWaitspinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onEntityAiWaitChanged);

    // 实体操作按钮
    connect(ui->setTargetpushButton, &QPushButton::clicked, this, &MainWindow::onSetTargetEntity);
    connect(ui->teleportTargetpushButton, &QPushButton::clicked, this, &MainWindow::onTeleportToTarget);
    connect(ui->swapTargetpushButton, &QPushButton::clicked, this, &MainWindow::onSwapEntityPositions);

    // 全局资源
    connect(ui->missionResourcetableWidget, &QTableWidget::cellChanged, this, &MainWindow::onMissionResourceChanged);

    // 定时刷新
    connect(m_refreshTimer, &QTimer::timeout, this, &MainWindow::onRefreshTimer);

    // 附加/分离
    connect(m_memMgr, &MemoryManager::processAttached, this, [this]() {
        uintptr_t base = m_memMgr->moduleBaseAddress();
        m_gameData->setModuleBase(base);
        statusBar()->showMessage(QString("已附加: %1 (PID: %2) 模块基址: 0x%3")
            .arg(m_memMgr->attachedProcessName(),m_memMgr->attachedProcessId())
            .arg(base, 0, 16));
        refreshAll();
        m_refreshTimer->start();
        setControlsEnabled(true);
    });
    connect(m_memMgr, &MemoryManager::processDetached, this, [this]() {
        statusBar()->showMessage("已分离");
        setControlsEnabled(false);
        m_refreshTimer->stop();
    });
    connect(m_memMgr, &MemoryManager::attachError, this, [this](const QString &err) {
        statusBar()->showMessage("错误: " + err);
        QMessageBox::warning(this, "错误", err);
    });
}

// ==================== 表格初始化 ====================
void MainWindow::setupEntityTable()
{
    QTableWidget *t = ui->entitytableWidget;
    t->setColumnCount(5);
    t->setHorizontalHeaderLabels({"ID", "类型", "子类型", "区域", "地址"});
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->horizontalHeader()->setStretchLastSection(true);
}

void MainWindow::setupCharacterStatTable()
{
    // QTableView: 5列: 属性名, 基础值, 附加值, 有效值, 是否已知
    QStandardItemModel *m = new QStandardItemModel(13, 5, this);
    m->setHorizontalHeaderLabels({"属性", "基础值", "附加值", "有效值", "是否已知"});

    for (int i = 0; i < 13; ++i) {
        QStandardItem *nameItem = new QStandardItem(statNames[i]);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        m->setItem(i, 0, nameItem);                  // 属性名 Label
        m->setItem(i, 1, new QStandardItem("0"));     // 基础值 SpinBox
        m->setItem(i, 2, new QStandardItem("0"));     // 附加值 SpinBox
        QStandardItem *effItem = new QStandardItem("0");
        effItem->setFlags(effItem->flags() & ~Qt::ItemIsEditable);
        m->setItem(i, 3, effItem);                    // 有效值 Label
        QStandardItem *dispItem = new QStandardItem();
        dispItem->setFlags(dispItem->flags() | Qt::ItemIsUserCheckable);
        dispItem->setCheckable(true);
        m->setItem(i, 4, dispItem);                   // 是否已知 CheckBox
    }

    ui->charaStattableView->setModel(m);
    ui->charaStattableView->horizontalHeader()->setStretchLastSection(true);
    // 设置 SpinBox 委托
    ui->charaStattableView->setItemDelegateForColumn(1, new SpinBoxDelegate(-128, 127, this));
    ui->charaStattableView->setItemDelegateForColumn(2, new SpinBoxDelegate(-128, 127, this));
}

void MainWindow::setupCharacterResourceTable()
{
    // QTableView: 2列: 资源名(Label), 数量(SpinBox)
    QStandardItemModel *m = new QStandardItemModel(7, 2, this);
    m->setHorizontalHeaderLabels({"资源", "数量"});
    for (int i = 0; i < 7; ++i) {
        QStandardItem *nameItem = new QStandardItem(resourceNames[i]);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        m->setItem(i, 0, nameItem);
        m->setItem(i, 1, new QStandardItem("0"));
    }
    ui->charaResourcetableView->setModel(m);
    ui->charaResourcetableView->horizontalHeader()->setStretchLastSection(true);
    ui->charaResourcetableView->setItemDelegateForColumn(1, new SpinBoxDelegate(-999999, 999999, this));
}

void MainWindow::setupCharacterWeaponTable()
{
    // QTableView: 3列: 武器名(Button), 数量(SpinBox), 锁定(CheckBox)
    QStandardItemModel *m = new QStandardItemModel(3, 3, this);
    m->setHorizontalHeaderLabels({"武器", "数量", "锁定"});
    ui->charaWeapontableView->setModel(m);
    for (int i = 0; i < 3; ++i) {
        // 武器名 - Button
        //m->setItem(i, 0, new QStandardItem("(空)")); //onWeaponButtonClicked(-1, i);
        QPushButton *btn = new QPushButton("(空)", this);
        connect(btn, &QPushButton::clicked, this, [this, &i](){
            onWeaponButtonClicked(-1, i);
        });
        ui->charaWeapontableView->setIndexWidget(m->index(i, 0), btn);
        // 数量 - SpinBox
        m->setItem(i, 1, new QStandardItem("0"));
        // 锁定 - CheckBox
        QStandardItem *lockItem = new QStandardItem();
        lockItem->setFlags(lockItem->flags() | Qt::ItemIsUserCheckable);
        lockItem->setCheckable(true);
        m->setItem(i, 2, lockItem);
    }
    ui->charaWeapontableView->horizontalHeader()->setStretchLastSection(true);
    ui->charaWeapontableView->setItemDelegateForColumn(1, new SpinBoxDelegate(0, 999, this));
}

void MainWindow::setupMissionResourceTable()
{
    QTableWidget *t = ui->missionResourcetableWidget;
    t->setColumnCount(2);
    t->setHorizontalHeaderLabels({"资源", "数量"});
    t->setRowCount(7);
    for (int i = 0; i < 7; ++i) {
        t->setItem(i, 0, new QTableWidgetItem(resourceNames[i]));
        QTableWidgetItem *v = new QTableWidgetItem("0");
        v->setData(Qt::UserRole, i);
        t->setItem(i, 1, v);
    }
    t->horizontalHeader()->setStretchLastSection(true);
}

void MainWindow::setupMissionWeaponTable()
{
    // Storage_slots[15]: 3行5列, 每列 武器名(Button)+数量(SpinBox)
    QGridLayout *grid = qobject_cast<QGridLayout*>(ui->missionWeapon->layout());
    if (!grid) {
        grid = new QGridLayout(ui->missionWeapon);
        ui->missionWeapon->setLayout(grid);
    }

    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 5; ++col) {
            int idx = row * 5 + col;
            QWidget *cell = new QWidget(this);
            QHBoxLayout *hbox = new QHBoxLayout(cell);
            hbox->setContentsMargins(1, 1, 1, 1);

            QPushButton *btn = new QPushButton("(空)", this);
            btn->setProperty("slotIndex", idx);
            connect(btn, &QPushButton::clicked, this, [this, idx]() {
                onStorageWeaponClicked(idx);
            });
            hbox->addWidget(btn);

            QSpinBox *sb = new QSpinBox(this);
            sb->setRange(0, 999);
            sb->setProperty("slotIndex", idx);
            // 连接 storage stack 变化
            connect(sb, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, idx](int) {
                if (!m_updatingUI)
                    writeMissionStorageStack(idx);
            });
            hbox->addWidget(sb);

            grid->addWidget(cell, row, col);
        }
    }
}

// ==================== 进程 ====================
void MainWindow::onRefreshProcess() { refreshProcessList(); }

void MainWindow::onFilterProcessChanged(const QString &text)
{
    Q_UNUSED(text);
    refreshProcessList();
}

void MainWindow::onAttachProcess()
{
    int idx = ui->processcomboBox->currentIndex();
    if (idx < 0) {
        QMessageBox::information(this, "提示", "请选择进程");
        return;
    }
    uint32_t pid = ui->processcomboBox->currentData().toUInt();
    m_memMgr->attachProcessById(pid);
}

void MainWindow::refreshProcessList()
{
    QString filter = ui->filterProcessText->toPlainText().trimmed();
    QString cur = ui->processcomboBox->currentText();
    ui->processcomboBox->clear();
    for (const auto &p : m_memMgr->enumerateProcesses()) {
        QString name = p["name"].toString();
        uint32_t pid = p["pid"].toUInt();
        if (!filter.isEmpty() && !name.contains(filter, Qt::CaseInsensitive)
            && !QString::number(pid).contains(filter))
            continue;
        ui->processcomboBox->addItem(name + " (PID:" + QString::number(pid) + ")", pid);
    }
    int idx = ui->processcomboBox->findText(cur);
    if (idx >= 0) ui->processcomboBox->setCurrentIndex(idx);
}

// ==================== 角色 ====================
void MainWindow::onCharacterSelected(int)
{
    int idx = selectedCharacterIndex();
    if (idx >= 0 && isAttached()) {
        refreshCharacterData(idx);
    }
}

void MainWindow::onCharacterNameChanged()
{ int i = selectedCharacterIndex(); if (i >= 0 && !m_updatingUI) writeCharacterName(i); }

void MainWindow::onCharacterPerkChanged()
{ int i = selectedCharacterIndex(); if (i >= 0 && !m_updatingUI) writeCharacterPerk(i); }

void MainWindow::onCharacterTraitChanged()
{ int i = selectedCharacterIndex(); if (i >= 0 && !m_updatingUI) writeCharacterTrait(i); }

void MainWindow::onCharacterDescriptionChanged()
{ int i = selectedCharacterIndex(); if (i >= 0 && !m_updatingUI) writeCharacterDescription(i); }

void MainWindow::onCharacterHpChanged(int)
{ int i = selectedCharacterIndex(); if (i >= 0 && !m_updatingUI) writeCharacterHp(i); }

void MainWindow::onCharacterSpeedChanged(double)
{ int i = selectedCharacterIndex(); if (i >= 0 && !m_updatingUI) writeCharacterSpeed(i); }

void MainWindow::onCharacterStatusToggled()
{ int i = selectedCharacterIndex(); if (i >= 0 && !m_updatingUI) writeCharacterStatus(i); }

void MainWindow::onCharacterStatChanged(const QModelIndex &topLeft, const QModelIndex &)
{
    if (m_updatingUI) return;
    int i = selectedCharacterIndex();
    if (i < 0) return;
    // 只处理基础值和附加值列的变化 (col 1,2)
    int col = topLeft.column();
    int row = topLeft.row();
    if (col == 1 || col == 2) {
        // 先更新缓存再写入
        QStandardItemModel *sm = static_cast<QStandardItemModel*>(ui->charaStattableView->model());
        bool ok;
        int8_t baseVal = static_cast<int8_t>(sm->item(row, 1)->text().toInt(&ok));
        if (!ok) return;
        int8_t bonusVal = static_cast<int8_t>(sm->item(row, 2)->text().toInt(&ok));
        if (!ok) return;
        if (m_gameData->writeCharacterStat(i, row, baseVal, bonusVal)) {
            m_charCache[i].base_stats[row] = baseVal;
            m_charCache[i].bonus_stats[row] = bonusVal;
            // 更新有效值
            int effective = baseVal + bonusVal;
            m_updatingUI = true;
            sm->item(row, 3)->setText(QString::number(effective));
            m_updatingUI = false;
        }
    } else if (col == 4) {
        // 是否已知列
        int i = selectedCharacterIndex();
        if (i < 0) return;
        QStandardItemModel *sm = static_cast<QStandardItemModel*>(ui->charaStattableView->model());
        bool checked = sm->item(row, 4)->checkState() == Qt::Checked;
        m_charCache[i].display_stat[row] = checked ? 1 : 0;
        // 通过 writeCharacter 写入 display_stat
        uintptr_t addr = m_gameData->calcCharacterAddress(i);
        if (addr && m_memMgr->isAttached()) {
            m_memMgr->write<int8_t>(addr + 0x1BC + row, checked ? 1 : 0);
        }
    }
}

void MainWindow::onCharacterResourceChanged(const QModelIndex &topLeft, const QModelIndex &)
{
    if (m_updatingUI) return;
    int i = selectedCharacterIndex();
    if (i < 0) return;
    int col = topLeft.column();
    int row = topLeft.row();
    if (col == 1) {
        writeCharacterResources(i);
    }
}

void MainWindow::onCharacterWeaponChanged(const QModelIndex &topLeft, const QModelIndex &)
{
    if (m_updatingUI) return;
    int i = selectedCharacterIndex();
    if (i < 0) return;
    int col = topLeft.column();
    int row = topLeft.row();
    switch (col)
    {
        case 1:
        case 2:
        writeCharacterWeaponSlot(i, row);
            break;
        
        default:
            break;
    }
}

void MainWindow::refreshCharacterData(int ci)
{
    if (!isAttached() || ci < 0 || ci >= m_charCache.size()) return;
    // 如果用户正在编辑输入控件，跳过刷新
    if (hasEditingFocus()) return;

    const auto &ch = m_charCache[ci];
    m_updatingUI = true;

    ui->charaThingplainTextEdit->setPlainText(QString::number(ch.cur_thingid));
    ui->charaNameplainTextEdit->setPlainText(ch.name);
    ui->charaPerkplainTextEdit->setPlainText(ch.perk);
    ui->charaTraitplainTextEdit->setPlainText(ch.trait);
    ui->charaDescplainTextEdit->setPlainText(ch.description);
    ui->charaHpspinBox->setValue(ch.health);
    ui->charaSpeeddoubleSpinBox->setValue(ch.speed_bonus);

    // 状态
    ui->charaStatuscheckBox128->setChecked(ch.status & STATUS_EXPLORER);
    ui->charaStatuscheckBox064->setChecked(ch.status & STATUS_DOGPAL);
    ui->charaStatuscheckBox032->setChecked(ch.status & STATUS_COFFEE);
    ui->charaStatuscheckBox016->setChecked(ch.status & STATUS_DOGMALUS);
    ui->charaStatuscheckBox008->setChecked(ch.status & STATUS_SUPERDOG);
    ui->charaStatuscheckBox004->setChecked(ch.status & STATUS_TIRED);
    ui->charaStatuscheckBox002->setChecked(ch.status & STATUS_INJURED);
    ui->charaStatuscheckBox001->setChecked(ch.status & STATUS_SICK);

    // 属性表
    QStandardItemModel *statM = static_cast<QStandardItemModel*>(ui->charaStattableView->model());
    for (int i = 0; i < 13; ++i) {
        statM->item(i, 1)->setText(QString::number(ch.base_stats[i]));
        statM->item(i, 2)->setText(QString::number(ch.bonus_stats[i]));
        int effective = ch.base_stats[i] + ch.bonus_stats[i];
        statM->item(i, 3)->setText(QString::number(effective));
        statM->item(i, 4)->setCheckState(ch.display_stat[i] ? Qt::Checked : Qt::Unchecked);
    }

    // 资源表
    QStandardItemModel *resM = static_cast<QStandardItemModel*>(ui->charaResourcetableView->model());
    for (int i = 0; i < 7; ++i)
        resM->item(i, 1)->setText(QString::number(ch.resource[i]));

    // 武器表
    QStandardItemModel *wpM = static_cast<QStandardItemModel*>(ui->charaWeapontableView->model());
    for (int i = 0; i < 3; ++i) {
        int wid = ch.weapon_id[i];
        QPushButton *btn = qobject_cast<QPushButton*>(ui->charaWeapontableView->indexWidget(wpM->index(i, 0)));
        if (wid > 0 && wid < m_weaponNames.size()){
            btn->setText(m_weaponNames[wid]);
        }
        else
            btn->setText("(空)");
        wpM->item(i, 1)->setText(QString::number(ch.weapon_stack[i]));
        wpM->item(i, 2)->setCheckState(ch.weapon_lock[i] ? Qt::Checked : Qt::Unchecked);
    }

    m_updatingUI = false;
}

// ==================== 写入角色 ====================
void MainWindow::writeCharacterName(int i)
{ QString s = ui->charaNameplainTextEdit->toPlainText().trimmed(); if (m_gameData->writeCharacterName(i, s)) m_charCache[i].name = s; }

void MainWindow::writeCharacterPerk(int i)
{ QString s = ui->charaPerkplainTextEdit->toPlainText().trimmed(); if (m_gameData->writeCharacterPerk(i, s)) m_charCache[i].perk = s; }

void MainWindow::writeCharacterTrait(int i)
{ QString s = ui->charaTraitplainTextEdit->toPlainText().trimmed(); if (m_gameData->writeCharacterTrait(i, s)) m_charCache[i].trait = s; }

void MainWindow::writeCharacterDescription(int i)
{ QString s = ui->charaDescplainTextEdit->toPlainText().trimmed(); if (m_gameData->writeCharacterDescription(i, s)) m_charCache[i].description = s; }

void MainWindow::writeCharacterHp(int i)
{ int32_t v = ui->charaHpspinBox->value(); if (m_gameData->writeCharacterHealth(i, v)) m_charCache[i].health = v; }

void MainWindow::writeCharacterSpeed(int i)
{ float v = static_cast<float>(ui->charaSpeeddoubleSpinBox->value()); if (m_gameData->writeCharacterSpeedBonus(i, v)) m_charCache[i].speed_bonus = v; }

void MainWindow::writeCharacterStatus(int i)
{
    uint8_t s = 0;
    if (ui->charaStatuscheckBox001->isChecked()) s |= STATUS_SICK;
    if (ui->charaStatuscheckBox002->isChecked()) s |= STATUS_INJURED;
    if (ui->charaStatuscheckBox004->isChecked()) s |= STATUS_TIRED;
    if (ui->charaStatuscheckBox008->isChecked()) s |= STATUS_SUPERDOG;
    if (ui->charaStatuscheckBox016->isChecked()) s |= STATUS_DOGMALUS;
    if (ui->charaStatuscheckBox032->isChecked()) s |= STATUS_COFFEE;
    if (ui->charaStatuscheckBox064->isChecked()) s |= STATUS_DOGPAL;
    if (ui->charaStatuscheckBox128->isChecked()) s |= STATUS_EXPLORER;
    if (m_gameData->writeCharacterStatus(i, s)) m_charCache[i].status = s;
}

void MainWindow::writeCharacterStats(int i)
{
    if (i < 0) return;
    QStandardItemModel *sm = static_cast<QStandardItemModel*>(ui->charaStattableView->model());
    for (int row = 0; row < 13; ++row) {
        bool ok;
        int8_t baseVal = static_cast<int8_t>(sm->item(row, 1)->text().toInt(&ok));
        if (!ok) baseVal = m_charCache[i].base_stats[row];
        int8_t bonusVal = static_cast<int8_t>(sm->item(row, 2)->text().toInt(&ok));
        if (!ok) bonusVal = m_charCache[i].bonus_stats[row];
        m_gameData->writeCharacterStat(i, row, baseVal, bonusVal);
        m_charCache[i].base_stats[row] = baseVal;
        m_charCache[i].bonus_stats[row] = bonusVal;
    }
}

void MainWindow::writeCharacterResources(int i)
{
    if (i < 0) return;
    QStandardItemModel *rm = static_cast<QStandardItemModel*>(ui->charaResourcetableView->model());
    for (int row = 0; row < 7; ++row) {
        bool ok;
        int32_t val = rm->item(row, 1)->text().toInt(&ok);
        if (ok && m_gameData->writeCharacterResource(i, row, val))
            m_charCache[i].resource[row] = val;
    }
}

void MainWindow::writeCharacterWeaponSlot(int charIndex, int slot)
{
    if (charIndex < 0 || slot < 0 || slot >= 3) return;
    QStandardItemModel *wm = static_cast<QStandardItemModel*>(ui->charaWeapontableView->model());
    bool ok;
    int32_t stack = wm->item(slot, 1)->text().toInt(&ok);
    if (!ok) stack = m_charCache[charIndex].weapon_stack[slot];
    int32_t id = m_charCache[charIndex].weapon_id[slot];
    int32_t lock = (wm->item(slot, 2)->checkState() == Qt::Checked) ? 1 : 0;

    if (m_gameData->writeCharacterWeapon(charIndex, slot, id, stack, lock)) {
        m_charCache[charIndex].weapon_stack[slot] = stack;
        m_charCache[charIndex].weapon_lock[slot] = lock;
    }
}

// ==================== 武器按钮 ====================
void MainWindow::onWeaponButtonClicked(int charIndex, int slot)
{
    if (!isAttached()) return;
    if (charIndex == -1){
        charIndex = selectedCharacterIndex();
    }
    WeaponDialog dlg(m_weaponNames, this);
    if (dlg.exec() == QDialog::Accepted) {
        int wid = dlg.selectedWeaponIndex();
        if (wid >= 0) {
            int32_t stack = m_charCache[charIndex].weapon_stack[slot];
            int32_t lock = m_charCache[charIndex].weapon_lock[slot];
            m_gameData->writeCharacterWeapon(charIndex, slot, wid, stack, lock);
            m_charCache[charIndex].weapon_id[slot] = wid;
            QStandardItemModel *m = static_cast<QStandardItemModel*>(ui->charaWeapontableView->model());
            QPushButton* btn = static_cast<QPushButton*>(ui->charaWeapontableView->indexWidget(m->index(slot, 0)));
            btn->setText(m_weaponNames.value(wid, "(空)"));
        }
    }
}

void MainWindow::onStorageWeaponClicked(int slotIndex)
{
    if (!isAttached()) return;
    WeaponDialog dlg(m_weaponNames, this);
    if (dlg.exec() == QDialog::Accepted) {
        int wid = dlg.selectedWeaponIndex();
        writeMissionStorageWeapon(slotIndex);
        // 更新id
        uintptr_t addr = m_memMgr->moduleBaseAddress() + 0x5E2238 + 0x48 + slotIndex * 8;
        m_memMgr->write<int32_t>(addr, wid);
        m_missionCache.storage_id[slotIndex] = wid;
        // 更新按钮文字
        QGridLayout *grid = qobject_cast<QGridLayout*>(ui->missionWeapon->layout());
        if (grid) {
            int r = slotIndex / 5, c = slotIndex % 5;
            QLayoutItem *li = grid->itemAtPosition(r, c);
            if (li) {
                QWidget *cell = li->widget();
                if (cell) {
                    QPushButton *btn = cell->findChild<QPushButton*>();
                    if (btn) btn->setText(m_weaponNames.value(wid, "(空)"));
                }
            }
        }
    }
}

void MainWindow::writeMissionStorageWeapon(int slotIndex)
{
    if (!isAttached()) return;
    // 先写入当前的 stack
    writeMissionStorageStack(slotIndex);
}

void MainWindow::writeMissionStorageStack(int slotIndex)
{
    if (!isAttached()) return;
    QGridLayout *grid = qobject_cast<QGridLayout*>(ui->missionWeapon->layout());
    if (!grid) return;
    int r = slotIndex / 5, c = slotIndex % 5;
    QLayoutItem *li = grid->itemAtPosition(r, c);
    if (!li) return;
    QWidget *cell = li->widget();
    if (!cell) return;
    QSpinBox *sb = cell->findChild<QSpinBox*>();
    if (!sb) return;

    uintptr_t addr = m_memMgr->moduleBaseAddress() + 0x5E2238 + 0x48 + slotIndex * 8;
    // 写入 stack
    int32_t stackVal = sb->value();
    m_memMgr->write<int32_t>(addr + 0x04, stackVal);
    m_missionCache.storage_stack[slotIndex] = stackVal;
}

// ==================== 实体 ====================
void MainWindow::onEntityTypeFilterChanged(int idx)
{ m_entityTypeFilter = ui->entityTypecomboBox->itemData(idx).toInt(); refreshEntityList(); }

void MainWindow::onEntityAreaFilterChanged(int idx)
{ m_entityAreaFilter = ui->entityAreacomboBox->itemData(idx).toInt(); refreshEntityList(); }

void MainWindow::onEntityTableSelectionChanged()
{ int e = selectedEntityIndex(); if (e >= 0) refreshEntityData(e); }

void MainWindow::onEntityFlagToggled()
{ int e = selectedEntityIndex(); if (e >= 0 && !m_updatingUI) writeEntityFlag(); }

void MainWindow::onEntityPosChanged()
{ int e = selectedEntityIndex(); if (e >= 0 && !m_updatingUI) writeEntityPos(); }

void MainWindow::onEntityVelChanged()
{ int e = selectedEntityIndex(); if (e >= 0 && !m_updatingUI) writeEntityVel(); }

void MainWindow::onEntityPhysicsChanged()
{ int e = selectedEntityIndex(); if (e >= 0 && !m_updatingUI) writeEntityPhysics(); }

void MainWindow::onEntityHitpointsChanged(int)
{ int e = selectedEntityIndex(); if (e >= 0 && !m_updatingUI) writeEntityHitpoints(); }

void MainWindow::onEntityAiStateChanged(int)
{ int e = selectedEntityIndex(); if (e >= 0 && !m_updatingUI) writeEntityAiState(); }

void MainWindow::onEntityAiWaitChanged(int)
{ int e = selectedEntityIndex(); if (e >= 0 && !m_updatingUI) writeEntityAiWait(); }

void MainWindow::refreshEntityList()
{
    if (!isAttached()) return;
    int selIdx = selectedEntityIndex();
    // 保存滚动条位置
    int scrollPos = 0;
    QTableWidget *t = ui->entitytableWidget;
    if (t->verticalScrollBar())
        scrollPos = t->verticalScrollBar()->value();

    m_thingCache = m_gameData->readAllThings();
    t->setRowCount(0);

    auto typeName = [](uint8_t ty) -> QString {
        switch (ty) { case 1: return "人类"; case 2: return "僵尸"; case 3: return "物品"; case 4: return "抛射物"; default: return QString("类型%1").arg(ty); }
    };
    auto subName = [](uint8_t st) -> QString {
        switch (st) { case 0: return "家具"; case 1: return "拾取物"; case 2: return "武器"; case 3: return "车辆"; case 4: return "特殊拾取"; default: return QString("子类型%1").arg(st); }
    };

    uintptr_t baseAddr = m_memMgr->moduleBaseAddress() + 0x5632E0;

    int row = 0;
    for (int i = 0; i < m_thingCache.size(); ++i) {
        const auto &th = m_thingCache[i];
        if (th.id == 0) continue;
        else if (m_entityAreaFilter >= 0 && th.mapid != m_entityAreaFilter) continue;
        else if ((m_entityTypeFilter >= 0 && m_entityTypeFilter <= 4 && th.type != m_entityTypeFilter) ||
                    (m_entityTypeFilter >= 5 && (th.type != 3 || th.subtype + 5 != m_entityTypeFilter))) continue;
        

        t->insertRow(row);
        QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(th.id));
        idItem->setData(Qt::UserRole, i);
        t->setItem(row, 0, idItem);
        t->setItem(row, 1, new QTableWidgetItem(typeName(th.type)));
        if (th.type == 3)
        {
            t->setItem(row, 2, new QTableWidgetItem(subName(th.subtype)));
        }else{
            t->setItem(row, 2, new QTableWidgetItem(typeName(th.type)));
        }
        t->setItem(row, 3, new QTableWidgetItem(QString::number(th.mapid)));
        // 实体地址 = 主模块基址 + 0x5632E0 + 索引 * 0x304
        uintptr_t addr = baseAddr + i * 0x304;
        t->setItem(row, 4, new QTableWidgetItem("0x" + QString::number(addr, 16).toUpper()));
        ++row;
    }

    ui->entityCountLabel->setText(QString("实体: %1").arg(row));
    // 恢复选择
    if (selIdx >= 0)
        for (int r = 0; r < t->rowCount(); ++r)
            if (t->item(r, 0)->data(Qt::UserRole).toInt() == selIdx)
            { t->selectRow(r); break; }
    // 恢复滚动条位置
    if (t->verticalScrollBar())
        t->verticalScrollBar()->setValue(scrollPos);
}

void MainWindow::refreshEntityData(int ei)
{
    if (ei < 0 || ei >= m_thingCache.size()) return;
    const auto &th = m_thingCache[ei];
    m_updatingUI = true;
    ui->noCollidecheckBox->setChecked(th.nocollide);
    ui->unSeencheckBox->setChecked(th.unseen);
    ui->inVisiblecheckBox->setChecked(th.invisible);
    ui->fadecheckBox->setChecked(th.fade);
    ui->noLightingcheckBox->setChecked(th.no_lighting);
    ui->glowcheckBox->setChecked(th.glow);
    ui->noHitcheckBox->setChecked(th.no_hit);
    ui->noDamagecheckBox->setChecked(th.no_do_damage);
    ui->pausecheckBox->setChecked(th.pause);

    ui->posXdoubleSpinBox->setValue(th.pos[0]);
    ui->posYdoubleSpinBox->setValue(th.pos[1]);
    ui->posZdoubleSpinBox->setValue(th.pos[2]);
    ui->velXdoubleSpinBox->setValue(th.vel[0]);
    ui->velYdoubleSpinBox->setValue(th.vel[1]);
    ui->velZdoubleSpinBox->setValue(th.vel[2]);
    ui->massdoubleSpinBox->setValue(th.phy[0]);
    ui->frictiondoubleSpinBox->setValue(th.phy[1]);
    ui->bounceFrictiondoubleSpinBox->setValue(th.phy[2]);
    ui->hitpointsspinBox->setValue(th.hitpoints);
    ui->aiStatespinBox->setValue(th.ai_state);
    ui->aiWaitspinBox->setValue(th.ai_wait);
    m_updatingUI = false;
}

// ==================== 写入实体 ====================
void MainWindow::writeEntityFlag()
{
    int idx = selectedEntityIndex(); if (idx < 0) return;
    ThingData d = m_thingCache[idx];
    d.nocollide = ui->noCollidecheckBox->isChecked() ? 1 : 0;
    d.unseen = ui->unSeencheckBox->isChecked() ? 1 : 0;
    d.invisible = ui->inVisiblecheckBox->isChecked() ? 1 : 0;
    d.fade = ui->fadecheckBox->isChecked() ? 1 : 0;
    d.no_lighting = ui->noLightingcheckBox->isChecked() ? 1 : 0;
    d.glow = ui->glowcheckBox->isChecked() ? 1 : 0;
    d.no_hit = ui->noHitcheckBox->isChecked() ? 1 : 0;
    d.no_do_damage = ui->noDamagecheckBox->isChecked() ? 1 : 0;
    d.pause = ui->pausecheckBox->isChecked() ? 1 : 0;
    if (m_gameData->writeThing(idx, d)) m_thingCache[idx] = d;
}

void MainWindow::writeEntityPos()
{
    int idx = selectedEntityIndex(); if (idx < 0) return;
    ThingData d = m_thingCache[idx];
    d.pos[0] = static_cast<float>(ui->posXdoubleSpinBox->value());
    d.pos[1] = static_cast<float>(ui->posYdoubleSpinBox->value());
    d.pos[2] = static_cast<float>(ui->posZdoubleSpinBox->value());
    if (m_gameData->writeThing(idx, d)) {
        m_thingCache[idx].pos[0] = d.pos[0];
        m_thingCache[idx].pos[1] = d.pos[1];
        m_thingCache[idx].pos[2] = d.pos[2];
    }
}

void MainWindow::writeEntityVel()
{
    int idx = selectedEntityIndex(); if (idx < 0) return;
    ThingData d = m_thingCache[idx];
    d.vel[0] = static_cast<float>(ui->velXdoubleSpinBox->value());
    d.vel[1] = static_cast<float>(ui->velYdoubleSpinBox->value());
    d.vel[2] = static_cast<float>(ui->velZdoubleSpinBox->value());
    if (m_gameData->writeThing(idx, d)) { m_thingCache[idx].vel[0] = d.vel[0]; m_thingCache[idx].vel[1] = d.vel[1]; m_thingCache[idx].vel[2] = d.vel[2]; }
}

void MainWindow::writeEntityPhysics()
{
    int idx = selectedEntityIndex(); if (idx < 0) return;
    ThingData d = m_thingCache[idx];
    d.phy[0] = static_cast<float>(ui->massdoubleSpinBox->value());
    d.phy[1] = static_cast<float>(ui->frictiondoubleSpinBox->value());
    d.phy[2] = static_cast<float>(ui->bounceFrictiondoubleSpinBox->value());
    if (m_gameData->writeThing(idx, d)) { m_thingCache[idx].phy[0] = d.phy[0]; m_thingCache[idx].phy[1] = d.phy[1]; m_thingCache[idx].phy[2] = d.phy[2]; }
}

void MainWindow::writeEntityHitpoints()
{
    int idx = selectedEntityIndex(); if (idx < 0) return;
    int32_t hp = ui->hitpointsspinBox->value();
    uintptr_t addr = m_gameData->calcThingAddress(idx);
    if (addr && m_memMgr->write<int32_t>(addr + 0x254, hp))
        m_thingCache[idx].hitpoints = hp;
}

void MainWindow::writeEntityAiState()
{
    int idx = selectedEntityIndex(); if (idx < 0) return;
    uint32_t v = ui->aiStatespinBox->value();
    uintptr_t addr = m_gameData->calcThingAddress(idx);
    if (addr && m_memMgr->write<uint32_t>(addr + 0x288, v))
        m_thingCache[idx].ai_state = v;
}

void MainWindow::writeEntityAiWait()
{
    int idx = selectedEntityIndex(); if (idx < 0) return;
    uint32_t v = ui->aiWaitspinBox->value();
    uintptr_t addr = m_gameData->calcThingAddress(idx);
    if (addr && m_memMgr->write<uint32_t>(addr + 0x2A8, v))
        m_thingCache[idx].ai_wait = v;
}

// ==================== 实体操作 ====================
void MainWindow::onSetTargetEntity()
{
    int e = selectedEntityIndex();
    if (e >= 0) {
        m_targetEntityIndex = e;
        statusBar()->showMessage(QString("目标: ID=%1, 索引=%2").arg(m_thingCache[e].id, e));
        ui->setTargetpushButton->setStyleSheet("background-color:lightgreen;");
    }
}

void MainWindow::onTeleportToTarget()
{
    int cur = selectedEntityIndex();
    if (cur < 0 || m_targetEntityIndex < 0) {
        QMessageBox::information(this, "提示", "请先选择实体并设置目标");
        return;
    }
    if (!isAttached()) return;

    const auto &target = m_thingCache[m_targetEntityIndex];
    ThingData d = m_thingCache[cur];
    // 复制坐标和mapid
    d.pos[0] = target.pos[0]; d.pos[1] = target.pos[1]; d.pos[2] = target.pos[2];
    d.mapid = target.mapid;

    if (m_gameData->writeThing(cur, d)) {
        m_thingCache[cur] = d;
        refreshEntityData(cur);
        statusBar()->showMessage(QString("已传送至目标"));
    }
}

void MainWindow::onSwapEntityPositions()
{
    int cur = selectedEntityIndex();
    if (cur < 0 || m_targetEntityIndex < 0) {
        QMessageBox::information(this, "提示", "请先选择实体并设置目标");
        return;
    }
    if (!isAttached()) return;

    ThingData d1 = m_thingCache[cur];
    ThingData d2 = m_thingCache[m_targetEntityIndex];

    // 交换坐标+mapid
    for (int i = 0; i < 3; ++i) std::swap(d1.pos[i], d2.pos[i]);
    std::swap(d1.mapid, d2.mapid);

    bool ok1 = m_gameData->writeThing(cur, d1);
    bool ok2 = m_gameData->writeThing(m_targetEntityIndex, d2);
    if (ok1 && ok2) {
        m_thingCache[cur] = d1;
        m_thingCache[m_targetEntityIndex] = d2;
        refreshEntityData(cur);
        statusBar()->showMessage("已交换位置");
    }
}

// ==================== 全局 ====================
void MainWindow::onMissionResourceChanged(int, int)
{ if (!m_updatingUI) writeMissionResource(); }

void MainWindow::onMissionStorageStackChanged(int row, int col)
{
    Q_UNUSED(row);
    Q_UNUSED(col);
    // 这个由 setupMissionWeaponTable 中的 connect 直接调用 writeMissionStorageStack
    // 这里不做额外处理
}

void MainWindow::writeMissionResource()
{
    for (int i = 0; i < 7; ++i) {
        auto *item = ui->missionResourcetableWidget->item(i, 1);
        if (!item) continue;
        bool ok; int32_t v = item->text().toInt(&ok);
        if (ok && m_gameData->writeMissionResource(i, v))
            m_missionCache.resource[i] = v;
    }
}

// ==================== 定时刷新 ====================
void MainWindow::onRefreshTimerChara(){
    // 如果用户正在编辑控件，完全跳过角色页刷新
    if (hasEditingFocus()) return;

    m_charCache = m_gameData->readAllCharacters();
    m_updatingUI = true;
    int curCharIdx = selectedCharacterIndex();
    ui->charaSelcomboBox->blockSignals(true);
    ui->charaSelcomboBox->clear();
    for (int i = 0; i < m_charCache.size(); ++i){
        if (!m_charCache[i].name.isEmpty()){
            ui->charaSelcomboBox->addItem(m_charCache[i].name, i);
        }
    }
    for (int i = 0; i < ui->charaSelcomboBox->count(); ++i)
        if (ui->charaSelcomboBox->itemData(i).toInt() == curCharIdx)
        { ui->charaSelcomboBox->setCurrentIndex(i); break; }
    ui->charaSelcomboBox->blockSignals(false);
    m_updatingUI = false;

    if (curCharIdx >= 0 && curCharIdx < m_charCache.size())
        refreshCharacterData(curCharIdx);
}

void MainWindow::onRefreshTimerEntity(){
    m_thingCache = m_gameData->readAllThings();
    int selEntity = selectedEntityIndex();
    refreshEntityList();
    if (selEntity >= 0) {
        QTableWidget *t = ui->entitytableWidget;
        for (int r = 0; r < t->rowCount(); ++r)
            if (t->item(r, 0)->data(Qt::UserRole).toInt() == selEntity)
            { t->selectRow(r); refreshEntityData(selEntity); break; }
    }
}

void MainWindow::onRefreshTimerMission(){
    // 如果用户正在编辑，跳过刷新
    if (hasEditingFocus()) return;

    m_missionCache = m_gameData->readMissionState();
    m_updatingUI = true;

    QList<QPlainTextEdit*> missonCharapainTextEdits = ui->missionChara->findChildren<QPlainTextEdit*>();
    for (size_t i = 0; i < missonCharapainTextEdits.length(); ++i)
    {
        if (m_missionCache.player_char[i])
        {
            QString pName(QString::number(m_missionCache.player_char[i] - 1));
            if (i <= ui->charaSelcomboBox->count())
            {
                pName = QString("[#%1]%2").arg(pName, ui->charaSelcomboBox->itemText(m_missionCache.player_char[i] - 1));
            }
            missonCharapainTextEdits[i]->setPlainText(pName);
            
        }else{
            missonCharapainTextEdits[i]->setPlainText("无");
        }
    }

    for (int i = 0; i < 7; ++i)
        if (ui->missionResourcetableWidget->item(i, 1))
            ui->missionResourcetableWidget->item(i, 1)->setText(QString::number(m_missionCache.resource[i]));

    // 刷新仓库武器
    QGridLayout *grid = qobject_cast<QGridLayout*>(ui->missionWeapon->layout());
    if (grid) {
        for (int i = 0; i < 15; ++i) {
            int r = i / 5, c = i % 5;
            QLayoutItem *li = grid->itemAtPosition(r, c);
            if (!li) continue;
            QWidget *cell = li->widget();
            if (!cell) continue;
            QPushButton *btn = cell->findChild<QPushButton*>();
            QSpinBox *sb = cell->findChild<QSpinBox*>();
            if (btn) {
                int wid = m_missionCache.storage_id[i];
                btn->setText(wid > 0 && wid < m_weaponNames.size() ? m_weaponNames[wid] : "(空)");
            }
            if (sb) sb->setValue(m_missionCache.storage_stack[i]);
        }
    }
    m_updatingUI = false;
}

void MainWindow::onRefreshTimer()
{
    if (!isAttached()) return;
    switch (ui->tabWidget->currentIndex())
    {
    case 0:
    onRefreshTimerChara();
        break;
    case 1:
    onRefreshTimerEntity();
        break;
    case 2:
    onRefreshTimerMission();
        break;
    default:
        break;
    }
}

// ==================== 全部刷新 ====================
void MainWindow::refreshAll()
{
    if (!isAttached()) return;
    m_weaponNames = m_gameData->readAllWeaponNames();
    onRefreshTimerChara();
    onRefreshTimerEntity();
    onRefreshTimerMission();
}