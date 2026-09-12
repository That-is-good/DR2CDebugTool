#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <algorithm>
#include <random>
#include <QSet>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QGraphicsRectItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsEllipseItem>
#include <QScrollBar>
#include <QPainter>
#include <QPair>
#include <QDir>
#include <QRandomGenerator>

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

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (m_entityMode == ResizeMode
        && watched == ui->entitygraphicsView->viewport()
        && event->type() == QEvent::Wheel) {
        QWheelEvent *we = static_cast<QWheelEvent*>(event);
        const qreal factor = we->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
        ui->entitygraphicsView->scale(factor, factor);
        return true;
    }

    if (m_entityMode == SelectMode
        && watched == ui->entitygraphicsView->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                QGraphicsItem *item = nullptr;
                for (QGraphicsItem *candidate : ui->entitygraphicsView->items(me->pos())) {
                    if (candidate->data(0).toULongLong() != 0) {
                        item = candidate;
                        break;
                    }
                }
                quint64 addr = item ? item->data(0).toULongLong() : 0;
                if (addr != 0) {
                    if (!item->isSelected()) {
                        m_entityScene->clearSelection();
                        item->setSelected(true);
                    }
                    m_draggingEntity = true;
                    m_dragAddr = addr;
                    m_dragItem = item;
                    m_dragOffset = item->mapFromScene(ui->entitygraphicsView->mapToScene(me->pos()));
                    m_dragStartRegions.clear();
                    for (auto *selected : m_entityScene->selectedItems()) {
                        const quint64 selectedAddr = selected->data(0).toULongLong();
                        if (selectedAddr != 0)
                            m_dragStartRegions.insert(
                                selectedAddr, regionContainingScenePos(selected->scenePos()));
                    }
                    m_refreshTimer->stop();
                    return true;
                }
            }
        } else if (event->type() == QEvent::MouseMove && m_draggingEntity) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            if (m_dragItem) {
                const QPointF scenePos = ui->entitygraphicsView->mapToScene(me->pos());
                const QPointF requestedPos = scenePos - m_dragOffset;
                const QPointF delta = requestedPos - m_dragItem->pos();
                for (auto *sel : m_entityScene->selectedItems()) {
                    if (sel->data(0).toULongLong() != 0)
                        sel->setPos(sel->pos() + delta);
                }
                updateEntityMarkerPositions();
            }
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && m_draggingEntity) {
            m_draggingEntity = false;
            if (isAttached()) {
                const auto selectedItems = m_entityScene->selectedItems();
                for (auto *sel : selectedItems) {
                    const quint64 addr = sel->data(0).toULongLong();
                    if (addr == 0) continue;
                    int region = regionContainingScenePos(sel->scenePos());
                    if (region < 0)
                        region = m_dragStartRegions.value(addr, 0);
                    const QPointF reboundPos = clampEntityPos(sel, sel->scenePos(), region);
                    moveEntityToScenePos(addr, reboundPos);
                }
            }
            m_dragAddr = 0;
            m_dragItem = nullptr;
            m_dragOffset = QPointF();
            m_dragStartRegions.clear();
            if (isAttached())
                m_refreshTimer->start();
            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

// ==================== 辅助 ====================
bool MainWindow::isAttached() const
{
    return m_memMgr && m_memMgr->isAttached();
}

int MainWindow::selectedCharacterIndex() const
{
    return ui->charaSelcomboBox->currentData().toInt();
}

ThingData *MainWindow::thingByAddr(quint64 addr)
{
    for (auto &th : m_thingCache)
        if (th.addr == addr)
            return &th;
    return nullptr;
}

quint64 MainWindow::selectedEntityAddr() const
{
    const auto items = m_entityScene->selectedItems();
    if (items.isEmpty()) return 0;
    return items.first()->data(0).toULongLong();
}

QList<quint64> MainWindow::selectedEntityAddrs() const
{
    QList<quint64> addrs;
    for (const auto *item : m_entityScene->selectedItems()) {
        quint64 addr = item->data(0).toULongLong();
        if (addr != 0)
            addrs.append(addr);
    }
    return addrs;
}

int MainWindow::iconIndexForThing(const ThingData &th) const
{
    const quint8 ty = th.type[0];
    const quint8 sub = th.type[1];
    if (ty >= 1 && ty <= 4 && ty != 3)
        return ty;
    if (ty == 3)
        return sub + 5; // 家具5 拾取物6 武器7 车辆8 特殊拾取9
    return 3;
}

void MainWindow::applyEntityFlagsToAddrs(const QList<quint64> &addrs, bool set, quint8 flag)
{
    for (quint64 addr : addrs) {
        m_gameData->modifyThing(addr, [&](ThingData &th) {
            switch (flag) {
            case 0: th.nocollide   = set ? 1 : 0; break;
            case 1: th.vision[0]   = set ? 1 : 0; break; // 隐身(unseen)
            case 2: th.vision[1]   = set ? 1 : 0; break; // 不绘制(invisible)
            case 3: th.no_hit      = set ? 1 : 0; break; // 不可被击中
            case 4: th.nopick      = set ? 1 : 0; break; // 不可拾取
            case 5: th.glow        = set ? 1 : 0; break; // 发光
            default: break;
            }
        });
    }
    //refreshEntityView();
}

bool MainWindow::hasEditingFocus() const
{
    QWidget *w = QApplication::focusWidget();
    if (!w) return false;
    if (qobject_cast<QLineEdit*>(w)) return true;
    if (qobject_cast<QSpinBox*>(w)) return true;
    if (qobject_cast<QDoubleSpinBox*>(w)) return true;
    // 注意：QComboBox 是选择控件，不视为编辑焦点，避免角色下拉框阻止刷新
    return false;
}

// ==================== UI 初始化 ====================
void MainWindow::setupUI()
{
    ui->filterProcessText->setPlaceholderText(tr("输入进程名过滤..."));
    setupEntityView();
    setupCharacterStatTable();
    setupCharacterResourceTable();
    setupCharacterWeaponTable();
    setupMissionResourceTable();
    setupMissionWeaponTable();

    ui->entityAreacomboBox->addItem(tr("全部"), -1);
    ui->selectEntityMode->setChecked(true);
    onEntityModeChanged();
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
    connect(ui->settingBtn, &QPushButton::clicked, this, &MainWindow::onSetting);
    connect(ui->refreshProcessBtn, &QPushButton::clicked, this, &MainWindow::onRefreshProcess);
    connect(ui->attachProceesBtn, &QPushButton::clicked, this, &MainWindow::onAttachProcess);
    connect(ui->filterProcessText, &QLineEdit::returnPressed, this, [this]() {
        onFilterProcessChanged(ui->filterProcessText->text());
    });

    connect(ui->charaSelcomboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onCharacterSelected);

    for (auto *edit : {ui->charaNameplainTextEdit, ui->charaPerkplainTextEdit,
                        ui->charaTraitplainTextEdit, ui->charaDescplainTextEdit})
        connect(edit, &QLineEdit::returnPressed, this, [this]() {
            if (!m_updatingUI) onCharacterEditChanged();
        });

    connect(ui->charaHpspinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { if (!m_updatingUI) onCharacterSpinBoxChanged(); });
    connect(ui->charaSpeeddoubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { if (!m_updatingUI) onCharacterSpinBoxChanged(); });
    connect(ui->charaStatus1spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { if (!m_updatingUI) onCharacterSpinBoxChanged(); });
    connect(ui->charaStatus2spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { if (!m_updatingUI) onCharacterSpinBoxChanged(); });

    for (auto *cb : {ui->charaFemalecheckBox, ui->charaPetcheckBox})
        connect(cb, &QCheckBox::toggled, this, [this]() {
            if (!m_updatingUI) onCharacterCheckBoxToggled();
        });

    connect(static_cast<QStandardItemModel*>(ui->charaStattableView->model()),
            &QStandardItemModel::dataChanged, this, &MainWindow::onCharacterStatChanged);
    connect(static_cast<QStandardItemModel*>(ui->charaResourcetableView->model()),
            &QStandardItemModel::dataChanged, this, &MainWindow::onCharacterResourceChanged);
    connect(static_cast<QStandardItemModel*>(ui->charaWeapontableView->model()),
            &QStandardItemModel::dataChanged, this, &MainWindow::onCharacterWeaponChanged);

    connect(ui->entityAreacomboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onEntityAreaFilterChanged);
    connect(ui->entitygraphicsView, &QGraphicsView::customContextMenuRequested,
            this, &MainWindow::onEntityMenu);

    connect(ui->selectEntityMode, &QRadioButton::toggled, this, &MainWindow::onEntityModeChanged);
    connect(ui->resizeScreenMode, &QRadioButton::toggled, this, &MainWindow::onEntityModeChanged);
    connect(ui->moveScreenMode, &QRadioButton::toggled, this, &MainWindow::onEntityModeChanged);

    connect(ui->missionResourcetableView, &QTableView::clicked, this, [this](const QModelIndex &) {
        if (!m_updatingUI) onMissionChanged();
    });

    connect(ui->cmdplainTextEdit, &QLineEdit::returnPressed, this, &MainWindow::onCmdSend);

    connect(m_refreshTimer, &QTimer::timeout, this, &MainWindow::onRefreshTimer);

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

// ==================== 视图初始化 ====================
void MainWindow::setupEntityView()
{
    m_iconPixmaps.resize(10); // 索引1..9 对应图标
    for (int i = 1; i <= 9; ++i) {
        QPixmap pix(QString(QCoreApplication::applicationDirPath() + "/Icons/%1.png").arg(i));
        if (pix.isNull())
            pix = QPixmap(24, 24);
        m_iconPixmaps[i] = pix;
    }
    m_leaderPixmap.load(QCoreApplication::applicationDirPath() + "/Icons/leader.png");

    const QStringList imageFilters = {"*.png", "*.jpg", "*.jpeg", "*.bmp"};
    auto loadPixmapDirectory = [&imageFilters](const QString &path) {
        QVector<QPixmap> pixmaps;
        const QFileInfoList files = QDir(path).entryInfoList(
            imageFilters, QDir::Files, QDir::Name);
        for (const QFileInfo &file : files) {
            QPixmap pixmap(file.absoluteFilePath());
            if (!pixmap.isNull())
                pixmaps.append(pixmap);
        }
        return pixmaps;
    };
    const QString iconsPath = QCoreApplication::applicationDirPath() + "/Icons";
    m_playerEntityPixmaps = loadPixmapDirectory(iconsPath + "/player");
    m_furniturePixmaps = loadPixmapDirectory(iconsPath + "/furniture");

    m_entityScene = new QGraphicsScene(this);
    ui->entitygraphicsView->setScene(m_entityScene);
    ui->entitygraphicsView->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->entitygraphicsView->setRenderHint(QPainter::Antialiasing, true);
    ui->entitygraphicsView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    ui->entitygraphicsView->setRubberBandSelectionMode(Qt::IntersectsItemShape);
    ui->entitygraphicsView->setMouseTracking(true);
    ui->entitygraphicsView->viewport()->installEventFilter(this);
}

void MainWindow::onEntityModeChanged()
{
    if (ui->selectEntityMode->isChecked()) {
        m_entityMode = SelectMode;
        ui->entitygraphicsView->setDragMode(QGraphicsView::RubberBandDrag);
    } else if (ui->resizeScreenMode->isChecked()) {
        m_entityMode = ResizeMode;
        ui->entitygraphicsView->setDragMode(QGraphicsView::NoDrag);
    } else if (ui->moveScreenMode->isChecked()) {
        m_entityMode = MoveMode;
        ui->entitygraphicsView->setDragMode(QGraphicsView::ScrollHandDrag);
    } else {
        m_entityMode = SelectMode;
        ui->entitygraphicsView->setDragMode(QGraphicsView::RubberBandDrag);
    }

    // 切换模式时更新实体图元的可选状态
    for (auto *item : m_entityScene->items()) {
        if (item->data(0).toULongLong() != 0) {
            item->setFlag(QGraphicsItem::ItemIsSelectable, m_entityMode == SelectMode);
        }
    }
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
    QTableView *t = ui->charaStattableView;
    t->setModel(m);
    t->horizontalHeader()->setStretchLastSection(true);
    t->setItemDelegateForColumn(1, new SpinBoxDelegate(-128, 127, this));
    t->setItemDelegateForColumn(2, new SpinBoxDelegate(-128, 127, this));
    t->setItemDelegateForColumn(3, new SpinBoxDelegate(-128, 127, this));
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
    QTableView *t = ui->missionResourcetableView;
    QStandardItemModel *m = new QStandardItemModel(resourceNames.length(), 2, this);
    m->setHorizontalHeaderLabels({tr("资源"), tr("数量")});
    for (int i = 0; i < resourceNames.length(); ++i) {
        QStandardItem *nameItem = new QStandardItem(resourceNames[i]);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        m->setItem(i, 0, nameItem);
        m->setItem(i, 1, new QStandardItem("0"));
    }
    t->setModel(m);
    t->horizontalHeader()->setStretchLastSection(true);
    t->setItemDelegateForColumn(1, new SpinBoxDelegate(-999999, 999999, this));
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

// ==================== 角色 ====================
void MainWindow::onCharacterSelected(int)
{
    int idx = selectedCharacterIndex();
    if (idx >= 0 && isAttached())
        refreshCharacterData(idx);
}

void MainWindow::onCharacterEditChanged()
{
    int i = selectedCharacterIndex();
    if (i < 0 || i >= m_charCache.size() || m_updatingUI) return;
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
    if (i < 0 || i >= m_charCache.size() || m_updatingUI) return;
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
    if (i < 0 || i >= m_charCache.size() || m_updatingUI) return;
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
    if (i < 0 || i >= m_charCache.size()) return;

    int col = topLeft.column();
    int row = topLeft.row();
    QStandardItemModel *sm = static_cast<QStandardItemModel*>(ui->charaStattableView->model());

    if (col >= 1 && col <= 3) {
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
    if (i < 0 || i >= m_charCache.size() || topLeft.column() != 1) return;

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
    if (i < 0 || i >= m_charCache.size()) return;
    int col = topLeft.column();
    int slot = topLeft.row();
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

    QStandardItemModel *statM = static_cast<QStandardItemModel*>(ui->charaStattableView->model());
    for (int i = 0; i < 13; ++i) {
        statM->item(i, 1)->setText(QString::number(ch.stats[1][i]));
        statM->item(i, 2)->setText(QString::number(ch.stats[2][i]));
        statM->item(i, 3)->setText(QString::number(ch.stats[3][i]));
        int effective = ch.stats[1][i] + ch.stats[2][i] + ch.stats[3][i];
        statM->item(i, 4)->setText(QString::number(effective));
        statM->item(i, 5)->setCheckState(ch.stats[0][i] ? Qt::Checked : Qt::Unchecked);
    }

    QStandardItemModel *resM = static_cast<QStandardItemModel*>(ui->charaResourcetableView->model());
    for (int i = 0; i < resourceNames.length(); ++i)
        resM->item(i, 1)->setText(QString::number(ch.resource[i]));

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
    if (charIndex < 0 || charIndex >= m_charCache.size()) return;

    WeaponDialog dlg(m_weaponNames, this);
    if (dlg.exec() == QDialog::Accepted) {
        int wid = dlg.selectedWeaponIndex();
        if (wid < 0) return;
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

// ==================== 实体 ====================
void MainWindow::onEntityAreaFilterChanged(int idx)
{ m_entityAreaFilter = ui->entityAreacomboBox->itemData(idx).toInt(); refreshEntityView(); }

int MainWindow::regionContainingScenePos(const QPointF &scenePos) const
{
    for (auto it = m_regionOffsets.constBegin(); it != m_regionOffsets.constEnd(); ++it) {
        const int mapid = it.key();
        const QPointF origin = it.value();
        const QSizeF sz = m_regionSizes.value(mapid, QSizeF());
        if (scenePos.x() >= origin.x() && scenePos.x() <= origin.x() + sz.width()
            && scenePos.y() >= origin.y() && scenePos.y() <= origin.y() + sz.height())
            return mapid;
    }
    return -1;
}

int MainWindow::regionFromScenePos(const QPointF &scenePos) const
{
    const int region = regionContainingScenePos(scenePos);
    return region >= 0 ? region : (m_entityAreaFilter >= 0 ? m_entityAreaFilter : 0);
}

QPointF MainWindow::clampEntityPos(const QGraphicsItem *item, const QPointF &scenePos,
                                   int region) const
{
    const QRectF bounds = item->boundingRect();
    const QPointF origin = m_regionOffsets.value(region, QPointF(0, 0));
    const QSizeF size = m_regionSizes.value(region, QSizeF());
    if (size.isEmpty()) return scenePos;

    return QPointF(
        qBound(origin.x() - bounds.left(), scenePos.x(),
               origin.x() + size.width() - bounds.right()),
        qBound(origin.y() - bounds.top(), scenePos.y(),
               origin.y() + size.height() - bounds.bottom()));
}

void MainWindow::updateEntityMarkerPositions()
{
    const int count = std::min(m_thingItems.size(), m_thingMarkers.size());
    for (int slot = 0; slot < count; ++slot) {
        QGraphicsPixmapItem *item = m_thingItems[slot];
        if (!item || !item->isVisible()) continue;

        const QPointF pos = item->scenePos();
        m_thingMarkers[slot]->setPos(pos);
        m_playerMarkers[slot]->setPos(pos);
        if (m_leaderMarker && m_leaderMarker->isVisible()
            && item->data(1).toUInt() == m_leaderMarker->data(1).toUInt())
            m_leaderMarker->setPos(pos);
    }
}

void MainWindow::rebuildEntityBackground(const QList<int> &regions,
                                         const QHash<int, QSizeF> &sizes,
                                         const QSet<int> &inactiveRegions)
{
    if (m_backgroundGroup) {
        m_entityScene->removeItem(m_backgroundGroup);
        delete m_backgroundGroup;
    }

    m_backgroundGroup = new QGraphicsItemGroup();
    m_backgroundGroup->setZValue(-10);
    m_entityScene->addItem(m_backgroundGroup);

    for (int mapid : regions) {
        const QPointF origin = m_regionOffsets.value(mapid);
        const QSizeF size = sizes.value(mapid);
        QGraphicsRectItem *rect = m_entityScene->addRect(
            origin.x(), origin.y(), size.width(), size.height(),
            QPen(QColor(210, 190, 210), 2),
            inactiveRegions.contains(mapid)
                ? QBrush(Qt::NoBrush)
                : QBrush(QColor("#5C5059")));
        m_backgroundGroup->addToGroup(rect);

        QGraphicsSimpleTextItem *label = m_entityScene->addSimpleText(
            QString(tr("区域%1")).arg(mapid));
        label->setBrush(QColor(200, 200, 200));
        label->setPos(origin.x() + 4, origin.y() + 4);
        m_backgroundGroup->addToGroup(label);
    }
}

void MainWindow::ensureEntityItems(int poolSize)
{
    if (m_thingItems.size() == poolSize)
        return;

    for (auto *item : m_thingItems)
        if (item) { m_entityScene->removeItem(item); delete item; }
    for (auto *marker : m_thingMarkers)
        if (marker) { m_entityScene->removeItem(marker); delete marker; }
    for (auto *marker : m_playerMarkers)
        if (marker) { m_entityScene->removeItem(marker); delete marker; }
    if (m_leaderMarker) {
        m_entityScene->removeItem(m_leaderMarker);
        delete m_leaderMarker;
        m_leaderMarker = nullptr;
    }

    m_thingItems.fill(nullptr, poolSize);
    m_thingMarkers.fill(nullptr, poolSize);
    m_playerMarkers.fill(nullptr, poolSize);
    m_thingIconCache.fill(0, poolSize);
    m_thingImageAddrCache.fill(0, poolSize);
    m_thingImageTypeCache.fill(0, poolSize);
    m_thingImageSubTypeCache.fill(0, poolSize);

    for (int slot = 0; slot < poolSize; ++slot) {
        QGraphicsPixmapItem *item = new QGraphicsPixmapItem();
        item->setAcceptHoverEvents(true);
        item->setData(0, 0);
        item->setData(1, 0);
        item->setVisible(false);
        item->setFlag(QGraphicsItem::ItemIsSelectable, m_entityMode == SelectMode);
        m_entityScene->addItem(item);
        m_thingItems[slot] = item;

        QGraphicsEllipseItem *centerMarker = new QGraphicsEllipseItem(-16, -16, 32, 32);
        centerMarker->setPen(QPen(QColor(255, 200, 0), 3));
        centerMarker->setBrush(QBrush(QColor(255, 200, 0, 60)));
        centerMarker->setAcceptedMouseButtons(Qt::NoButton);
        centerMarker->setFlag(QGraphicsItem::ItemIsSelectable, false);
        centerMarker->setVisible(false);
        centerMarker->setZValue(-8);
        m_entityScene->addItem(centerMarker);
        m_thingMarkers[slot] = centerMarker;

        QGraphicsRectItem *playerMarker = new QGraphicsRectItem();
        playerMarker->setPen(QPen(Qt::red, 2));
        playerMarker->setBrush(Qt::NoBrush);
        playerMarker->setData(0, 0);
        playerMarker->setAcceptedMouseButtons(Qt::NoButton);
        playerMarker->setZValue(5);
        playerMarker->setVisible(false);
        m_entityScene->addItem(playerMarker);
        m_playerMarkers[slot] = playerMarker;
    }
}

void MainWindow::moveEntityToScenePos(quint64 addr, const QPointF &scenePos)
{
    if (!addr || !isAttached()) return;
    const int region = regionFromScenePos(scenePos);
    const QPointF origin = m_regionOffsets.value(region, QPointF(0, 0));
    const float x = static_cast<float>(scenePos.x() - origin.x());
    const float y = static_cast<float>(scenePos.y() - origin.y());
    
    //float delta2d[2] = {0, 0};
    m_gameData->modifyThing(addr, [&](ThingData &th) {
        th.mapid = static_cast<quint8>(region);
        th.vec3d[0][0] = x;
        th.vec3d[0][1] = y;
    });
    statusBar()->showMessage(QString(tr("已移动实体至区域%1 (X:%2 Y:%3)"))
                                 .arg(region)
                                 .arg(x, 0, 'f', 1)
                                 .arg(y, 0, 'f', 1));
    //refreshEntityView();
}

quint64 MainWindow::spawnEntityAt(uint type, const QPointF &scenePos)
{
    const int region = regionFromScenePos(scenePos);
    const QPointF origin = m_regionOffsets.value(region, QPointF(0, 0));
    const float x = static_cast<float>(scenePos.x() - origin.x());
    const float y = static_cast<float>(scenePos.y() - origin.y());

    quint64 ptr = onSpawnEntity(type);
    if (ptr != 0 && ptr != static_cast<quint64>(-1))
        m_gameData->modifyThing(ptr, [&](ThingData &th) {
            th.mapid = static_cast<quint8>(region);
            th.vec3d[0][0] = x;
            th.vec3d[0][1] = y;
        });
    return ptr;
}

void MainWindow::RenderMapTileLayerAt(const QPointF &scenePos){
    if (!isAttached()) return;
    const int region = regionFromScenePos(scenePos);
    // const QPointF origin = m_regionOffsets.value(region, QPointF(0, 0));
    // const float x = static_cast<float>(scenePos.x() - origin.x());
    // const float y = static_cast<float>(scenePos.y() - origin.y());
    if (m_memMgr->SwitchActiveTileLayer(region)) {
        statusBar()->showMessage(tr("已设置渲染图层"));
    } else {
        statusBar()->showMessage(tr("设置图层失败"));
    }
}

void MainWindow::refreshEntityView()
{
    if (!isAttached()) return;
    if (m_draggingEntity) return;

    m_thingCache = m_gameData->readAllThings(); // 固定大小实体池，下标即池索引

    // 按过滤条件筛选可见实体
    QVector<ThingData> visible;
    visible.reserve(m_thingCache.size());
    for (const auto &th : m_thingCache) {
        if (th.id == 0) continue;
        if (m_entityAreaFilter >= 0 && th.mapid != m_entityAreaFilter) continue;
        visible.append(th);
    }

    // 动态刷新区域组合框：只保留"全部" + 实际存在的区域
    {
        QSet<int> areaSet;
        for (const auto &th : m_thingCache)
            if (th.id != 0)
                areaSet.insert(th.mapid);

        if (areaSet.size() != ui->entityAreacomboBox->count() - 1) {
            int prevArea = m_entityAreaFilter;
            ui->entityAreacomboBox->blockSignals(true);
            ui->entityAreacomboBox->clear();
            ui->entityAreacomboBox->addItem(tr("全部"), -1);
            QList<int> areas = areaSet.values();
            std::sort(areas.begin(), areas.end());
            for (int a : areas)
                ui->entityAreacomboBox->addItem(QString(tr("区域%1")).arg(a), a);
            int restoreIdx = 0;
            for (int i = 0; i < ui->entityAreacomboBox->count(); ++i)
                if (ui->entityAreacomboBox->itemData(i).toInt() == prevArea) { restoreIdx = i; break; }
            ui->entityAreacomboBox->setCurrentIndex(restoreIdx);
            m_entityAreaFilter = ui->entityAreacomboBox->itemData(restoreIdx).toInt();
            ui->entityAreacomboBox->blockSignals(false);
        }
    }

    // 读取区域元数据，计算场景偏移
    QSet<int> visibleRegions;
    for (const auto &th : visible)
        visibleRegions.insert(th.mapid);

    int maxMapId = 0;
    for (int mapid : visibleRegions)
        maxMapId = std::max(maxMapId, static_cast<int>(mapid));

    QList<MapAreaData> areaMetas = m_gameData->readAllMapAreas(maxMapId);

    const qreal margin = 40.0;
    const int columns = 3;
    qreal cursorX = margin;
    qreal cursorY = margin;
    qreal rowHeight = 0.0;

    QList<int> sortedRegions = visibleRegions.values();
    std::sort(sortedRegions.begin(), sortedRegions.end());

    QHash<int, QSizeF> regionSizes;
    QSet<int> inactiveRegions;
    for (int mapid : sortedRegions) {
        MapAreaData meta = (mapid < areaMetas.size()) ? areaMetas[mapid] : MapAreaData();
        qreal w = meta.pixel_width > 0 ? meta.pixel_width : (meta.width * meta.tile_width);
        qreal h = meta.pixel_height > 0 ? meta.pixel_height : (meta.height * meta.tile_height);
        if (w <= 0) w = 1024.0;
        if (h <= 0) h = 1024.0;
        const qreal scaleX = (meta.scale_x > 0) ? meta.scale_x : 1.0;
        const qreal scaleY = (meta.scale_y > 0) ? meta.scale_y : 1.0;
        w *= scaleX;
        h *= scaleY;
        regionSizes.insert(mapid, QSizeF(w, h));
        if (!meta.valid())
            inactiveRegions.insert(mapid); // 无元数据的区域暂不绘制背景，仅保留实体
    }

    // 网格布局各区域（含无元数据区域，保证偏移连贯）
    QHash<int, QPointF> newOffsets;
    int col = 0;
    for (int i = 0; i < sortedRegions.size(); ++i) {
        const int mapid = sortedRegions[i];
        const QSizeF sz = regionSizes[mapid];
        newOffsets.insert(mapid, QPointF(cursorX, cursorY));
        rowHeight = std::max(rowHeight, sz.height());
        ++col;
        if (col >= columns && i + 1 < sortedRegions.size()) {
            cursorX = margin;
            cursorY += rowHeight + margin;
            rowHeight = 0.0;
            col = 0;
        } else {
            cursorX += sz.width() + margin;
        }
    }

    // 布局签名：区域集合 + 尺寸，决定是否需要重建背景
    QString layoutSig;
    for (int mapid : sortedRegions) {
        const QSizeF sz = regionSizes[mapid];
        layoutSig += QString("%1:%2x%3;").arg(mapid).arg((int)sz.width()).arg((int)sz.height());
    }

    const bool layoutChanged = (layoutSig != m_layoutSignature);

    if (layoutChanged) {
        m_regionOffsets = newOffsets;
        m_regionSizes = regionSizes;
        m_layoutSignature = layoutSig;

        rebuildEntityBackground(sortedRegions, m_regionSizes, inactiveRegions);
    }

    const int poolSize = m_gameData->maxThings();
    ensureEntityItems(poolSize);

    if (!m_leaderMarker) {
        m_leaderMarker = new QGraphicsPixmapItem(m_leaderPixmap);
        m_leaderMarker->setOffset(-m_leaderPixmap.width() / 2.0,
                                  -m_leaderPixmap.height() - 4.0);
        m_leaderMarker->setAcceptedMouseButtons(Qt::NoButton);
        m_leaderMarker->setZValue(5);
        m_leaderMarker->setData(0, 0);
        m_leaderMarker->setData(1, 0);
        m_leaderMarker->setVisible(false);
        m_entityScene->addItem(m_leaderMarker);
    }
    m_leaderMarker->setVisible(false);
    m_leaderMarker->setData(1, 0);

    // ---- 每个槽位：仅更新属性，不重建图元 ----
    const quint32 leaderCharSlot = m_gameData->readMissionStateLeaderChar();
    quint32 leaderEntityId = 0;
    if (leaderCharSlot > 0
        && leaderCharSlot <= static_cast<quint32>(m_gameData->maxCharacters())) {
        const CharacterData leader = m_gameData->readCharacter(
            static_cast<qint32>(leaderCharSlot - 1));
        leaderEntityId = leader.id[1];
    }
    const auto playerCharSlots = m_gameData->readMissionStatePlayerChar();
    QSet<quint32> playerEntityIds;
    for (quint32 charSlot : playerCharSlots) {
        if (charSlot == 0 || charSlot > static_cast<quint32>(m_gameData->maxCharacters()))
            continue;
        const CharacterData player = m_gameData->readCharacter(
            static_cast<qint32>(charSlot - 1));
        if (player.id[1] != 0)
            playerEntityIds.insert(player.id[1]);
    }

    int aliveCount = 0;
    for (int slot = 0; slot < poolSize; ++slot) {
        const ThingData &th = m_thingCache[slot];
        QGraphicsPixmapItem *item = m_thingItems[slot];
        QGraphicsEllipseItem *marker = m_thingMarkers[slot];
        QGraphicsRectItem *playerMarker = m_playerMarkers[slot];

        if (th.id == 0
            || (m_entityAreaFilter >= 0 && th.mapid != m_entityAreaFilter)) {
            item->setVisible(false);
            marker->setVisible(false);
            playerMarker->setVisible(false);
            continue;
        }

        ++aliveCount;

        if (m_thingImageAddrCache[slot] != th.addr
            || m_thingImageTypeCache[slot] != th.type[0]
            || m_thingImageSubTypeCache[slot] != th.type[1]) {
            QPixmap pixmap;
            const QVector<QPixmap> *randomPixmaps = nullptr;
            const int iconIdx = iconIndexForThing(th);
            if (iconIdx == 1)
                randomPixmaps = &m_playerEntityPixmaps;
            else if (iconIdx == 5)
                randomPixmaps = &m_furniturePixmaps;

            if (randomPixmaps && !randomPixmaps->isEmpty()) {
                const int index = QRandomGenerator::global()->bounded(randomPixmaps->size());
                pixmap = randomPixmaps->at(index);
            } else {
                pixmap = m_iconPixmaps.value(iconIdx, m_iconPixmaps.value(3));
            }

            item->setPixmap(pixmap);
            item->setOffset(-pixmap.width() / 2.0, -pixmap.height() / 2.0);
            m_thingImageAddrCache[slot] = th.addr;
            m_thingImageTypeCache[slot] = th.type[0];
            m_thingImageSubTypeCache[slot] = th.type[1];
        }

        const QPointF origin = m_regionOffsets.value(th.mapid, QPointF(0, 0));
        const QPointF rawPos(origin.x() + th.vec3d[0][0], origin.y() + th.vec3d[0][1]);
        const QPointF pos = clampEntityPos(item, rawPos, th.mapid);

        item->setPos(pos);
        item->setData(0, QVariant::fromValue(th.addr)); // 地址可能变化（池槽位固定）
        item->setData(1, QVariant::fromValue(th.id));
        item->setToolTip(QString(tr("ID: %1 | %2 | 区域：%3\nX: %4 Y: %5 Z: %6"))
                             .arg(th.id)
                             .arg(th.typeString)
                             .arg(th.mapid)
                             .arg(th.vec3d[0][0], 0, 'f', 1)
                             .arg(th.vec3d[0][1], 0, 'f', 1)
                             .arg(th.vec3d[0][2], 0, 'f', 1));
        item->setVisible(true);

        const bool isCenter = (th.addr == m_selectedThingAddr);
        marker->setVisible(isCenter);
        if (isCenter)
            marker->setPos(pos);

        if (th.id == leaderEntityId && !m_leaderPixmap.isNull()) {
            playerMarker->setRect(item->boundingRect());
            playerMarker->setPen(Qt::NoPen);
            playerMarker->setBrush(Qt::NoBrush);
            playerMarker->setVisible(false);
            m_leaderMarker->setPos(pos);
            m_leaderMarker->setData(1, QVariant::fromValue(th.id));
            m_leaderMarker->setVisible(true);
        } else if (playerEntityIds.contains(th.id)) {
            playerMarker->setRect(item->boundingRect());
            playerMarker->setPos(pos);
            playerMarker->setPen(QPen(QColor(0, 200, 200), 2));
            playerMarker->setVisible(true);
        } else {
            playerMarker->setVisible(false);
        }
    }

    ui->entityCountLabel->setText(QString(tr("实体: %1")).arg(aliveCount));
}

// ==================== 实体操作 ====================
void MainWindow::onEntityMenu(const QPoint &pos){
    // pos 为 viewport 坐标，转成场景坐标供"新建"使用
    const QPointF scenePos = ui->entitygraphicsView->mapToScene(pos);

    QGraphicsItem *clickedItem = ui->entitygraphicsView->itemAt(pos);
    quint64 clickedAddr = 0;
    if (clickedItem)
        clickedAddr = clickedItem->data(0).toULongLong();

    // 未命中实体但命中背景/标记时，尝试取最上层实体图元
    if (clickedAddr == 0) {
        for (auto *item : ui->entitygraphicsView->items(pos)) {
            const quint64 a = item->data(0).toULongLong();
            if (a != 0) { clickedAddr = a; break; }
        }
    }

    // 收集操作对象：若有选中实体则用选中集，否则退化为本次点击的实体
    QList<quint64> selAddrs = selectedEntityAddrs();
    if (clickedAddr != 0 && !selAddrs.contains(clickedAddr)) {
        // 点击未选中的实体：仅对该实体操作（并选中它，便于后续克隆/销毁等）
        selAddrs.clear();
        selAddrs.append(clickedAddr);
        m_entityScene->clearSelection();
        if (clickedItem)
            clickedItem->setSelected(true);
    }
    if (selAddrs.isEmpty() && clickedAddr != 0)
        selAddrs.append(clickedAddr);

    QMenu entitytableViewMenu(this);
    QList<QAction*> flagActs;
    QList<bool> flagBools = {true, true, true, true, true, true};
    if (!selAddrs.isEmpty()) {
        entitytableViewMenu.addAction(tr("设置为中心"), this, [this, clickedAddr]() {
            m_selectedThingAddr = clickedAddr;
            statusBar()->showMessage(QString(tr("已设置中心实体: 0x%1"))
                .arg(QString::number(m_selectedThingAddr, 16).toUpper()));
            refreshEntityView(); // 立即刷新以显示高亮圈（图元复用，开销极小）
        });

        QMenu *flagMenu = entitytableViewMenu.addMenu(tr("标志"));
        flagActs.append(flagMenu->addAction(tr("无碰撞")));
        flagActs.append(flagMenu->addAction(tr("隐身")));
        flagActs.append(flagMenu->addAction(tr("不绘制")));
        flagActs.append(flagMenu->addAction(tr("不可被击中")));
        flagActs.append(flagMenu->addAction(tr("不可拾取")));
        flagActs.append(flagMenu->addAction(tr("发光")));
        for (QAction *act : flagActs)
            act->setCheckable(true);
        for (quint64 addr : selAddrs) {
            ThingData *cur = thingByAddr(addr);
            if (!cur) continue;
            flagBools[0] &= (cur->nocollide != 0);
            flagBools[1] &= (cur->vision[0] != 0);
            flagBools[2] &= (cur->vision[1] != 0);
            flagBools[3] &= (cur->no_hit != 0);
            flagBools[4] &= (cur->nopick != 0);
            flagBools[5] &= (cur->glow != 0);
        }
        flagActs[0]->setChecked(flagBools[0]);
        flagActs[1]->setChecked(flagBools[1]);
        flagActs[2]->setChecked(flagBools[2]);
        flagActs[3]->setChecked(flagBools[3]);
        flagActs[4]->setChecked(flagBools[4]);
        flagActs[5]->setChecked(flagBools[5]);

        entitytableViewMenu.addSeparator();
        entitytableViewMenu.addAction(tr("修改位置"), this, [this, selAddrs]() { onEditEntityPosition(selAddrs); });
        entitytableViewMenu.addAction(tr("修改速度"), this, [this, selAddrs]() { onEditEntityVelocity(selAddrs); });
        entitytableViewMenu.addAction(tr("修改物理"), this, [this, selAddrs]() { onEditEntityPhysics(selAddrs); });
        entitytableViewMenu.addAction(tr("修改其他"), this, [this, selAddrs]() { onEditEntityOther(selAddrs); });
        entitytableViewMenu.addSeparator();
        entitytableViewMenu.addAction(tr("销毁"), this, [this, selAddrs]() { onDestoryEntity(selAddrs); });
        entitytableViewMenu.addAction(tr("传送至中心"), this, [this, selAddrs]() { onTeleportToTarget(selAddrs); });
        entitytableViewMenu.addAction(tr("随机交换"), this, [this, selAddrs]() { onSwapEntityPositions(selAddrs); });
        entitytableViewMenu.addAction(tr("克隆"), this, [this, selAddrs]() { onCloneEntity(selAddrs); });
        entitytableViewMenu.addAction(tr("设为玩家实体"), this, [this, clickedAddr]() { onSetAsPlayerEntity(clickedAddr); });
        entitytableViewMenu.addSeparator();
    }

    QMenu entitytableViewNewMenu(tr("新建"), this);
    QList<QAction*> newactions;
    newactions.append(entitytableViewNewMenu.addAction(tr("人类")));
    newactions.append(entitytableViewNewMenu.addAction(tr("僵尸")));
    newactions.append(entitytableViewNewMenu.addAction(tr("物品")));
    newactions.append(entitytableViewNewMenu.addAction(tr("抛射物")));
    newactions.append(entitytableViewNewMenu.addAction(tr("家具")));
    newactions.append(entitytableViewNewMenu.addAction(tr("拾取物")));
    newactions.append(entitytableViewNewMenu.addAction(tr("武器")));
    newactions.append(entitytableViewNewMenu.addAction(tr("车辆")));
    newactions.append(entitytableViewNewMenu.addAction(tr("特殊拾取")));
    entitytableViewMenu.addMenu(&entitytableViewNewMenu);
    entitytableViewMenu.addAction(tr("设置渲染图层"), this, [this, scenePos]() { RenderMapTileLayerAt(scenePos); });

    // 模态菜单期间暂停定时刷新，避免模型重建
    bool timerWasActive = m_refreshTimer->isActive();
    m_refreshTimer->stop();

    QAction *selectedAction = entitytableViewMenu.exec(ui->entitygraphicsView->viewport()->mapToGlobal(pos));

    if (timerWasActive)
        m_refreshTimer->start();

    if (!selectedAction) return;

    if (flagActs.contains(selectedAction)) {
        int flagIdx = flagActs.indexOf(selectedAction);
        applyEntityFlagsToAddrs(selAddrs, selectedAction->isChecked(), static_cast<quint8>(flagIdx));
    } else if (newactions.contains(selectedAction)) {
        spawnEntityAt(newactions.indexOf(selectedAction) + 1, scenePos);
        //refreshEntityView();
    }
}

void MainWindow::onEditEntityPosition(const QList<quint64> &addrs)
{
    if (addrs.isEmpty() || !isAttached()) return;
    ThingData cur = m_gameData->readThing(addrs.first());

    QDialog dlg(this);
    dlg.setWindowTitle(tr("修改位置"));
    QFormLayout *form = new QFormLayout(&dlg);

    QSpinBox *areaSb = new QSpinBox(&dlg);
    areaSb->setRange(-128, 127);
    areaSb->setValue(cur.mapid);
    form->addRow(tr("区域"), areaSb);

    auto makeDSpin = [&dlg](double v) {
        QDoubleSpinBox *sb = new QDoubleSpinBox(&dlg);
        sb->setDecimals(3);
        sb->setRange(-65535.0, 65535.0);
        sb->setValue(v);
        return sb;
    };
    QDoubleSpinBox *x = makeDSpin(static_cast<double>(cur.vec3d[0][0]));
    QDoubleSpinBox *y = makeDSpin(static_cast<double>(cur.vec3d[0][1]));
    QDoubleSpinBox *z = makeDSpin(static_cast<double>(cur.vec3d[0][2]));
    form->addRow(tr("坐标X"), x);
    form->addRow(tr("坐标Y"), y);
    form->addRow(tr("坐标Z"), z);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(box);

    bool timerWasActive = m_refreshTimer->isActive();
    m_refreshTimer->stop();
    if (dlg.exec() == QDialog::Accepted) {
        for (quint64 a : addrs) {
            m_gameData->modifyThing(a, [&](ThingData &th) {
                th.mapid = static_cast<quint8>(areaSb->value());
                th.vec3d[0][0] = static_cast<float>(x->value());
                th.vec3d[0][1] = static_cast<float>(y->value());
                th.vec3d[0][2] = static_cast<float>(z->value());
            });
        }
        //refreshEntityView();
    }
    if (timerWasActive) m_refreshTimer->start();
}

void MainWindow::onEditEntityVelocity(const QList<quint64> &addrs)
{
    if (addrs.isEmpty() || !isAttached()) return;
    ThingData cur = m_gameData->readThing(addrs.first());

    QDialog dlg(this);
    dlg.setWindowTitle(tr("修改速度"));
    QFormLayout *form = new QFormLayout(&dlg);

    auto makeDSpin = [&dlg](double v) {
        QDoubleSpinBox *sb = new QDoubleSpinBox(&dlg);
        sb->setDecimals(3);
        sb->setRange(-65535.0, 65535.0);
        sb->setValue(v);
        return sb;
    };
    QDoubleSpinBox *x = makeDSpin(static_cast<double>(cur.vec3d[1][0]));
    QDoubleSpinBox *y = makeDSpin(static_cast<double>(cur.vec3d[1][1]));
    QDoubleSpinBox *z = makeDSpin(static_cast<double>(cur.vec3d[1][2]));
    form->addRow(tr("速度X"), x);
    form->addRow(tr("速度Y"), y);
    form->addRow(tr("速度Z"), z);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(box);

    bool timerWasActive = m_refreshTimer->isActive();
    m_refreshTimer->stop();
    if (dlg.exec() == QDialog::Accepted) {
        for (quint64 a : addrs) {
            m_gameData->modifyThing(a, [&](ThingData &th) {
                th.vec3d[1][0] = static_cast<float>(x->value());
                th.vec3d[1][1] = static_cast<float>(y->value());
                th.vec3d[1][2] = static_cast<float>(z->value());
            });
        }
        //refreshEntityView();
    }
    if (timerWasActive) m_refreshTimer->start();
}

void MainWindow::onEditEntityPhysics(const QList<quint64> &addrs)
{
    if (addrs.isEmpty() || !isAttached()) return;
    ThingData cur = m_gameData->readThing(addrs.first());

    QDialog dlg(this);
    dlg.setWindowTitle(tr("修改物理"));
    QFormLayout *form = new QFormLayout(&dlg);

    auto makeDSpin = [&dlg](double v) {
        QDoubleSpinBox *sb = new QDoubleSpinBox(&dlg);
        sb->setDecimals(3);
        sb->setRange(-65535.0, 65535.0);
        sb->setValue(v);
        return sb;
    };
    QDoubleSpinBox *mass = makeDSpin(static_cast<double>(cur.phy[0]));
    QDoubleSpinBox *fric = makeDSpin(static_cast<double>(cur.phy[1]));
    QDoubleSpinBox *boun = makeDSpin(static_cast<double>(cur.phy[2]));
    form->addRow(tr("质量"), mass);
    form->addRow(tr("摩擦力"), fric);
    form->addRow(tr("弹跳力"), boun);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(box);

    bool timerWasActive = m_refreshTimer->isActive();
    m_refreshTimer->stop();
    if (dlg.exec() == QDialog::Accepted) {
        for (quint64 a : addrs) {
            m_gameData->modifyThing(a, [&](ThingData &th) {
                th.phy[0] = static_cast<float>(mass->value());
                th.phy[1] = static_cast<float>(fric->value());
                th.phy[2] = static_cast<float>(boun->value());
            });
        }
        //refreshEntityView();
    }
    if (timerWasActive) m_refreshTimer->start();
}

void MainWindow::onEditEntityOther(const QList<quint64> &addrs)
{
    if (addrs.isEmpty() || !isAttached()) return;
    ThingData cur = m_gameData->readThing(addrs.first());

    QDialog dlg(this);
    dlg.setWindowTitle(tr("修改其他"));
    QFormLayout *form = new QFormLayout(&dlg);

    QSpinBox *hp = new QSpinBox(&dlg);
    hp->setRange(-65535, 65535);
    hp->setValue(cur.hitpoints);
    form->addRow(tr("生命值"), hp);

    QSpinBox *sprite = new QSpinBox(&dlg);
    sprite->setRange(0, 65535);
    sprite->setValue(cur.spriteid);
    form->addRow(tr("精灵图"), sprite);

    QSpinBox *ai = new QSpinBox(&dlg);
    ai->setRange(0, 65535);
    ai->setValue(static_cast<int>(cur.ai_state));
    form->addRow(tr("AI状态"), ai);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(box);

    bool timerWasActive = m_refreshTimer->isActive();
    m_refreshTimer->stop();
    if (dlg.exec() == QDialog::Accepted) {
        for (quint64 a : addrs) {
            m_gameData->modifyThing(a, [&](ThingData &th) {
                th.hitpoints = hp->value();
                th.spriteid = static_cast<quint16>(sprite->value());
                th.ai_state = static_cast<quint32>(ai->value());
            });
        }
        //refreshEntityView();
    }
    if (timerWasActive) m_refreshTimer->start();
}

void MainWindow::onTeleportToTarget(const QList<quint64> &addrs)
{
    if (m_selectedThingAddr == 0 || !isAttached()) {
        QMessageBox::information(this, tr("提示"), tr("请先设置中心实体"));
        return;
    }
    ThingData *target = thingByAddr(m_selectedThingAddr);
    if (!target) {
        QMessageBox::information(this, tr("提示"), tr("中心实体不存在"));
        return;
    }

    int count = 0;
    for (quint64 addr : addrs) {
        if (addr == m_selectedThingAddr) continue;
        m_gameData->modifyThing(addr, [&](ThingData &d) {
            d.vec3d[0][0] = target->vec3d[0][0];
            d.vec3d[0][1] = target->vec3d[0][1];
            d.vec3d[0][2] = target->vec3d[0][2];
            d.mapid = target->mapid;
        });
        ++count;
    }
    if (count > 0) {
        statusBar()->showMessage(QString(tr("已传送 %1 个实体至目标")).arg(count));
        //refreshEntityView();
    } else {
        statusBar()->showMessage(tr("未选择要传送的实体"));
    }
}

void MainWindow::onSwapEntityPositions(const QList<quint64> &addrs)
{
    if (!isAttached()) return;

    QList<ThingData*> entities;
    for (quint64 addr : addrs) {
        ThingData *th = thingByAddr(addr);
        if (th)
            entities.append(th);
    }
    if (entities.size() < 2) {
        QMessageBox::information(this, tr("提示"), tr("请至少选择两个实体"));
        return;
    }

    QVector<QVector<float>> positions;
    QVector<quint8> maps;
    for (ThingData *th : entities) {
        QVector<float> pos;
        pos.append(th->vec3d[0][0]);
        pos.append(th->vec3d[0][1]);
        pos.append(th->vec3d[0][2]);
        positions.append(pos);
        maps.append(th->mapid);
    }

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(positions.begin(), positions.end(), g);
    std::shuffle(maps.begin(), maps.end(), g);

    for (int k = 0; k < entities.size(); ++k) {
        m_gameData->modifyThing(entities[k]->addr, [&](ThingData &d) {
            d.vec3d[0][0] = positions[k][0];
            d.vec3d[0][1] = positions[k][1];
            d.vec3d[0][2] = positions[k][2];
            d.mapid = maps[k];
        });
    }

    statusBar()->showMessage(QString(tr("已随机交换 %1 个实体的位置")).arg(entities.size()));
    //refreshEntityView();
}

void MainWindow::onDestoryEntity(const QList<quint64> &addrs)
{
    if (!isAttached()) return;

    int count = 0;
    for (quint64 addr : addrs) {
        ThingData *th = thingByAddr(addr);
        if (!th) continue;
        if (m_memMgr->FreeThing(th->addr)) {
            th->id = 0;
            ++count;
        }
    }
    if (count > 0) {
        statusBar()->showMessage(QString(tr("已销毁 %1 个实体")).arg(count));
        //refreshEntityView();
    } else {
        statusBar()->showMessage(tr("销毁实体失败"));
    }
}

quint64 MainWindow::onSpawnEntity(uint type){
    if (type < 1 || type > 9 || !isAttached()) return -1;

    quint64 thingPtr = 0;
    if (type == 1){
        thingPtr = m_memMgr->AllocateEntity(1);
        if (!thingPtr) {
            statusBar()->showMessage(tr("生成实体失败"));
            return -1;
        }
        quint32 charSlot = m_memMgr->AllocateCharacterSlot();
        if (!charSlot) {
            m_memMgr->FreeThing(thingPtr);
            statusBar()->showMessage(tr("生成实体失败: 角色池已满"));
            return -1;
        }
        m_memMgr->Assigncharactertothing(thingPtr, charSlot);
    }
    else if (type < 5){
        thingPtr = m_memMgr->AllocateEntity(type > 4 ? 3 : type);
    }else{
        thingPtr = m_memMgr->AllocateThing(type - 5);
    }
    if (thingPtr)
    {
        statusBar()->showMessage(tr("已生成实体"));
        //refreshEntityView();
    }else{
        statusBar()->showMessage(tr("生成实体失败"));
    }
    return thingPtr;
}

void MainWindow::onCloneEntity(const QList<quint64> &addrs){
    if (!isAttached()) return;

    int count = 0;
    for (quint64 addr : addrs) {
        ThingData *th = thingByAddr(addr);
        if (!th) continue;
        quint64 newThingPtr =  m_memMgr->AllocateEntity(th->type[0]);
        if (m_gameData->copyThing(th->addr, newThingPtr)) {
            ++count;
        }
    }
    if (count > 0) {
        statusBar()->showMessage(QString(tr("已克隆 %1 个实体")).arg(count));
        //refreshEntityView();
    } else {
        statusBar()->showMessage(tr("克隆实体失败"));
    }
}

void MainWindow::onSetAsPlayerEntity(quint64 addr){
    if (!addr || !isAttached()) return;
    ThingData cur = m_gameData->readThing(addr);
    if (m_memMgr->SetCurrentPlayerThing(cur.addr)){
        statusBar()->showMessage(tr("已设置为玩家实体"));
    }else{
        statusBar()->showMessage(tr("设置为玩家实体失败"));
    }
}

// ==================== 全局 ====================
void MainWindow::onMissionChanged()
{
    if (m_updatingUI || !isAttached()) return;

    QStandardItemModel *resM = static_cast<QStandardItemModel*>(ui->missionResourcetableView->model());
    for (int i = 0; i < resourceNames.length(); ++i) {
        bool ok;
        int v = resM->item(i, 1)->text().toInt(&ok);
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
    const auto playerCharSlots = m_gameData->readMissionStatePlayerChar();
    QSet<quint32> playerCharSlotSet;
    for (quint32 charSlot : playerCharSlots)
        if (charSlot != 0)
            playerCharSlotSet.insert(charSlot);

    // 构建当前角色列表：以角色ID(id[0])判断是否存在（名字可重复，ID唯一）
    QList<QPair<QString,int>> charList;
    for (int i = 0; i < m_charCache.size(); ++i) {
        if (m_charCache[i].id[0] != 0) {
            QString displayName = m_charCache[i].name;
            if (playerCharSlotSet.contains(static_cast<quint32>(i + 1)))
                displayName += QStringLiteral(" ★");
            charList.append(qMakePair(displayName, i));
        }
    }

    int curCharIdx = selectedCharacterIndex();

    // 数量变化 → 重建下拉框
    if (ui->charaSelcomboBox->count() != charList.size()) {
        m_updatingUI = true;
        ui->charaSelcomboBox->blockSignals(true);
        ui->charaSelcomboBox->clear();
        for (const auto &p : charList)
            ui->charaSelcomboBox->addItem(p.first, p.second);
        for (int i = 0; i < ui->charaSelcomboBox->count(); ++i)
            if (ui->charaSelcomboBox->itemData(i).toInt() == curCharIdx) {
                ui->charaSelcomboBox->setCurrentIndex(i);
                break;
            }
        ui->charaSelcomboBox->blockSignals(false);
        m_updatingUI = false;
    } else {
        // 数量没变 → 按角色索引(itemData)匹配，仅更新名字文本，不重建
        m_updatingUI = true;
        ui->charaSelcomboBox->blockSignals(true);
        for (const auto &p : charList) {
            for (int j = 0; j < ui->charaSelcomboBox->count(); ++j) {
                if (ui->charaSelcomboBox->itemData(j).toInt() == p.second) {
                    if (ui->charaSelcomboBox->itemText(j) != p.first)
                        ui->charaSelcomboBox->setItemText(j, p.first);
                    break;
                }
            }
        }
        ui->charaSelcomboBox->blockSignals(false);
        m_updatingUI = false;
    }

    if (curCharIdx >= 0 && curCharIdx < m_charCache.size())
        refreshCharacterData(curCharIdx);
}

void MainWindow::onRefreshTimerEntity()
{
    if (hasEditingFocus()) return;
    refreshEntityView();
}

void MainWindow::onRefreshTimerMission()
{
    if (hasEditingFocus()) return;

    m_missionCache = m_gameData->readMissionState();
    m_updatingUI = true;

    QStandardItemModel *resM = static_cast<QStandardItemModel*>(ui->missionResourcetableView->model());
    for (int i = 0; i < resourceNames.length(); ++i)
        resM->item(i, 1)->setText(QString::number(m_missionCache.resource[i]));

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

    // 进程存活检测：目标进程已退出时自动分离，避免对已关闭进程做非法内存访问
    if (!m_memMgr->isProcessAlive()) {
        statusBar()->showMessage(tr("目标进程已退出，自动分离"));
        m_memMgr->detachProcess(); // 触发 processDetached 信号 → 停止刷新/禁用控件
        return;
    }

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