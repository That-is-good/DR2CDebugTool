#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , setting(new AddrSetting(this))
    , m_memMgr(new MemoryManager(this))
    , m_gameData(new GameDataReader(m_memMgr, this))
    , m_refreshTimer(new QTimer(this))
{
    resourceNames = {
        tr("无"), tr("食物"), tr("汽油"), tr("医疗"), tr("手枪"), tr("步枪"), tr("霰弹"), tr("垃圾")
    };
    statNames = {
        tr("士气"), tr("态度"), tr("镇静"), tr("魅力"), tr("智慧"), tr("忠诚"), tr("医疗"),
        tr("机械"), tr("射击"), tr("力量"), tr("灵巧"), tr("体能"), tr("活力")
    };
    ui->setupUi(this);
    setupUI();
    setupConnections();
    setWindowTitle(tr("加拿大的死亡之路调试工具"));
    QString configPath = QCoreApplication::applicationDirPath() + "/config.json";
    setting->loadFromFile(configPath);
    setBase();
}

MainWindow::~MainWindow()
{
    QString configPath = QCoreApplication::applicationDirPath() + "/config.json";
    setting->saveToFile(configPath);
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
    QWidget *w = QApplication::focusWidget();
    if (!w) return false;
    if (qobject_cast<QLineEdit*>(w)) return true;
    QWidget *p = w;
    while (p) {
        if (qobject_cast<QComboBox*>(p)) return true;
        p = p->parentWidget();
    }
    return false;
}

