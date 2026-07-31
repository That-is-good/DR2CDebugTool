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

    QList<quint64> GetOffset();
    QList<quint32> GetSize();
    QList<quint16> GetLength();
    quint16 GetUpdateFrequency();

    /*
    void SetOffset(QList<quint64>);
    void SetSize(QList<quint32>);
    void SetLength(QList<quint16>);
    void SetUpdateFrequency(quint16);
    */
   
    // 配置文件保存/读取
    void saveToFile(const QString &filePath) const;
    void loadFromFile(const QString &filePath);

    // 语言设置
    QString GetLanguage() const { return m_language; }
    void SetLanguage(const QString &lang) { m_language = lang; }

private:
    Ui::AddrSetting *ui;

    quint64 Charaoffset = 0x5E25D8;
    quint32 Charasize = 0x2E0;
    quint16 Charalength = 256;

    quint64 Entityoffset = 0x5632E0;
    quint32 Entitysize = 0x304;
    quint16 Entitylength = 610;

    quint64 Weaponoffset = 0x4E0080;
    quint32 Weaponsize = 0x1C4;
    quint16 Weaponlength = 1024;

    quint64 Missonoffset = 0x5E2238;

    quint16 UpdateFrequency = 500;

    QString m_language = "sys";

private slots:
    void AppliedSetting();
};

#endif // ADDRSETTING_H