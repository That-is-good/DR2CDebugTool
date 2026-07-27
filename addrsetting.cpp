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

    ui->LanguagecomboBox->addItem("English", "en");
    ui->LanguagecomboBox->addItem("中文", "zh-cn");
    ui->LanguagecomboBox->addItem("日本語", "ja");

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &AddrSetting::AppliedSetting);

    setWindowTitle("地址设置");
}

intptr_t AddrSetting::tryGetPtr(QString text, intptr_t d){
    bool isSucc = false;
    intptr_t r = text.toULongLong(&isSucc, 16);
    if (!isSucc)
    {
        return d;
    }
    return r;
}
uint AddrSetting::tryGetSize(QString text, uint d){
    bool isSucc = false;
    intptr_t r = text.toUInt(&isSucc, 16);
    if (!isSucc)
    {
        return d;
    }
    return r;
}
ushort AddrSetting::tryGetLength(QString text, ushort d){
    bool isSucc = false;
    intptr_t r = text.toULongLong(&isSucc);
    if (!isSucc)
    {
        return d;
    }
    return r;
}

void AddrSetting::RefreshText(){
    ui->CharaOffsetplainTextEdit->setPlainText(QString::number(Charaoffset, 16));
    ui->CharaSizeplainTextEdit->setPlainText(QString::number(Charasize, 16));
    ui->CharaLengthplainTextEdit->setPlainText(QString::number(Charalength));

    ui->EntityOffsetplainTextEdit->setPlainText(QString::number(Entityoffset, 16));
    ui->EntitySizeplainTextEdit->setPlainText(QString::number(Entitysize, 16));
    ui->EntityLengthplainTextEdit->setPlainText(QString::number(Entitylength));

    ui->WeaponOffsetplainTextEdit->setPlainText(QString::number(Weaponoffset, 16));
    ui->WeaponSizeplainTextEdit->setPlainText(QString::number(Weaponsize, 16));
    ui->WeaponLengthplainTextEdit->setPlainText(QString::number(Weaponlength));

    ui->MissonOffsetplainTextEdit->setPlainText(QString::number(Missonoffset, 16));

    ui->UpdateSpinBox->setValue(UpdateFrequency);
}

void AddrSetting::AppliedSetting(){
    Charaoffset = tryGetPtr(ui->CharaOffsetplainTextEdit->toPlainText(), 0x5E25D8);
    Charasize = tryGetSize(ui->CharaSizeplainTextEdit->toPlainText(), 0x2E0);
    Charalength = tryGetLength(ui->CharaLengthplainTextEdit->toPlainText(), 256);

    Entityoffset = tryGetPtr(ui->EntityOffsetplainTextEdit->toPlainText(), 0x5632E0);
    Entitysize = tryGetSize(ui->EntitySizeplainTextEdit->toPlainText(), 0x304);
    Entitylength = tryGetLength(ui->EntityLengthplainTextEdit->toPlainText(), 610);

    Weaponoffset = tryGetPtr(ui->WeaponOffsetplainTextEdit->toPlainText(), 0x4E0080);
    Weaponsize = tryGetSize(ui->WeaponSizeplainTextEdit->toPlainText(), 0x1C4);
    Weaponlength = tryGetLength(ui->WeaponLengthplainTextEdit->toPlainText(), 1024);

    Missonoffset = tryGetPtr(ui->MissonOffsetplainTextEdit->toPlainText(), 0x5E2238);

    UpdateFrequency = ui->UpdateSpinBox->value();
}

QList<intptr_t> AddrSetting::GetOffset(){
    return QList<intptr_t>{Entityoffset, Charaoffset, Weaponoffset, Missonoffset};
}

QList<uint> AddrSetting::GetSize(){
    return QList<uint>{Entitysize, Charasize, Weaponsize};
}

QList<ushort> AddrSetting::GetLength(){
    return QList<ushort>{Entitylength, Charalength, Weaponlength};
}

ushort AddrSetting::GetUpdateFrequency(){
    return UpdateFrequency;
}

/*
void AddrSetting::SetOffset(QList<intptr_t> offset){
    Charaoffset = offset[0];
    Entityoffset = offset[1];
    Weaponoffset = offset[2];
    Missonoffset = offset[3];
}

void AddrSetting::SetSize(QList<uint> size){
    Charasize = size[0];
    Entitysize = size[1];
    Weaponsize = size[2];
}

void AddrSetting::SetLength(QList<ushort> length){
    Charalength = length[0];
    Entitylength = length[1];
    Weaponlength = length[2];
}

void AddrSetting::SetUpdateFrequency(ushort freq){
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

    auto readHex = [&](const QString &key, intptr_t defaultVal) -> intptr_t {
        QString s = root.value(key).toString();
        if (s.isEmpty()) return defaultVal;
        return s.toULongLong(nullptr, 16);
    };

    Charaoffset  = readHex("CharaOffset",  0x5E25D8);
    Charasize    = static_cast<uint>(readHex("CharaSize", 0x2E0));
    Charalength  = static_cast<ushort>(root.value("CharaLength").toInt(256));

    Entityoffset = readHex("EntityOffset", 0x5632E0);
    Entitysize   = static_cast<uint>(readHex("EntitySize", 0x304));
    Entitylength = static_cast<ushort>(root.value("EntityLength").toInt(610));

    Weaponoffset = readHex("WeaponOffset", 0x4E0080);
    Weaponsize   = static_cast<uint>(readHex("WeaponSize", 0x1C4));
    Weaponlength = static_cast<ushort>(root.value("WeaponLength").toInt(1024));

    Missonoffset = readHex("MissionOffset", 0x5E2238);

    UpdateFrequency = static_cast<ushort>(root.value("UpdateFrequency").toInt(500));

    RefreshText();
}

AddrSetting::~AddrSetting()
{
    delete ui;
}