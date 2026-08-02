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
        m_moduleBase = reinterpret_cast<quint64>(hMods[0]);
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
            proc["pid"] = static_cast<quint32>(pe32.th32ProcessID);
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

    m_handle = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION
                           | PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION,
                           FALSE, m_processId);
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

bool MemoryManager::attachProcessById(quint32 pid)
{
#ifdef Q_OS_WIN
    detachProcess();

    m_handle = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION
                           | PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION,
                           FALSE, pid);
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

bool MemoryManager::readMemory(quint64 address, void *buffer, quint64 size) const
{
#ifdef Q_OS_WIN
    if (!m_handle || !buffer || size == 0)
        return false;

    quint64 bytesRead = 0;
    return ReadProcessMemory(m_handle, reinterpret_cast<LPCVOID>(address), buffer, size, &bytesRead) && bytesRead == size;
#else
    Q_UNUSED(address);
    Q_UNUSED(buffer);
    Q_UNUSED(size);
    return false;
#endif
}

bool MemoryManager::writeMemory(quint64 address, const void *buffer, quint64 size) const
{
#ifdef Q_OS_WIN
    if (!m_handle || !buffer || size == 0)
        return false;

    quint64 bytesWritten = 0;
    return WriteProcessMemory(m_handle, reinterpret_cast<LPVOID>(address), buffer, size, &bytesWritten) && bytesWritten == size;
#else
    Q_UNUSED(address);
    Q_UNUSED(buffer);
    Q_UNUSED(size);
    return false;
#endif
}

QString MemoryManager::readString(quint64 address, quint64 maxLen) const
{
    QByteArray data(maxLen, '\0');
    if (!readMemory(address, data.data(), maxLen))
        return QString();

    int nullPos = data.indexOf('\0');
    if (nullPos >= 0)
        data.truncate(nullPos);

    return QString::fromUtf8(data);
}

bool MemoryManager::writeString(quint64 address, const QString &str, quint64 maxLen) const
{
    QByteArray data = str.toUtf8();
    if (static_cast<quint64>(data.size()) >= maxLen)
        data = data.left(maxLen - 1);
    data.append('\0');

    return writeMemory(address, data.constData(), data.size());
}

// ==================== 远程内存分配/释放 ====================
bool MemoryManager::AllocateRemoteString(const QString &str, quint64 *remoteAddr) const
{
#ifdef Q_OS_WIN
    if (!m_handle || !remoteAddr) return false;

    // 将字符串转为 UTF-8 并确保以 '\0' 结尾
    QByteArray data = str.toUtf8();
    if (data.isEmpty()) return false;
    if (!data.endsWith('\0'))
        data.append('\0');
    quint32 size = static_cast<quint32>(data.size());

    // 在目标进程中分配内存用于存放字符串
    void *remoteMem = VirtualAllocEx(m_handle, nullptr, size,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMem) return false;

    quint64 bytesWritten = 0;
    if (!WriteProcessMemory(m_handle, remoteMem, data.constData(), size, &bytesWritten)
            || bytesWritten != size) {
        VirtualFreeEx(m_handle, remoteMem, 0, MEM_RELEASE);
        return false;
    }

    *remoteAddr = reinterpret_cast<quint64>(remoteMem);
    return true;
#else
    Q_UNUSED(str);
    Q_UNUSED(remoteAddr);
    return false;
#endif
}

bool MemoryManager::FreeRemoteMemory(quint64 remoteAddr) const
{
#ifdef Q_OS_WIN
    if (!m_handle || remoteAddr == 0) return false;

    return VirtualFreeEx(m_handle, reinterpret_cast<LPVOID>(remoteAddr), 0, MEM_RELEASE) != 0;
#else
    Q_UNUSED(remoteAddr);
    return false;
#endif
}

