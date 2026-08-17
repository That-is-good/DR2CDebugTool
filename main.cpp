#include "mainwindow.h"

#include <QApplication>
#include <QTranslator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QTranslator translator;
    QString lang;

    // 尝试从 config.json 读取语言设置
    QString configPath = QCoreApplication::applicationDirPath() + "/config.json";
    QFile file(configPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = file.readAll();
        file.close();
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error == QJsonParseError::NoError) {
            QJsonObject root = doc.object();
            lang = root.value("Language").toString();
        }
    }

    // 如果配置中没有语言设置或设置为 "sys"，则使用系统语言
    if (lang.isEmpty() || lang == "sys") {
        const QStringList uiLanguages = QLocale::system().uiLanguages();
        for (const QString &locale : uiLanguages) {
            const QString baseName = QString("./qt_%1.qm").arg(QLocale(locale).name());
            if (translator.load("./translations" + baseName)){
                a.installTranslator(&translator);
                break;
            }
        }
    } else {
        // 使用配置中的语言
        QString qmFile = QString("./qt_%1.qm").arg(lang);
        if (translator.load("./translations" + qmFile)) {
            a.installTranslator(&translator);
        }
    }

    MainWindow w;
    w.show();
    return QApplication::exec();
}