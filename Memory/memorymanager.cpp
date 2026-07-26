#include "memorymanager.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#endif

MemoryManager::MemoryManager(QObject *parent)
    : QObject(parent)
    , m_attached(false)
    , m_processId(0)
    , m_moduleBase(0)
    , m_handle(nullptr)
{
}

MemoryManager::~MemoryManager()
{
    detachProcess();
}

void MemoryManager::queryModuleBase()
{
#ifdef Q_OS_WIN
    if (!m_handle) return;
    HMODULE hMods[1024];
    DWORD cbNeeded;
    if (EnumProcessModules(m_handle, hMods, sizeof(hMods), &cbNeeded)) {
        m_moduleBase = reinterpret_cast<uintptr_t>(hMods[0]);
    }
#endif
}

QList<QVariantMap> MemoryManager::enumerateProcesses()
{
    QList<QVariantMap> processes;
#ifdef Q_OS_WIN
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return processes;

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            QVariantMap proc;
            proc["pid"] = static_cast<uint32_t>(pe32.th32ProcessID);
            proc["name"] = QString::fromWCharArray(pe32.szExeFile);
            processes.append(proc);
        } while (Process32NextW(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
#endif
    return processes;
}

bool MemoryManager::attachProcess(const QString &processName)
{
#ifdef Q_OS_WIN
    detachProcess();

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return false;

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);
    bool found = false;

    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            QString name = QString::fromWCharArray(pe32.szExeFile);
            if (name.compare(processName, Qt::CaseInsensitive) == 0) {
                m_processId = pe32.th32ProcessID;
                m_processName = name;
                found = true;
                break;
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);

    if (!found) {
        emit attachError(QString("未找到进程: %1").arg(processName));
        return false;
    }

    m_handle = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, m_processId);
    if (!m_handle) {
        emit attachError(QString("无法打开进程 %1 (PID: %2)").arg(m_processName).arg(m_processId));
        m_processId = 0;
        m_processName.clear();
        return false;
    }

    queryModuleBase();
    m_attached = true;
    emit processAttached();
    return true;
#else
    Q_UNUSED(processName);
    emit attachError("仅支持 Windows 平台");
    return false;
#endif
}

bool MemoryManager::attachProcessById(uint32_t pid)
{
#ifdef Q_OS_WIN
    detachProcess();

    m_handle = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, pid);
    if (!m_handle) {
        emit attachError(QString("无法打开进程 PID: %1").arg(pid));
        return false;
    }

    m_processId = pid;

    WCHAR exeName[MAX_PATH] = {0};
    DWORD size = MAX_PATH;
    if (QueryFullProcessImageNameW(m_handle, 0, exeName, &size)) {
        m_processName = QString::fromWCharArray(exeName);
        int lastSlash = m_processName.lastIndexOf('\\');
        if (lastSlash >= 0)
            m_processName = m_processName.mid(lastSlash + 1);
    }

    queryModuleBase();
    m_attached = true;
    emit processAttached();
    return true;
#else
    Q_UNUSED(pid);
    emit attachError("仅支持 Windows 平台");
    return false;
#endif
}

void MemoryManager::detachProcess()
{
#ifdef Q_OS_WIN
    if (m_handle) {
        CloseHandle(m_handle);
        m_handle = nullptr;
    }
#endif
    m_attached = false;
    m_processId = 0;
    m_processName.clear();
    m_moduleBase = 0;
    emit processDetached();
}

bool MemoryManager::readMemory(uintptr_t address, void *buffer, size_t size) const
{
#ifdef Q_OS_WIN
    if (!m_handle || !buffer || size == 0)
        return false;

    SIZE_T bytesRead = 0;
    return ReadProcessMemory(m_handle, reinterpret_cast<LPCVOID>(address), buffer, size, &bytesRead) && bytesRead == size;
#else
    Q_UNUSED(address);
    Q_UNUSED(buffer);
    Q_UNUSED(size);
    return false;
#endif
}

bool MemoryManager::writeMemory(uintptr_t address, const void *buffer, size_t size) const
{
#ifdef Q_OS_WIN
    if (!m_handle || !buffer || size == 0)
        return false;

    SIZE_T bytesWritten = 0;
    return WriteProcessMemory(m_handle, reinterpret_cast<LPVOID>(address), buffer, size, &bytesWritten) && bytesWritten == size;
#else
    Q_UNUSED(address);
    Q_UNUSED(buffer);
    Q_UNUSED(size);
    return false;
#endif
}

QString MemoryManager::readString(uintptr_t address, size_t maxLen) const
{
    QByteArray data(maxLen, '\0');
    if (!readMemory(address, data.data(), maxLen))
        return QString();

    int nullPos = data.indexOf('\0');
    if (nullPos >= 0)
        data.truncate(nullPos);

    return QString::fromUtf8(data);
}

bool MemoryManager::writeString(uintptr_t address, const QString &str, size_t maxLen) const
{
    QByteArray data = str.toUtf8();
    if (static_cast<size_t>(data.size()) >= maxLen)
        data = data.left(maxLen - 1);
    data.append('\0');

    return writeMemory(address, data.constData(), data.size());
}