// ==================== 通用远程函数调用 ====================
bool MemoryManager::CallFunction(quint64 funcRva, const QVector<quint64> &args, quint64 *retValue) const
{
#ifdef Q_OS_WIN
    if (!m_handle) return false;

    quint64 funcAddr = m_moduleBase + funcRva;
    int argCount = args.size();

    // 判断目标进程是 32 位还是 64 位
    BOOL isWow64 = FALSE;
    bool isTarget32Bit = false;
    if (IsWow64Process(m_handle, &isWow64)) {
        isTarget32Bit = (isWow64 == TRUE);
    }

    // 存储远程分配的内存，用于后续释放
    QVector<void*> remoteAllocs;

    // 如果需要返回值，在目标进程中分配 8 字节内存用于保存 EAX/RAX
    void *retStorage = nullptr;
    if (retValue) {
        retStorage = VirtualAllocEx(m_handle, nullptr, sizeof(quint64),
                                    MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!retStorage)
            return false;
        remoteAllocs.append(retStorage);
    }

    QByteArray shellcode;
    if (isTarget32Bit) {
        // ---- x86 (32-bit) cdecl: 参数从右到左 push，调用者清理栈 ----
        for (int i = argCount - 1; i >= 0; --i) {
            // push arg
            shellcode.append(static_cast<char>(0x68));
            quint64 arg = args[i];
            shellcode.append(reinterpret_cast<const char*>(&arg), 4);
        }
        // mov eax, funcAddr
        shellcode.append(static_cast<char>(0xB8));
        shellcode.append(reinterpret_cast<const char*>(&funcAddr), 4);
        // call eax
        shellcode.append(static_cast<char>(0xFF));
        shellcode.append(static_cast<char>(0xD0));
        // add esp, 4*argCount   (cdecl 调用者清理栈)
        if (argCount > 0) {
            quint32 stackClean = static_cast<quint32>(4 * argCount);
            shellcode.append(static_cast<char>(0x81));
            shellcode.append(static_cast<char>(0xC4));
            shellcode.append(reinterpret_cast<const char*>(&stackClean), 4);
        }
        // 如果请求了返回值，将 EAX 保存到远程内存 retStorage
        if (retValue && retStorage) {
            // mov edi, retStorage
            shellcode.append(static_cast<char>(0xBF));
            quint32 retStorage32 = static_cast<quint32>(reinterpret_cast<quint64>(retStorage));
            shellcode.append(reinterpret_cast<const char*>(&retStorage32), 4);
            // mov [edi], eax
            shellcode.append(static_cast<char>(0x89));
            shellcode.append(static_cast<char>(0x07));
        }
        // ret
        shellcode.append(static_cast<char>(0xC3));
    } else {
        // ---- x64 (64-bit) Windows calling convention ----
        // 前4个参数: RCX, RDX, R8, R9
        // 第5+个参数: 栈上 [rsp+0x28], [rsp+0x30], ...
        // 必须预留 shadow space (0x20)，栈16字节对齐

        // 计算栈参数数量
        int stackArgCount = (argCount > 4) ? (argCount - 4) : 0;
        // 栈空间 = shadow space (0x20) + 栈参数 (每个8字节)
        quint32 allocSize = 0x20 + static_cast<quint32>(stackArgCount * 8);
        // 16字节对齐
        if (allocSize % 16 != 0)
            allocSize = (allocSize + 15) & ~15u;

        // sub rsp, allocSize
        if (allocSize >= 0x80) {
            shellcode.append(static_cast<char>(0x48));
            shellcode.append(static_cast<char>(0x81));
            shellcode.append(static_cast<char>(0xEC));
            shellcode.append(reinterpret_cast<const char*>(&allocSize), 4);
        } else {
            shellcode.append(static_cast<char>(0x48));
            shellcode.append(static_cast<char>(0x83));
            shellcode.append(static_cast<char>(0xEC));
            shellcode.append(static_cast<char>(allocSize & 0xFF));
        }

        // 前4个参数: RCX, RDX, R8, R9
        // arg0 -> RCX
        if (argCount >= 1) {
            shellcode.append(static_cast<char>(0x48));
            shellcode.append(static_cast<char>(0xB9));
            shellcode.append(reinterpret_cast<const char*>(&args[0]), 8);
        }
        // arg1 -> RDX
        if (argCount >= 2) {
            shellcode.append(static_cast<char>(0x48));
            shellcode.append(static_cast<char>(0xBA));
            shellcode.append(reinterpret_cast<const char*>(&args[1]), 8);
        }
        // arg2 -> R8
        if (argCount >= 3) {
            shellcode.append(static_cast<char>(0x49));
            shellcode.append(static_cast<char>(0xB8));
            shellcode.append(reinterpret_cast<const char*>(&args[2]), 8);
        }
        // arg3 -> R9
        if (argCount >= 4) {
            shellcode.append(static_cast<char>(0x49));
            shellcode.append(static_cast<char>(0xB9));
            shellcode.append(reinterpret_cast<const char*>(&args[3]), 8);
        }

        // 第5+个参数: 放到 [rsp+0x20], [rsp+0x28], ... 使用 mov rax, imm64; mov [rsp+off], rax
        for (int i = 4; i < argCount; ++i) {
            int stackOffset = 0x20 + (i - 4) * 8;
            // mov rax, args[i]
            shellcode.append(static_cast<char>(0x48));
            shellcode.append(static_cast<char>(0xB8));
            shellcode.append(reinterpret_cast<const char*>(&args[i]), 8);
            // mov [rsp+stackOffset], rax
            if (stackOffset < 0x80) {
                shellcode.append(static_cast<char>(0x48));
                shellcode.append(static_cast<char>(0x89));
                shellcode.append(static_cast<char>(0x44));
                shellcode.append(static_cast<char>(0x24));
                shellcode.append(static_cast<char>(stackOffset & 0xFF));
            } else {
                shellcode.append(static_cast<char>(0x48));
                shellcode.append(static_cast<char>(0x89));
                shellcode.append(static_cast<char>(0x84));
                shellcode.append(static_cast<char>(0x24));
                quint32 off = static_cast<quint32>(stackOffset);
                shellcode.append(reinterpret_cast<const char*>(&off), 4);
            }
        }

        // mov rax, funcAddr
        shellcode.append(static_cast<char>(0x48));
        shellcode.append(static_cast<char>(0xB8));
        shellcode.append(reinterpret_cast<const char*>(&funcAddr), 8);
        // call rax
        shellcode.append(static_cast<char>(0xFF));
        shellcode.append(static_cast<char>(0xD0));

        // add rsp, allocSize
        if (allocSize >= 0x80) {
            shellcode.append(static_cast<char>(0x48));
            shellcode.append(static_cast<char>(0x81));
            shellcode.append(static_cast<char>(0xC4));
            shellcode.append(reinterpret_cast<const char*>(&allocSize), 4);
        } else {
            shellcode.append(static_cast<char>(0x48));
            shellcode.append(static_cast<char>(0x83));
            shellcode.append(static_cast<char>(0xC4));
            shellcode.append(static_cast<char>(allocSize & 0xFF));
        }
        // 如果请求了返回值，将 RAX 保存到远程内存 retStorage
        if (retValue && retStorage) {
            // mov rdi, retStorage
            shellcode.append(static_cast<char>(0x48));
            shellcode.append(static_cast<char>(0xBF));
            quint64 retStorage64 = reinterpret_cast<quint64>(retStorage);
            shellcode.append(reinterpret_cast<const char*>(&retStorage64), 8);
            // mov [rdi], rax
            shellcode.append(static_cast<char>(0x48));
            shellcode.append(static_cast<char>(0x89));
            shellcode.append(static_cast<char>(0x07));
        }
        // ret
        shellcode.append(static_cast<char>(0xC3));
    }

    // 在目标进程中分配可执行内存用于 shellcode
    void *remoteCode = VirtualAllocEx(m_handle, nullptr, shellcode.size(),
                                      MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!remoteCode) {
        for (void *p : remoteAllocs) VirtualFreeEx(m_handle, p, 0, MEM_RELEASE);
        return false;
    }
    remoteAllocs.append(remoteCode);

    // 写入 shellcode
    quint64 codeWritten = 0;
    BOOL wroteCode = WriteProcessMemory(m_handle, remoteCode, shellcode.constData(),
                                        shellcode.size(), &codeWritten);
    if (!wroteCode || codeWritten != static_cast<quint64>(shellcode.size())) {
        for (void *p : remoteAllocs) VirtualFreeEx(m_handle, p, 0, MEM_RELEASE);
        return false;
    }

    // 创建远程线程执行 shellcode
    HANDLE hThread = CreateRemoteThread(m_handle, nullptr, 0,
                                        reinterpret_cast<LPTHREAD_START_ROUTINE>(remoteCode),
                                        nullptr, 0, nullptr);
    if (!hThread) {
        for (void *p : remoteAllocs) VirtualFreeEx(m_handle, p, 0, MEM_RELEASE);
        return false;
    }

    // 等待线程完成
    WaitForSingleObject(hThread, INFINITE);

    // 读取返回值
    if (retValue && retStorage) {
        quint64 ret = 0;
        quint64 bytesRead = 0;
        if (!ReadProcessMemory(m_handle, retStorage, &ret, sizeof(ret), &bytesRead)
                || bytesRead != sizeof(ret)) {
            CloseHandle(hThread);
            for (void *p : remoteAllocs) VirtualFreeEx(m_handle, p, 0, MEM_RELEASE);
            return false;
        }
        *retValue = ret;
    }

    // 清理
    CloseHandle(hThread);
    for (void *p : remoteAllocs) VirtualFreeEx(m_handle, p, 0, MEM_RELEASE);

    return true;
#else
    Q_UNUSED(funcRva);
    Q_UNUSED(args);
    Q_UNUSED(retValue);
    return false;
#endif
}