// ==================== UI 初始化 ====================
void MainWindow::setupUI()
{
    ui->filterProcessText->setPlaceholderText(tr("输入进程名过滤..."));
    setupEntityTable();
    setupCharacterStatTable();
    setupCharacterResourceTable();
    setupCharacterWeaponTable();
    setupMissionResourceTable();
    setupMissionWeaponTable();

    ui->entityTypecomboBox->addItem(tr("全部"), -1);
    ui->entityTypecomboBox->addItem(tr("人类"), 1);
    ui->entityTypecomboBox->addItem(tr("僵尸"), 2);
    ui->entityTypecomboBox->addItem(tr("物品"), 3);
    ui->entityTypecomboBox->addItem(tr("抛射物"), 4);
    ui->entityTypecomboBox->addItem(tr("家具"), 5);
    ui->entityTypecomboBox->addItem(tr("拾取物"), 6);
    ui->entityTypecomboBox->addItem(tr("武器"), 7);
    ui->entityTypecomboBox->addItem(tr("车辆"), 8);
    ui->entityTypecomboBox->addItem(tr("特殊拾取"), 9);
    ui->entityAreacomboBox->addItem(tr("全部"), -1);
    for (int i = 0; i < 16; ++i)
        ui->entityAreacomboBox->addItem(QString(tr("区域%1")).arg(i), i);

    ui->spawnEntitycomboBox->addItem(tr("人类"), 1);
    ui->spawnEntitycomboBox->addItem(tr("僵尸"), 2);
    ui->spawnEntitycomboBox->addItem(tr("物品"), 3);
    ui->spawnEntitycomboBox->addItem(tr("抛射物"), 4);
    // ui->spawnEntitycomboBox->addItem(tr("家具"), 5);
    // ui->spawnEntitycomboBox->addItem(tr("拾取物"), 6);
    // ui->spawnEntitycomboBox->addItem(tr("武器"), 7);
    // ui->spawnEntitycomboBox->addItem(tr("车辆"), 8);
    // ui->spawnEntitycomboBox->addItem(tr("特殊拾取"), 9);
    
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
    // 设置 / 进程
    connect(ui->settingBtn, &QPushButton::clicked, this, &MainWindow::onSetting);
    connect(ui->refreshProcessBtn, &QPushButton::clicked, this, &MainWindow::onRefreshProcess);
    connect(ui->attachProceesBtn, &QPushButton::clicked, this, &MainWindow::onAttachProcess);
    connect(ui->filterProcessText, &QLineEdit::returnPressed, this, [this]() {
        onFilterProcessChanged(ui->filterProcessText->text());
    });

    // ---- 角色 - 合并槽 ----
    connect(ui->charaSelcomboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onCharacterSelected);

    // 所有 QLineEdit → 同一个 slot
    for (auto *edit : {ui->charaNameplainTextEdit, ui->charaPerkplainTextEdit,
                        ui->charaTraitplainTextEdit, ui->charaDescplainTextEdit})
        connect(edit, &QLineEdit::returnPressed, this, [this]() {
            if (!m_updatingUI) onCharacterEditChanged();
        });

    // 所有角色 SpinBox/DoubleSpinBox → 同一个 slot
    connect(ui->charaHpspinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { if (!m_updatingUI) onCharacterSpinBoxChanged(); });
    connect(ui->charaSpeeddoubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { if (!m_updatingUI) onCharacterSpinBoxChanged(); });
    connect(ui->charaStatus1spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { if (!m_updatingUI) onCharacterSpinBoxChanged(); });
    connect(ui->charaStatus2spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { if (!m_updatingUI) onCharacterSpinBoxChanged(); });

    // 角色所有 QCheckBox → 同一个 slot
    for (auto *cb : {ui->charaFemalecheckBox, ui->charaPetcheckBox})
        connect(cb, &QCheckBox::toggled, this, [this]() {
            if (!m_updatingUI) onCharacterCheckBoxToggled();
        });

    // 属性/资源/武器 表
    connect(static_cast<QStandardItemModel*>(ui->charaStattableView->model()),
            &QStandardItemModel::dataChanged, this, &MainWindow::onCharacterStatChanged);
    connect(static_cast<QStandardItemModel*>(ui->charaResourcetableView->model()),
            &QStandardItemModel::dataChanged, this, &MainWindow::onCharacterResourceChanged);
    connect(static_cast<QStandardItemModel*>(ui->charaWeapontableView->model()),
            &QStandardItemModel::dataChanged, this, &MainWindow::onCharacterWeaponChanged);

    // ---- 实体 - 合并槽 ----
    connect(ui->entityTypecomboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onEntityTypeFilterChanged);
    connect(ui->entityAreacomboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onEntityAreaFilterChanged);
    connect(ui->entitytableWidget, &QTableWidget::itemSelectionChanged,
            this, &MainWindow::onEntityTableSelectionChanged);

    // 实体所有 QCheckBox → 同一个 slot
    QList<QCheckBox*> entityCBs = {
        ui->noCollidecheckBox, ui->unSeencheckBox, ui->inVisiblecheckBox,
        ui->noHitcheckBox, ui->noDamagecheckBox, ui->glowcheckBox
    };
    for (auto *cb : entityCBs)
        connect(cb, &QCheckBox::toggled, this, [this]() {
            if (!m_updatingUI) onEntityCheckBoxToggled();
        });

    // 实体所有 QDoubleSpinBox → 同一个 slot
    QList<QDoubleSpinBox*> entityDSBs = {
        ui->posXdoubleSpinBox, ui->posYdoubleSpinBox, ui->posZdoubleSpinBox,
        ui->velXdoubleSpinBox, ui->velYdoubleSpinBox, ui->velZdoubleSpinBox,
        ui->massdoubleSpinBox, ui->frictiondoubleSpinBox, ui->bounceFrictiondoubleSpinBox
    };
    for (auto *sb : entityDSBs)
        connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
            if (!m_updatingUI) onEntityDoubleSpinBoxChanged();
        });

    // 实体其他 QSpinBox → 同一个 slot
    connect(ui->hitpointsspinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        if (!m_updatingUI) onEntitySpinBoxChanged();
    });
    connect(ui->aiStatespinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        if (!m_updatingUI) onEntitySpinBoxChanged();
    });
    connect(ui->aiWaitspinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        if (!m_updatingUI) onEntitySpinBoxChanged();
    });

    // 实体操作按钮
    connect(ui->setTargetpushButton, &QPushButton::clicked, this, &MainWindow::onSetTargetEntity);
    connect(ui->teleportTargetpushButton, &QPushButton::clicked, this, &MainWindow::onTeleportToTarget);
    connect(ui->swapTargetpushButton, &QPushButton::clicked, this, &MainWindow::onSwapEntityPositions);
    connect(ui->destoryEntitypushButton, &QPushButton::clicked, this, &MainWindow::onDestoryEntity);
    connect(ui->spawnEntitypushButton, &QPushButton::clicked, this, &MainWindow::onSpawnEntity);

    // ---- 全局 ----
    connect(ui->missionResourcetableWidget, &QTableWidget::cellChanged, this, [this](int, int) {
        if (!m_updatingUI) onMissionChanged();
    });

    connect(ui->cmdplainTextEdit, &QLineEdit::returnPressed, this, &MainWindow::onCmdSend);

    // ---- 定时刷新 ----
    connect(m_refreshTimer, &QTimer::timeout, this, &MainWindow::onRefreshTimer);

    // ---- 附加/分离 ----
    connect(m_memMgr, &MemoryManager::processAttached, this, [this]() {
        quint64 base = m_memMgr->moduleBaseAddress();
        m_gameData->setModuleBase(base);
        statusBar()->showMessage(QString(tr("已附加: %1 (PID: %2) 模块基址: 0x%3"))
            .arg(m_memMgr->attachedProcessName())
            .arg(m_memMgr->attachedProcessId())
            .arg(base, 0, 16));
        refreshAll();
        m_refreshTimer->start();
        setControlsEnabled(true);
    });
    connect(m_memMgr, &MemoryManager::processDetached, this, [this]() {
        statusBar()->showMessage(tr("已分离"));
        setControlsEnabled(false);
        m_refreshTimer->stop();
    });
    connect(m_memMgr, &MemoryManager::attachError, this, [this](const QString &err) {
        statusBar()->showMessage(tr("错误: ") + err);
        QMessageBox::warning(this, tr("错误"), err);
    });
}

// ==================== 表格初始化 ====================
void MainWindow::setupEntityTable()
{
    QTableWidget *t = ui->entitytableWidget;
    t->setColumnCount(5);
    t->setHorizontalHeaderLabels({"ID", tr("类型"), tr("子类型"), tr("区域"), tr("地址")});
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->horizontalHeader()->setStretchLastSection(true);
}

