#ifndef WEAPONDIALOG_H
#define WEAPONDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QStringList>

class WeaponDialog : public QDialog
{
    Q_OBJECT
public:
    explicit WeaponDialog(const QStringList &weaponNames, QWidget *parent = nullptr);

    int selectedWeaponIndex() const { return m_selectedIndex; }

private slots:
    void onItemDoubleClicked(QListWidgetItem *item);
    void onFilterChanged(const QString &text);

private:
    QLineEdit *m_filterEdit;
    QListWidget *m_listWidget;
    QStringList m_allNames;
    int m_selectedIndex = -1;

    void refillList(const QString &filter);
};

#endif // WEAPONDIALOG_H