bool MemoryManager::ScriptEvaluateStringSafe(const QString &script) const
{
#ifdef Q_OS_WIN
    if (!m_handle) return false;

    // 在目标进程中分配并写入脚本字符串
    quint64 remoteString = 0;
    if (!AllocateRemoteString(script, &remoteString))
        return false;

    QVector<quint64> args;
    args.append(remoteString);

    bool result = CallFunction(0x08FB60, args);

    // 释放远程字符串内存
    FreeRemoteMemory(remoteString);

    return result;
#else
    Q_UNUSED(script);
    return false;
#endif
}

bool MemoryManager::FreeThing(quint64 curThingPtr) const
{
#ifdef Q_OS_WIN
    if (!m_handle) return false;
    QVector<quint64> args;
    args.append(curThingPtr);
    
    return CallFunction(0x052aa0, args);
#else
    Q_UNUSED(curThingPtr);
    return false;
#endif
}

bool MemoryManager::AllocateEntity(qint8 type) const
{
#ifdef Q_OS_WIN
    if (!m_handle) return false;
    QVector<quint64> args;
    args.append(type);
    
    return CallFunction(0x052710, args);
#else
    Q_UNUSED(type);
    return false;
#endif
}

bool MemoryManager::AllocateThing(qint8 subtype) const{
#ifdef Q_OS_WIN
    if (!m_handle) return false;
    QVector<quint64> args;
    args.append(subtype);
    
    return CallFunction(0x06deb0, args);
#else
    Q_UNUSED(subtype);
    return false;
#endif
}

quint32 MemoryManager::AllocateCharacterSlot() const
{
#ifdef Q_OS_WIN
    if (!m_handle) return false;
    quint64 characterPtr;
    if (!CallFunction(0x029a10, QVector<quint64>(), &characterPtr)){
        return -1;
    }
    return static_cast<quint32>(characterPtr);
#else
    return -1;
#endif
}

bool MemoryManager::Assigncharactertothing(quint64 addr, quint32 charid) const{
#ifdef Q_OS_WIN
    if (!m_handle) return false;
    QVector<quint64> args;
    args.append(addr);
    args.append(charid);
    
    return CallFunction(0x05c440, args);
#else
    Q_UNUSED(addr);
    Q_UNUSED(charid);
    return false;
#endif
}