void MainWindow::setupCharacterStatTable()
{
    QStandardItemModel *m = new QStandardItemModel(13, 6, this);
    m->setHorizontalHeaderLabels({tr("属性"), tr("基础值"), tr("临时值"), tr("附加值"), tr("有效值"), tr("是否已知")});
    for (int i = 0; i < 13; ++i) {
        QStandardItem *nameItem = new QStandardItem(statNames[i]);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        m->setItem(i, 0, nameItem);
        m->setItem(i, 1, new QStandardItem("0"));
        m->setItem(i, 2, new QStandardItem("0"));
        m->setItem(i, 3, new QStandardItem("0"));
        QStandardItem *effItem = new QStandardItem("0");
        effItem->setFlags(effItem->flags() & ~Qt::ItemIsEditable);
        m->setItem(i, 4, effItem);
        QStandardItem *dispItem = new QStandardItem();
        dispItem->setFlags(dispItem->flags() | Qt::ItemIsUserCheckable);
        dispItem->setCheckable(true);
        m->setItem(i, 5, dispItem);
    }
    ui->charaStattableView->setModel(m);
    ui->charaStattableView->horizontalHeader()->setStretchLastSection(true);
    ui->charaStattableView->setItemDelegateForColumn(1, new SpinBoxDelegate(-128, 127, this));
    ui->charaStattableView->setItemDelegateForColumn(2, new SpinBoxDelegate(-128, 127, this));
    ui->charaStattableView->setItemDelegateForColumn(3, new SpinBoxDelegate(-128, 127, this));
}

void MainWindow::setupCharacterResourceTable()
{
    QStandardItemModel *m = new QStandardItemModel(resourceNames.length(), 2, this);
    m->setHorizontalHeaderLabels({tr("资源"), tr("数量")});
    for (int i = 0; i < resourceNames.length(); ++i) {
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
    QStandardItemModel *m = new QStandardItemModel(3, 3, this);
    m->setHorizontalHeaderLabels({tr("武器"), tr("数量"), tr("锁定")});
    ui->charaWeapontableView->setModel(m);
    for (int i = 0; i < 3; ++i) {
        QPushButton *btn = new QPushButton(tr("(空)"), this);
        int slotIndex = i;
        connect(btn, &QPushButton::clicked, this, [this, slotIndex]() {
            onWeaponButtonClicked(-1, slotIndex);
        });
        ui->charaWeapontableView->setIndexWidget(m->index(i, 0), btn);
        m->setItem(i, 1, new QStandardItem("0"));
        QStandardItem *lockItem = new QStandardItem();
        lockItem->setFlags(lockItem->flags() | Qt::ItemIsUserCheckable);
        lockItem->setCheckable(true);
        m->setItem(i, 2, lockItem);
    }
    ui->charaWeapontableView->horizontalHeader()->setStretchLastSection(true);
    ui->charaWeapontableView->setItemDelegateForColumn(1, new SpinBoxDelegate(-999, 999, this));
}

void MainWindow::setupMissionResourceTable()
{
    QTableWidget *t = ui->missionResourcetableWidget;
    t->setColumnCount(2);
    t->setHorizontalHeaderLabels({tr("资源"), tr("数量")});
    t->setRowCount(resourceNames.length());
    for (int i = 0; i < resourceNames.length(); ++i) {
        t->setItem(i, 0, new QTableWidgetItem(resourceNames[i]));
        QTableWidgetItem *v = new QTableWidgetItem("0");
        v->setData(Qt::UserRole, i);
        t->setItem(i, 1, v);
    }
    t->horizontalHeader()->setStretchLastSection(true);
}

void MainWindow::setupMissionWeaponTable()
{
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

            QPushButton *btn = new QPushButton(tr("(空)"), this);
            connect(btn, &QPushButton::clicked, this, [this, idx]() {
                onStorageWeaponClicked(idx);
            });
            hbox->addWidget(btn);

            QSpinBox *sb = new QSpinBox(this);
            sb->setRange(0, 999);
            connect(sb, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
                if (!m_updatingUI) onMissionChanged();
            });
            hbox->addWidget(sb);

            grid->addWidget(cell, row, col);
        }
    }
}

// ==================== 设置 / 进程 ====================
void MainWindow::setBase()
{
    m_gameData->SetOffset(setting->GetOffset());
    m_gameData->SetSize(setting->GetSize());
    m_gameData->SetLength(setting->GetLength());
    m_refreshTimer->setInterval(setting->GetUpdateFrequency());
}

void MainWindow::onSetting()
{
    if (setting->isHidden()) {
        setting->exec();
        setBase();
    }
}

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
        QMessageBox::information(this, tr("提示"), tr("请选择进程"));
        return;
    }
    quint32 pid = ui->processcomboBox->currentData().toUInt();
    m_memMgr->attachProcessById(pid);
}

void MainWindow::refreshProcessList()
{
    QString filter = ui->filterProcessText->text();
    QString cur = ui->processcomboBox->currentText();
    ui->processcomboBox->clear();
    for (const auto &p : m_memMgr->enumerateProcesses()) {
        QString name = p["name"].toString();
        quint32 pid = p["pid"].toUInt();
        if (!filter.isEmpty() && !name.contains(filter, Qt::CaseInsensitive)
            && !QString::number(pid).contains(filter))
            continue;
        ui->processcomboBox->addItem(QString("%1 (PID: %2)").arg(name).arg(pid), pid);
    }
    int idx = ui->processcomboBox->findText(cur);
    if (idx >= 0) ui->processcomboBox->setCurrentIndex(idx);
}

// ==================== 角色 - 使用 modifyCharacter（读内存→修改→写回→返回最新数据） ====================
void MainWindow::onCharacterSelected(int)
{
    int idx = selectedCharacterIndex();
    if (idx >= 0 && isAttached())
        refreshCharacterData(idx);
}

