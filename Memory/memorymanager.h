#ifndef MEMORYMANAGER_H
#define MEMORYMANAGER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QVariantMap>
#include <QProcess>
#include <cstdint>

class MemoryManager : public QObject
{
    Q_OBJECT

public:
    explicit MemoryManager(QObject *parent = nullptr);
    ~MemoryManager() override;

    // 进程管理
    QList<QVariantMap> enumerateProcesses();
    bool attachProcess(const QString &processName);
    bool attachProcessById(uint32_t pid);
    void detachProcess();
    bool isAttached() const { return m_attached; }
    QString attachedProcessName() const { return m_processName; }
    uint32_t attachedProcessId() const { return m_processId; }

    // 主模块基址
    uintptr_t moduleBaseAddress() const { return m_moduleBase; }

    // 内存读写
    bool readMemory(uintptr_t address, void *buffer, size_t size) const;
    bool writeMemory(uintptr_t address, const void *buffer, size_t size) const;

    template<typename T>
    bool read(uintptr_t address, T &value) const
    {
        return readMemory(address, &value, sizeof(T));
    }

    template<typename T>
    bool write(uintptr_t address, const T &value) const
    {
        return writeMemory(address, &value, sizeof(T));
    }

    QString readString(uintptr_t address, size_t maxLen = 40) const;
    bool writeString(uintptr_t address, const QString &str, size_t maxLen = 40) const;

    bool readBytes(uintptr_t address, void *buffer, size_t size) const { return readMemory(address, buffer, size); }
    bool writeBytes(uintptr_t address, const void *buffer, size_t size) { return writeMemory(address, buffer, size); }

signals:
    void processAttached();
    void processDetached();
    void attachError(const QString &error);

private:
    void queryModuleBase();

    bool m_attached = false;
    uint32_t m_processId = 0;
    QString m_processName;
    uintptr_t m_moduleBase = 0;
    void *m_handle = nullptr; // HANDLE
};

#endif // MEMORYMANAGER_H