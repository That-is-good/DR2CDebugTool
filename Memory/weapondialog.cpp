#include "weapondialog.h"
#include <QVBoxLayout>
#include <QListWidgetItem>
#include <QLabel>

WeaponDialog::WeaponDialog(const QStringList &weaponNames, QWidget *parent)
    : QDialog(parent)
    , m_allNames(weaponNames)
{
    setWindowTitle("选择武器");
    setMinimumSize(400, 500);

    QVBoxLayout *layout = new QVBoxLayout(this);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText("输入名称或ID筛选...");
    layout->addWidget(new QLabel("筛选:", this));
    layout->addWidget(m_filterEdit);

    m_listWidget = new QListWidget(this);
    layout->addWidget(m_listWidget);

    refillList(QString());

    connect(m_filterEdit, &QLineEdit::textChanged, this, &WeaponDialog::onFilterChanged);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &WeaponDialog::onItemDoubleClicked);
}

void WeaponDialog::refillList(const QString &filter)
{
    m_listWidget->clear();
    for (int i = 0; i < m_allNames.size(); ++i) {
        QString display = QString("[%1] %2").arg(i).arg(m_allNames[i]);
        if (!filter.isEmpty()) {
            // 同时匹配索引和名称
            bool matchIdx = QString::number(i).contains(filter);
            bool matchName = m_allNames[i].contains(filter, Qt::CaseInsensitive);
            if (!matchIdx && !matchName)
                continue;
        }
        QListWidgetItem *item = new QListWidgetItem(display);
        item->setData(Qt::UserRole, i);
        m_listWidget->addItem(item);
    }
}

void WeaponDialog::onFilterChanged(const QString &text)
{
    refillList(text);
}

void WeaponDialog::onItemDoubleClicked(QListWidgetItem *item)
{
    if (item) {
        m_selectedIndex = item->data(Qt::UserRole).toInt();
        accept();
    }
}