void MainWindow::onCharacterEditChanged()
{
    int i = selectedCharacterIndex();
    if (i < 0 || m_updatingUI) return;
    QString name  = ui->charaNameplainTextEdit->text();
    QString perk  = ui->charaPerkplainTextEdit->text();
    QString trait = ui->charaTraitplainTextEdit->text();
    QString desc  = ui->charaDescplainTextEdit->text();
    m_charCache[i] = m_gameData->modifyCharacter(i, [&](CharacterData &ch) {
        ch.name = name;
        ch.perk = perk;
        ch.trait = trait;
        ch.description = desc;
    });
}

void MainWindow::onCharacterSpinBoxChanged()
{
    int i = selectedCharacterIndex();
    if (i < 0 || m_updatingUI) return;
    int hp = ui->charaHpspinBox->value();
    float spd = static_cast<float>(ui->charaSpeeddoubleSpinBox->value());
    int mf0 = ui->charaStatus1spinBox->value();
    int mf1 = ui->charaStatus2spinBox->value();
    m_charCache[i] = m_gameData->modifyCharacter(i, [&](CharacterData &ch) {
        ch.health = hp;
        ch.speed_bonus = spd;
        ch.mod_flags[0] = mf0;
        ch.mod_flags[1] = mf1;
    });
}

void MainWindow::onCharacterCheckBoxToggled()
{
    int i = selectedCharacterIndex();
    if (i < 0 || m_updatingUI) return;
    quint16 female = ui->charaFemalecheckBox->isChecked() ? 1 : 0;
    quint16 pet = ui->charaPetcheckBox->isChecked() ? 1 : 0;
    m_charCache[i] = m_gameData->modifyCharacter(i, [&](CharacterData &ch) {
        ch.femalePet[0] = female;
        ch.femalePet[1] = pet;
    });
}

void MainWindow::onCharacterStatChanged(const QModelIndex &topLeft, const QModelIndex &)
{
    if (m_updatingUI) return;
    int i = selectedCharacterIndex();
    if (i < 0) return;

    int col = topLeft.column();
    int row = topLeft.row();
    QStandardItemModel *sm = static_cast<QStandardItemModel*>(ui->charaStattableView->model());

    // UI col: 0=属性名, 1=基础值, 2=临时值, 3=附加值, 4=有效值, 5=是否已知
    // stats[0]=已知, stats[1]=基础, stats[2]=临时, stats[3]=附加
    if (col >= 1 && col <= 3) {
        // 从 UI 读取全部行
        bool ok;
        qint8 uiBase[13], uiTemp[13], uiBonus[13];
        for (int k = 0; k < 13; ++k) {
            uiBase[k]  = static_cast<qint8>(sm->item(k, 1)->text().toInt(&ok)); if (!ok) return;
            uiTemp[k]  = static_cast<qint8>(sm->item(k, 2)->text().toInt(&ok)); if (!ok) return;
            uiBonus[k] = static_cast<qint8>(sm->item(k, 3)->text().toInt(&ok)); if (!ok) return;
        }
        m_charCache[i] = m_gameData->modifyCharacter(i, [&](CharacterData &ch) {
            for (int k = 0; k < 13; ++k) {
                ch.stats[1][k] = uiBase[k];
                ch.stats[2][k] = uiTemp[k];
                ch.stats[3][k] = uiBonus[k];
            }
        });
        // 更新有效值显示
        m_updatingUI = true;
        for (int k = 0; k < 13; ++k) {
            int effective = uiBase[k] + uiTemp[k] + uiBonus[k];
            sm->item(k, 4)->setText(QString::number(effective));
        }
        m_updatingUI = false;
    } else if (col == 5) {
        bool known = sm->item(row, 5)->checkState() == Qt::Checked;
        m_charCache[i] = m_gameData->modifyCharacter(i, [&](CharacterData &ch) {
            ch.stats[0][row] = known ? 1 : 0;
        });
    }
}

void MainWindow::onCharacterResourceChanged(const QModelIndex &topLeft, const QModelIndex &)
{
    if (m_updatingUI) return;
    int i = selectedCharacterIndex();
    if (i < 0 || topLeft.column() != 1) return;

    QStandardItemModel *rm = static_cast<QStandardItemModel*>(ui->charaResourcetableView->model());
    int newRes[8];
    int cnt = resourceNames.length();
    for (int r = 0; r < cnt; ++r) {
        bool ok;
        newRes[r] = rm->item(r, 1)->text().toInt(&ok);
        if (!ok) newRes[r] = m_charCache[i].resource[r];
    }
    m_charCache[i] = m_gameData->modifyCharacter(i, [&](CharacterData &ch) {
        for (int r = 0; r < cnt; ++r)
            ch.resource[r] = newRes[r];
    });
}

void MainWindow::onCharacterWeaponChanged(const QModelIndex &topLeft, const QModelIndex &)
{
    if (m_updatingUI) return;
    int i = selectedCharacterIndex();
    if (i < 0) return;
    int col = topLeft.column();
    int slot = topLeft.row();
    // weaponslots[槽位][内容]
    // 内容：武器数量, 武器ID, 武器锁;
    // 界面：武器按钮 武器数量 武器锁
    if (col == 1 || col == 2) {
        QStandardItemModel *wm = static_cast<QStandardItemModel*>(ui->charaWeapontableView->model());
        bool ok;
        int stack = wm->item(slot, 1)->text().toInt(&ok);
        int lock = (wm->item(slot, 2)->checkState() == Qt::Checked) ? 1 : 0;
        m_charCache[i] = m_gameData->modifyCharacter(i, [&](CharacterData &ch) {
            ch.weaponslots[slot][0] = stack;
            ch.weaponslots[slot][2] = lock;
        });
    }
}

