#ifndef ADDRSETTING_H
#define ADDRSETTING_H

#include <QDialog>
#include <QDialogButtonBox>
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

    QList<quint16> GetLength();
    quint16 GetUpdateFrequency();
    // 附加时是否把图集还原报告写入程序目录（排查 mod 图集用，默认关闭）
    bool GetSaveSheetReport() const { return SaveSheetReport; }

    // 配置文件保存/读取
    void saveToFile(const QString &filePath) const;
    void loadFromFile(const QString &filePath);

    // 语言设置
    QString GetLanguage() const { return m_language; }
    void SetLanguage(const QString &lang) { m_language = lang; }

private:
    void GetLocalLanguages();

    Ui::AddrSetting *ui;

    quint16 Charalength = 255;

    quint16 Entitylength = 610;

    quint16 Weaponlength = 1024;

    quint16 UpdateFrequency = 500;

    bool SaveSheetReport = false;

    QString m_language = "sys";

private slots:
    void AppliedSetting();
};

#endif // ADDRSETTING_H