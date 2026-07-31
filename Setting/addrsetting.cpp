#include "addrsetting.h"
#include "ui_addrsetting.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

AddrSetting::AddrSetting(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AddrSetting)
{
    ui->setupUi(this);

    ui->LanguagecomboBox->addItem("System", "sys");
    ui->LanguagecomboBox->addItem("English", "en");
    ui->LanguagecomboBox->addItem("中文", "zh-cn");
    ui->LanguagecomboBox->addItem("日本語", "ja");

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &AddrSetting::AppliedSetting);

    setWindowTitle("地址设置");
}

void AddrSetting::RefreshText(){
    ui->CharaOffsetspinBox->setValue(Charaoffset);
    ui->CharaSizespinBox->setValue(Charasize);
    ui->CharaLengthspinBox->setValue(Charalength);

    ui->EntityOffsetspinBox->setValue(Entityoffset);
    ui->EntitySizespinBox->setValue(Entitysize);
    ui->EntityLengthspinBox->setValue(Entitylength);

    ui->WeaponOffsetspinBox->setValue(Weaponoffset);
    ui->WeaponSizespinBox->setValue(Weaponsize);
    ui->WeaponLengthspinBox->setValue(Weaponlength);

    ui->MissonOffsetspinBox->setValue(Missonoffset);

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
    Charaoffset = ui->CharaOffsetspinBox->value();
    Charasize = ui->CharaSizespinBox->value();
    Charalength = ui->CharaLengthspinBox->value();

    Entityoffset = ui->EntityOffsetspinBox->value();
    Entitysize = ui->EntitySizespinBox->value();
    Entitylength = ui->EntityLengthspinBox->value();

    Weaponoffset = ui->WeaponOffsetspinBox->value();
    Weaponsize = ui->WeaponSizespinBox->value();
    Weaponlength = ui->WeaponLengthspinBox->value();

    Missonoffset = ui->MissonOffsetspinBox->value();

    UpdateFrequency = ui->UpdateSpinBox->value();

    m_language = ui->LanguagecomboBox->currentData().toString();
}

QList<quint64> AddrSetting::GetOffset(){
    return QList<quint64>{Entityoffset, Charaoffset, Weaponoffset, Missonoffset};
}

QList<quint32> AddrSetting::GetSize(){
    return QList<quint32>{Entitysize, Charasize, Weaponsize};
}

QList<quint16> AddrSetting::GetLength(){
    return QList<quint16>{Entitylength, Charalength, Weaponlength};
}

quint16 AddrSetting::GetUpdateFrequency(){
    return UpdateFrequency;
}

/*
void AddrSetting::SetOffset(QList<quint64> offset){
    Charaoffset = offset[0];
    Entityoffset = offset[1];
    Weaponoffset = offset[2];
    Missonoffset = offset[3];
}

void AddrSetting::SetSize(QList<quint32> size){
    Charasize = size[0];
    Entitysize = size[1];
    Weaponsize = size[2];
}

void AddrSetting::SetLength(QList<quint16> length){
    Charalength = length[0];
    Entitylength = length[1];
    Weaponlength = length[2];
}

void AddrSetting::SetUpdateFrequency(quint16 freq){
    UpdateFrequency = freq;
}
*/

void AddrSetting::saveToFile(const QString &filePath) const
{
    QJsonObject root;

    root["CharaOffset"]    = QString("0x%1").arg(Charaoffset, 0, 16);
    root["CharaSize"]      = QString("0x%1").arg(Charasize, 0, 16);
    root["CharaLength"]    = Charalength;

    root["EntityOffset"]   = QString("0x%1").arg(Entityoffset, 0, 16);
    root["EntitySize"]     = QString("0x%1").arg(Entitysize, 0, 16);
    root["EntityLength"]   = Entitylength;

    root["WeaponOffset"]   = QString("0x%1").arg(Weaponoffset, 0, 16);
    root["WeaponSize"]     = QString("0x%1").arg(Weaponsize, 0, 16);
    root["WeaponLength"]   = Weaponlength;

    root["MissionOffset"]  = QString("0x%1").arg(Missonoffset, 0, 16);
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

    auto readHex = [&](const QString &key, quint64 defaultVal) -> quint64 {
        QString s = root.value(key).toString();
        if (s.isEmpty()) return defaultVal;
        return s.toULongLong(nullptr, 16);
    };

    Charaoffset  = readHex("CharaOffset",  0x5E25D8);
    Charasize    = static_cast<quint32>(readHex("CharaSize", 0x2E0));
    Charalength  = static_cast<quint16>(root.value("CharaLength").toInt(256));

    Entityoffset = readHex("EntityOffset", 0x5632E0);
    Entitysize   = static_cast<quint32>(readHex("EntitySize", 0x304));
    Entitylength = static_cast<quint16>(root.value("EntityLength").toInt(610));

    Weaponoffset = readHex("WeaponOffset", 0x4E0080);
    Weaponsize   = static_cast<quint32>(readHex("WeaponSize", 0x1C4));
    Weaponlength = static_cast<quint16>(root.value("WeaponLength").toInt(1024));

    Missonoffset = readHex("MissionOffset", 0x5E2238);

    UpdateFrequency = static_cast<quint16>(root.value("UpdateFrequency").toInt(500));

    m_language = root.value("Language").toString("sys");

    RefreshText();
}

AddrSetting::~AddrSetting()
{
    delete ui;
}