void MainWindow::refreshCharacterData(int ci)
{
    if (!isAttached() || ci < 0 || ci >= m_charCache.size()) return;

    const auto &ch = m_charCache[ci];
    m_updatingUI = true;

    ui->charaThingplainTextEdit->setText(QString::number(ch.id[1]));
    ui->charaNameplainTextEdit->setText(ch.name);
    ui->charaPerkplainTextEdit->setText(ch.perk);
    ui->charaTraitplainTextEdit->setText(ch.trait);
    ui->charaDescplainTextEdit->setText(ch.description);
    ui->charaHpspinBox->setValue(ch.health);
    ui->charaSpeeddoubleSpinBox->setValue(ch.speed_bonus);
    ui->charaFemalecheckBox->setChecked(ch.femalePet[0] != 0);
    ui->charaPetcheckBox->setChecked(ch.femalePet[1] != 0);
    ui->charaStatus1spinBox->setValue(ch.mod_flags[0]);
    ui->charaStatus2spinBox->setValue(ch.mod_flags[1]);

    // 属性表: stats[0]=已知, stats[1]=基础, stats[2]=临时, stats[3]=附加
    QStandardItemModel *statM = static_cast<QStandardItemModel*>(ui->charaStattableView->model());
    for (int i = 0; i < 13; ++i) {
        statM->item(i, 1)->setText(QString::number(ch.stats[1][i]));
        statM->item(i, 2)->setText(QString::number(ch.stats[2][i]));
        statM->item(i, 3)->setText(QString::number(ch.stats[3][i]));
        int effective = ch.stats[1][i] + ch.stats[2][i] + ch.stats[3][i];
        statM->item(i, 4)->setText(QString::number(effective));
        statM->item(i, 5)->setCheckState(ch.stats[0][i] ? Qt::Checked : Qt::Unchecked);
    }

    // 资源表
    QStandardItemModel *resM = static_cast<QStandardItemModel*>(ui->charaResourcetableView->model());
    for (int i = 0; i < resourceNames.length(); ++i)
        resM->item(i, 1)->setText(QString::number(ch.resource[i]));

    // weaponslots[槽位][内容]
    // 内容：武器数量, 武器ID, 武器锁;
    // 界面：武器按钮 武器数量 武器锁
    QStandardItemModel *wpM = static_cast<QStandardItemModel*>(ui->charaWeapontableView->model());
    for (int i = 0; i < 3; ++i) {
        int wid = ch.weaponslots[i][1];
        QPushButton *btn = qobject_cast<QPushButton*>(ui->charaWeapontableView->indexWidget(wpM->index(i, 0)));
        btn->setText((wid > 0 && wid < m_weaponNames.size()) ? m_weaponNames[wid] : tr("(空)"));
        wpM->item(i, 1)->setText(QString::number(ch.weaponslots[i][0]));
        wpM->item(i, 2)->setCheckState(ch.weaponslots[i][2] ? Qt::Checked : Qt::Unchecked);
    }

    m_updatingUI = false;
}

// ==================== 武器按钮 ====================
void MainWindow::onWeaponButtonClicked(int charIndex, int slot)
{
    if (!isAttached()) return;
    if (charIndex == -1) charIndex = selectedCharacterIndex();

    WeaponDialog dlg(m_weaponNames, this);
    if (dlg.exec() == QDialog::Accepted) {
        int wid = dlg.selectedWeaponIndex();
        if (wid < 0) return;
        // weaponslots[槽位][内容]
        // 内容：武器数量, 武器ID, 武器锁;
        // 界面：武器按钮 武器数量 武器锁
        m_charCache[charIndex] = m_gameData->modifyCharacter(charIndex, [&](CharacterData &ch) {
            ch.weaponslots[slot][1] = wid;
        });
        QStandardItemModel *m = static_cast<QStandardItemModel*>(ui->charaWeapontableView->model());
        QPushButton *btn = static_cast<QPushButton*>(ui->charaWeapontableView->indexWidget(m->index(slot, 0)));
        btn->setText(m_weaponNames.value(wid, tr("(空)")));
    }
}

void MainWindow::onStorageWeaponClicked(int slotIndex)
{
    if (!isAttached()) return;
    WeaponDialog dlg(m_weaponNames, this);
    if (dlg.exec() == QDialog::Accepted) {
        int wid = dlg.selectedWeaponIndex();
        if (wid < 0) return;
        m_missionCache.storage_slots[slotIndex][0] = wid;
        // 读取当前 stack
        QGridLayout *grid = qobject_cast<QGridLayout*>(ui->missionWeapon->layout());
        if (grid) {
            int r = slotIndex / 5, c = slotIndex % 5;
            QLayoutItem *li = grid->itemAtPosition(r, c);
            if (li) {
                QWidget *cell = li->widget();
                if (cell) {
                    QSpinBox *sb = cell->findChild<QSpinBox*>();
                    if (sb) m_missionCache.storage_slots[slotIndex][1] = sb->value();
                    QPushButton *btn = cell->findChild<QPushButton*>();
                    if (btn) btn->setText(m_weaponNames.value(wid, tr("(空)")));
                }
            }
        }
        m_gameData->writeMission(m_missionCache);
    }
}

