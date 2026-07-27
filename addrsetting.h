#ifndef ADDRSETTING_H
#define ADDRSETTING_H

#include <QDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QList>
#include <QString>

namespace Ui {
class AddrSetting;
}

class AddrSetting : public QDialog
{
    Q_OBJECT

public:
    explicit AddrSetting(QWidget *parent = nullptr);
    ~AddrSetting();
    void RefreshText();

    QList<intptr_t> GetOffset();
    QList<uint> GetSize();
    QList<ushort> GetLength();
    ushort GetUpdateFrequency();

    /*
    void SetOffset(QList<intptr_t>);
    void SetSize(QList<uint>);
    void SetLength(QList<ushort>);
    void SetUpdateFrequency(ushort);
    */
   
    // 配置文件保存/读取
    void saveToFile(const QString &filePath) const;
    void loadFromFile(const QString &filePath);

private:
    Ui::AddrSetting *ui;

    intptr_t tryGetPtr(QString text, intptr_t d);
    uint tryGetSize(QString text, uint d);
    ushort tryGetLength(QString text, ushort d);

    intptr_t Charaoffset = 0x5E25D8;
    uint Charasize = 0x2E0;
    ushort Charalength = 256;

    intptr_t Entityoffset = 0x5632E0;
    uint Entitysize = 0x304;
    ushort Entitylength = 610;

    intptr_t Weaponoffset = 0x4E0080;
    uint Weaponsize = 0x1C4;
    ushort Weaponlength = 1024;

    intptr_t Missonoffset = 0x5E2238;

    ushort UpdateFrequency = 500;

private slots:
    void AppliedSetting();
};

#endif // ADDRSETTING_H