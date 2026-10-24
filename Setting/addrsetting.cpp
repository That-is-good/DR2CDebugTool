#include "addrsetting.h"
#include "ui_addrsetting.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>

AddrSetting::AddrSetting(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AddrSetting)
{
    ui->setupUi(this);

    ui->LanguagecomboBox->addItem("System", "sys");
    GetLocalLanguages();
    const QList<QWidget *> offsetControls{
        ui->EntityOffsetLabel, ui->EntityOffsetspinBox,
        ui->WeaponOffsetLabel, ui->WeaponOffsetspinBox,
        ui->MissonOffsetLabel, ui->MissonOffsetspinBox,
        ui->AreaOffsetLabel, ui->AreaOffsetspinBox,
        ui->CharaSizeLabel, ui->CharaSizespinBox,
        ui->EntitySizeLabel, ui->EntitySizespinBox,
        ui->WeaponSizeLabel, ui->WeaponSizespinBox,
        ui->AreaSizeLabel, ui->AreaSizespinBox
    };
    for (QWidget *offsetControl : offsetControls)
        offsetControl->hide();
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &AddrSetting::AppliedSetting);
}

void AddrSetting::GetLocalLanguages(){
    QString translationDir = QCoreApplication::applicationDirPath() + "/translations";
    QDir dir(translationDir);
    if (dir.exists()) {
        QStringList filters;
        filters << "*.qm";
        dir.setNameFilters(filters);
        QFileInfoList fileList = dir.entryInfoList(QDir::Files);
        for (const QFileInfo &fileInfo : fileList) {
            QString fileName = fileInfo.baseName(); // 如 "qt_zh_CN"
            int lastUnderscore = fileName.indexOf('_');
            if (lastUnderscore == -1)
                continue;

            QString langCode = fileName.mid(lastUnderscore + 1); // "zh_CN"

            QLocale locale(langCode);
            QString displayName = locale.languageToString(locale.language());

            if (locale.territory() != QLocale::AnyTerritory) {
                displayName += QString(" (%1)").arg(QLocale::territoryToString(locale.territory()));
            }

            ui->LanguagecomboBox->addItem(displayName, langCode);
        }
    }
}

void AddrSetting::RefreshText(){
    ui->CharaLengthspinBox->setValue(Charalength);

    ui->EntityLengthspinBox->setValue(Entitylength);

    ui->WeaponLengthspinBox->setValue(Weaponlength);

    ui->UpdateSpinBox->setValue(UpdateFrequency);

    // 设置语言选择框
    for (int i = 0; i < ui->LanguagecomboBox->count(); ++i) {
        if (ui->LanguagecomboBox->itemData(i).toString() == m_language) {
            ui->LanguagecomboBox->setCurrentIndex(i);
            break;
        }
    }
}

void AddrSetting::AppliedSetting(){
    Charalength = ui->CharaLengthspinBox->value();

    Entitylength = ui->EntityLengthspinBox->value();

    Weaponlength = ui->WeaponLengthspinBox->value();

    UpdateFrequency = ui->UpdateSpinBox->value();

    m_language = ui->LanguagecomboBox->currentData().toString();
}

QList<quint16> AddrSetting::GetLength(){
    return QList<quint16>{Entitylength, Charalength, Weaponlength};
}

quint16 AddrSetting::GetUpdateFrequency(){
    return UpdateFrequency;
}

void AddrSetting::saveToFile(const QString &filePath) const
{
    QJsonObject root;

    root["CharaLength"]    = Charalength;

    root["EntityLength"]   = Entitylength;

    root["WeaponLength"]   = Weaponlength;

    root["UpdateFrequency"] = UpdateFrequency;
    root["Language"]        = m_language;

    QJsonDocument doc(root);
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    }
}

void AddrSetting::loadFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return; // 文件不存在则使用默认值

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError)
        return;

    QJsonObject root = doc.object();

    Charalength  = static_cast<quint16>(root.value("CharaLength").toInt(255));

    Entitylength = static_cast<quint16>(root.value("EntityLength").toInt(610));

    Weaponlength = static_cast<quint16>(root.value("WeaponLength").toInt(1024));

    UpdateFrequency = static_cast<quint16>(root.value("UpdateFrequency").toInt(500));

    m_language = root.value("Language").toString("sys");

    RefreshText();
}

AddrSetting::~AddrSetting()
{
    delete ui;
}