// ==================== 实体 - 使用 modifyThing ====================
void MainWindow::onEntityTypeFilterChanged(int idx)
{ m_entityTypeFilter = ui->entityTypecomboBox->itemData(idx).toInt(); refreshEntityList(); }

void MainWindow::onEntityAreaFilterChanged(int idx)
{ m_entityAreaFilter = ui->entityAreacomboBox->itemData(idx).toInt(); refreshEntityList(); }

void MainWindow::onEntityTableSelectionChanged()
{ int e = selectedEntityIndex(); if (e >= 0) refreshEntityData(e); }

void MainWindow::onEntityCheckBoxToggled()
{
    int e = selectedEntityIndex();
    if (e < 0 || m_updatingUI) return;
    // vision[0]=unseen, vision[1]=invisible, hit[0]=no_hit, hit[1]=no_do_damage
    quint8 nocollide = ui->noCollidecheckBox->isChecked() ? 1 : 0;
    quint8 unseen   = ui->unSeencheckBox->isChecked()    ? 1 : 0;
    quint8 invisible = ui->inVisiblecheckBox->isChecked() ? 1 : 0;
    quint8 glow     = ui->glowcheckBox->isChecked()       ? 1 : 0;
    quint8 nohit    = ui->noHitcheckBox->isChecked()      ? 1 : 0;
    quint8 nodamage = ui->noDamagecheckBox->isChecked()   ? 1 : 0;
    m_thingCache[e] = m_gameData->modifyThing(e, [&](ThingData &th) {
        th.nocollide = nocollide;
        th.vision[0] = unseen;
        th.vision[1] = invisible;
        th.glow = glow;
        th.hit[0] = nohit;
        th.hit[1] = nodamage;
    });
}

void MainWindow::onEntityDoubleSpinBoxChanged()
{
    int e = selectedEntityIndex();
    if (e < 0 || m_updatingUI) return;
    float px = static_cast<float>(ui->posXdoubleSpinBox->value());
    float py = static_cast<float>(ui->posYdoubleSpinBox->value());
    float pz = static_cast<float>(ui->posZdoubleSpinBox->value());
    float vx = static_cast<float>(ui->velXdoubleSpinBox->value());
    float vy = static_cast<float>(ui->velYdoubleSpinBox->value());
    float vz = static_cast<float>(ui->velZdoubleSpinBox->value());
    float mass = static_cast<float>(ui->massdoubleSpinBox->value());
    float fric = static_cast<float>(ui->frictiondoubleSpinBox->value());
    float boun = static_cast<float>(ui->bounceFrictiondoubleSpinBox->value());
    m_thingCache[e] = m_gameData->modifyThing(e, [&](ThingData &th) {
        th.vec3d[0][0] = px; th.vec3d[0][1] = py; th.vec3d[0][2] = pz;
        th.vec3d[1][0] = vx; th.vec3d[1][1] = vy; th.vec3d[1][2] = vz;
        th.phy[0] = mass; th.phy[1] = fric; th.phy[2] = boun;
    });
}

void MainWindow::onEntitySpinBoxChanged()
{
    int e = selectedEntityIndex();
    if (e < 0 || m_updatingUI) return;
    int hp = ui->hitpointsspinBox->value();
    quint32 aistate = static_cast<quint32>(ui->aiStatespinBox->value());
    qint32 aiwait = static_cast<qint32>(ui->aiWaitspinBox->value());
    m_thingCache[e] = m_gameData->modifyThing(e, [&](ThingData &th) {
        th.hitpoints = hp;
        th.ai_state = aistate;
        th.ai_wait = aiwait;
    });
}

void MainWindow::refreshEntityList()
{
    if (!isAttached()) return;
    int selIdx = selectedEntityIndex();
    int scrollPos = 0;
    QTableWidget *t = ui->entitytableWidget;
    if (t->verticalScrollBar())
        scrollPos = t->verticalScrollBar()->value();

    m_thingCache = m_gameData->readAllThings();
    t->setRowCount(0);

    auto typeName = [](qint8 ty) -> QString {
        switch (ty) {
        case 1: return tr("人类");
        case 2: return tr("僵尸");
        case 3: return tr("物品");
        case 4: return tr("抛射物");
        default: return QString(tr("类型%1")).arg(ty);
        }
    };
    auto subName = [](qint8 st) -> QString {
        switch (st) {
        case 0: return tr("家具");
        case 1: return tr("拾取物");
        case 2: return tr("武器");
        case 3: return tr("车辆");
        case 4: return tr("特殊拾取");
        default: return QString(tr("子类型%1")).arg(st);
        }
    };

    int row = 0;
    for (int i = 0; i < m_thingCache.size(); ++i) {
        const auto &th = m_thingCache[i];
        if (th.id == 0) continue;
        if (m_entityAreaFilter >= 0 && th.mapid != m_entityAreaFilter) continue;
        qint8 ty = th.type[0];
        qint8 sub = th.type[1];
        if (m_entityTypeFilter >= 0 && m_entityTypeFilter <= 4 && ty != m_entityTypeFilter) continue;
        if (m_entityTypeFilter >= 5 && (ty != 3 || sub + 5 != m_entityTypeFilter)) continue;

        t->insertRow(row);
        QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(th.id));
        idItem->setData(Qt::UserRole, i);
        t->setItem(row, 0, idItem);
        t->setItem(row, 1, new QTableWidgetItem(typeName(ty)));
        t->setItem(row, 2, new QTableWidgetItem(ty == 3 ? subName(sub) : typeName(ty)));
        t->setItem(row, 3, new QTableWidgetItem(QString::number(th.mapid)));
        t->setItem(row, 4, new QTableWidgetItem("0x" + QString::number(th.addr, 16).toUpper()));
        ++row;
    }

    ui->entityCountLabel->setText(QString(tr("实体: %1")).arg(row));
    if (selIdx >= 0)
        for (int r = 0; r < t->rowCount(); ++r)
            if (t->item(r, 0)->data(Qt::UserRole).toInt() == selIdx) {
                t->selectRow(r);
                break;
            }
    if (t->verticalScrollBar())
        t->verticalScrollBar()->setValue(scrollPos);
}

