#ifndef MEMORYMANAGER_H
#define MEMORYMANAGER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QVariantMap>
#include <QProcess>

#include "base.h"

class MemoryManager : public QObject
{
    Q_OBJECT

public:
    explicit MemoryManager(QObject *parent = nullptr);
    ~MemoryManager() override;

    // 进程管理
    QList<QVariantMap> enumerateProcesses();
    bool attachProcess(const QString&);
    bool attachProcessById(quint32);
    void detachProcess();
    bool isAttached() const { return m_attached; }
    QString attachedProcessName() const { return m_processName; }
    quint32 attachedProcessId() const { return m_processId; }

    // 主模块基址
    quint64 moduleBaseAddress() const { return m_moduleBase; }

    // 内存读写
    bool readMemory(quint64, void*, quint64) const;
    bool writeMemory(quint64, const void*, quint64) const;

    template<typename T>
    bool read(quint64 address, T &value) const
    {
        return readMemory(address, &value, sizeof(T));
    }

    template<typename T>
    bool write(quint64 address, const T &value) const
    {
        return writeMemory(address, &value, sizeof(T));
    }

    QString readString(quint64, quint64 maxLen = 40) const;
    bool writeString(quint64, const QString&, quint64 maxLen = 40) const;

    // 调用函数
    bool CallFunction(quint64, const QVector<quint64>&) const;

    bool ScriptEvaluateStringSafe(const QString&) const;
    bool FreeThing(quint64) const;
    bool AllocateEntity(qint8) const;

signals:
    void processAttached();
    void processDetached();
    void attachError(const QString&);

private:
    void queryModuleBase();

    bool m_attached = false;
    quint32 m_processId = 0;
    QString m_processName;
    quint64 m_moduleBase = 0;
    void *m_handle = nullptr; // HANDLE
};

#endif // MEMORYMANAGER_H