void MainWindow::refreshEntityData(int ei)
{
    if (ei < 0 || ei >= m_thingCache.size()) return;
    const auto &th = m_thingCache[ei];
    m_updatingUI = true;

    ui->noCollidecheckBox->setChecked(th.nocollide);
    ui->unSeencheckBox->setChecked(th.vision[0]);
    ui->inVisiblecheckBox->setChecked(th.vision[1]);
    ui->glowcheckBox->setChecked(th.glow);
    ui->noHitcheckBox->setChecked(th.hit[0]);
    ui->noDamagecheckBox->setChecked(th.hit[1]);
    ui->posXdoubleSpinBox->setValue(static_cast<double>(th.vec3d[0][0]));
    ui->posYdoubleSpinBox->setValue(static_cast<double>(th.vec3d[0][1]));
    ui->posZdoubleSpinBox->setValue(static_cast<double>(th.vec3d[0][2]));
    ui->velXdoubleSpinBox->setValue(static_cast<double>(th.vec3d[1][0]));
    ui->velYdoubleSpinBox->setValue(static_cast<double>(th.vec3d[1][1]));
    ui->velZdoubleSpinBox->setValue(static_cast<double>(th.vec3d[1][2]));
    ui->massdoubleSpinBox->setValue(static_cast<double>(th.phy[0]));
    ui->frictiondoubleSpinBox->setValue(static_cast<double>(th.phy[1]));
    ui->bounceFrictiondoubleSpinBox->setValue(static_cast<double>(th.phy[2]));
    ui->hitpointsspinBox->setValue(th.hitpoints);
    ui->aiStatespinBox->setValue(static_cast<int>(th.ai_state));
    ui->aiWaitspinBox->setValue(static_cast<int>(th.ai_wait));
    m_updatingUI = false;
}

// ==================== 实体操作 ====================
void MainWindow::onSetTargetEntity()
{
    int e = selectedEntityIndex();
    if (e >= 0 && m_targetEntityIndex != e) {
        m_targetEntityIndex = e;
        statusBar()->showMessage(QString(tr("目标: ID=%1, 索引=%2")).arg(m_thingCache[e].id).arg(e));
        ui->setTargetpushButton->setStyleSheet("background-color:lightgreen;");
    } else {
        ui->setTargetpushButton->setStyleSheet("background-color:transparent;");
        m_targetEntityIndex = -1;
    }
}

void MainWindow::onTeleportToTarget()
{
    int cur = selectedEntityIndex();
    if (cur < 0 || m_targetEntityIndex < 0 || !isAttached()) {
        QMessageBox::information(this, tr("提示"), tr("请先选择实体并设置目标"));
        return;
    }
    const auto &target = m_thingCache[m_targetEntityIndex];
    m_thingCache[cur] = m_gameData->modifyThing(cur, [&](ThingData &d) {
        d.vec3d[0][0] = target.vec3d[0][0];
        d.vec3d[0][1] = target.vec3d[0][1];
        d.vec3d[0][2] = target.vec3d[0][2];
        d.mapid = target.mapid;
    });
    refreshEntityData(cur);
    statusBar()->showMessage(tr("已传送至目标"));
}

void MainWindow::onSwapEntityPositions()
{
    int cur = selectedEntityIndex();
    if (cur < 0 || m_targetEntityIndex < 0 || !isAttached()) {
        QMessageBox::information(this, tr("提示"), tr("请先选择实体并设置目标"));
        return;
    }
    int target = m_targetEntityIndex;
    // 暂存坐标
    float posCur[3] = { m_thingCache[cur].vec3d[0][0], m_thingCache[cur].vec3d[0][1], m_thingCache[cur].vec3d[0][2] };
    quint8 mapCur = m_thingCache[cur].mapid;
    float posTgt[3] = { m_thingCache[target].vec3d[0][0], m_thingCache[target].vec3d[0][1], m_thingCache[target].vec3d[0][2] };
    quint8 mapTgt = m_thingCache[target].mapid;

    m_thingCache[cur] = m_gameData->modifyThing(cur, [&](ThingData &d) {
        d.vec3d[0][0] = posTgt[0]; d.vec3d[0][1] = posTgt[1]; d.vec3d[0][2] = posTgt[2];
        d.mapid = mapTgt;
    });
    m_thingCache[target] = m_gameData->modifyThing(target, [&](ThingData &d) {
        d.vec3d[0][0] = posCur[0]; d.vec3d[0][1] = posCur[1]; d.vec3d[0][2] = posCur[2];
        d.mapid = mapCur;
    });
    refreshEntityData(cur);
    statusBar()->showMessage(tr("已交换位置"));
}

void MainWindow::onDestoryEntity()
{
    int idx = selectedEntityIndex();
    if (idx < 0 || !isAttached() || idx >= m_thingCache.size()) return;
    ThingData &th = m_thingCache[idx];
    if (th.id == 0) return;
    if (m_memMgr->FreeThing(th.addr)) {
        th.id = 0;
        refreshEntityList();
        statusBar()->showMessage(tr("已销毁实体"));
    } else {
        statusBar()->showMessage(tr("销毁实体失败"));
    }
}

void MainWindow::onSpawnEntity(){
    int type = ui->spawnEntitycomboBox->currentData().toInt();
    if (type < 1 || type > 4 || !isAttached()) return;

    bool ok = m_memMgr->AllocateEntity(type);
    if (ok)
    {
        statusBar()->showMessage(tr("已生成实体"));
    }else{
        statusBar()->showMessage(tr("生成实体失败"));
    }
    
}
// ==================== 全局 ====================
void MainWindow::onMissionChanged()
{
    if (m_updatingUI || !isAttached()) return;

    for (int i = 0; i < resourceNames.length(); ++i) {
        auto *item = ui->missionResourcetableWidget->item(i, 1);
        if (!item) continue;
        bool ok;
        int v = item->text().toInt(&ok);
        if (ok) m_missionCache.resource[i] = v;
    }

    QGridLayout *grid = qobject_cast<QGridLayout*>(ui->missionWeapon->layout());
    if (grid) {
        for (int i = 0; i < 15; ++i) {
            int r = i / 5, c = i % 5;
            QLayoutItem *li = grid->itemAtPosition(r, c);
            if (!li) continue;
            QWidget *cell = li->widget();
            if (!cell) continue;
            QSpinBox *sb = cell->findChild<QSpinBox*>();
            if (sb) m_missionCache.storage_slots[i][1] = sb->value();
        }
    }

    m_gameData->writeMission(m_missionCache);
}

void MainWindow::onCmdSend(){
    if (m_memMgr->ScriptEvaluateStringSafe(ui->cmdplainTextEdit->text())) {
        statusBar()->showMessage(tr("已发送命令"));
    } else {
        statusBar()->showMessage(tr("发送命令失败"));
    }
}
// ==================== 定时刷新 ====================
void MainWindow::onRefreshTimerChara()
{
    if (hasEditingFocus()) return;

    m_charCache = m_gameData->readAllCharacters();
    m_updatingUI = true;
    int curCharIdx = selectedCharacterIndex();
    ui->charaSelcomboBox->blockSignals(true);
    ui->charaSelcomboBox->clear();
    for (int i = 0; i < m_charCache.size(); ++i) {
        if (!m_charCache[i].name.isEmpty())
            ui->charaSelcomboBox->addItem(m_charCache[i].name, i);
    }
    for (int i = 0; i < ui->charaSelcomboBox->count(); ++i)
        if (ui->charaSelcomboBox->itemData(i).toInt() == curCharIdx) {
            ui->charaSelcomboBox->setCurrentIndex(i);
            break;
        }
    ui->charaSelcomboBox->blockSignals(false);
    m_updatingUI = false;

    if (curCharIdx >= 0 && curCharIdx < m_charCache.size())
        refreshCharacterData(curCharIdx);
}

void MainWindow::onRefreshTimerEntity()
{
    int selEntity = selectedEntityIndex();
    refreshEntityList();
    if (selEntity >= 0) {
        QTableWidget *t = ui->entitytableWidget;
        for (int r = 0; r < t->rowCount(); ++r)
            if (t->item(r, 0)->data(Qt::UserRole).toInt() == selEntity) {
                t->selectRow(r);
                refreshEntityData(selEntity);
                break;
            }
    }
}

void MainWindow::onRefreshTimerMission()
{
    if (hasEditingFocus()) return;

    m_missionCache = m_gameData->readMissionState();
    m_updatingUI = true;

    QList<QLineEdit*> missonCharapainTextEdits = ui->missionChara->findChildren<QLineEdit*>();
    for (int i = 0; i < missonCharapainTextEdits.length(); ++i) {
        if (m_missionCache.player_char[i]) {
            QString pName(QString::number(m_missionCache.player_char[i] - 1));
            if (i <= ui->charaSelcomboBox->count())
                pName = QString("[#%1]%2").arg(pName, ui->charaSelcomboBox->itemText(m_missionCache.player_char[i] - 1));
            missonCharapainTextEdits[i]->setText(pName);
        } else {
            missonCharapainTextEdits[i]->setText(tr("无"));
        }
    }

    for (int i = 0; i < 7; ++i)
        if (ui->missionResourcetableWidget->item(i, 1))
            ui->missionResourcetableWidget->item(i, 1)->setText(QString::number(m_missionCache.resource[i]));

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
                int wid = m_missionCache.storage_slots[i][0];
                btn->setText(wid > 0 && wid < m_weaponNames.size() ? m_weaponNames[wid] : tr("(空)"));
            }
            if (sb) sb->setValue(m_missionCache.storage_slots[i][1]);
        }
    }
    m_updatingUI = false;
}

void MainWindow::onRefreshTimer()
{
    if (!isAttached()) return;
    switch (ui->tabWidget->currentIndex()) {
    case 0: onRefreshTimerChara(); break;
    case 1: onRefreshTimerEntity(); break;
    case 2: onRefreshTimerMission(); break;
    default: break;
    }
}

void MainWindow::refreshAll()
{
    if (!isAttached()) return;
    m_weaponNames = m_gameData->readAllWeaponNames();
    onRefreshTimerChara();
    onRefreshTimerEntity();
    onRefreshTimerMission();
}