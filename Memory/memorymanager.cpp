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

bool MemoryManager::hasFunction(GameResolver::Function function) const
{
    return m_gameLayout.valid &&
        m_gameLayout.functionRvas[static_cast<size_t>(function)] != 0;
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

bool MemoryManager::resolveGameLayout()
{
    m_gameLayout = GameResolver::Layout{};
#ifdef Q_OS_WIN
    if (!m_handle || !m_moduleBase) {
        m_gameLayout.error = QStringLiteral("无法读取目标进程主模块。");
        return false;
    }

    IMAGE_DOS_HEADER dosHeader{};
    if (!readMemory(m_moduleBase, &dosHeader, sizeof(dosHeader)) ||
        dosHeader.e_magic != IMAGE_DOS_SIGNATURE || dosHeader.e_lfanew <= 0) {
        m_gameLayout.error = QStringLiteral("目标进程不是有效的 PE 模块。");
        return false;
    }

    const quint64 ntAddress = m_moduleBase + static_cast<quint32>(dosHeader.e_lfanew);
    DWORD signature = 0;
    IMAGE_FILE_HEADER fileHeader{};
    IMAGE_OPTIONAL_HEADER32 optionalHeader{};
    if (!readMemory(ntAddress, &signature, sizeof(signature)) ||
        signature != IMAGE_NT_SIGNATURE ||
        !readMemory(ntAddress + sizeof(signature), &fileHeader, sizeof(fileHeader)) ||
        !readMemory(ntAddress + sizeof(signature) + sizeof(fileHeader),
                    &optionalHeader, sizeof(optionalHeader)) ||
        fileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        optionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        optionalHeader.SizeOfImage == 0 ||
        fileHeader.NumberOfSections == 0 ||
        fileHeader.NumberOfSections > 96) {
        m_gameLayout.error = QStringLiteral("目标模块格式不受支持（需要 32 位 x86 PE）。");
        return false;
    }

    const quint64 sectionTableAddress = ntAddress + sizeof(signature) +
        sizeof(fileHeader) + fileHeader.SizeOfOptionalHeader;
    QByteArray sectionHeaders(static_cast<qsizetype>(fileHeader.NumberOfSections) *
                                  sizeof(IMAGE_SECTION_HEADER), '\0');
    if (!readMemory(sectionTableAddress, sectionHeaders.data(),
                    static_cast<quint64>(sectionHeaders.size()))) {
        m_gameLayout.error = QStringLiteral("无法读取目标模块节表。");
        return false;
    }

    IMAGE_SECTION_HEADER textSection{};
    bool foundText = false;
    for (quint16 i = 0; i < fileHeader.NumberOfSections; ++i) {
        IMAGE_SECTION_HEADER section{};
        memcpy(&section, sectionHeaders.constData() +
                    static_cast<qsizetype>(i) * sizeof(section), sizeof(section));
        if (memcmp(section.Name, ".text", 5) == 0 &&
            (section.Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0) {
            textSection = section;
            foundText = true;
            break;
        }
    }

    const quint32 sectionSize = qMax(textSection.Misc.VirtualSize, textSection.SizeOfRawData);
    if (!foundText || sectionSize == 0 || sectionSize > 32u * 1024u * 1024u ||
        static_cast<quint64>(textSection.VirtualAddress) + sectionSize >
            optionalHeader.SizeOfImage) {
        m_gameLayout.error = QStringLiteral("目标模块缺少有效的可执行代码节。");
        return false;
    }

    QByteArray executableSection(static_cast<qsizetype>(sectionSize), '\0');
    if (!readMemory(m_moduleBase + textSection.VirtualAddress, executableSection.data(),
                    sectionSize)) {
        m_gameLayout.error = QStringLiteral("无法读取目标模块代码节用于特征码解析。");
        return false;
    }

    if (!GameResolver::resolve(executableSection, textSection.VirtualAddress,
                               optionalHeader.ImageBase, optionalHeader.SizeOfImage,
                               &m_gameLayout))
        return false;

    quint32 characterPoolAddress = 0;
    if (!readMemory(m_moduleBase + m_gameLayout.characterPoolPointerRva,
                    &characterPoolAddress, sizeof(characterPoolAddress)) ||
        characterPoolAddress < m_moduleBase ||
        static_cast<quint64>(characterPoolAddress) - m_moduleBase >=
            m_gameLayout.imageSize) {
        m_gameLayout.valid = false;
        m_gameLayout.error = QStringLiteral("角色池运行时指针无效；为避免错误读写，已拒绝附加。");
        return false;
    }
    m_gameLayout.characterPoolRva =
        static_cast<quint32>(static_cast<quint64>(characterPoolAddress) - m_moduleBase);

    return true;
#else
    m_gameLayout.error = QStringLiteral("仅支持 Windows 平台。");
    return false;
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
    if (!m_moduleBase || !resolveGameLayout()) {
        const QString error = m_gameLayout.error.isEmpty()
            ? QStringLiteral("无法解析目标程序布局。") : m_gameLayout.error;
        detachProcess();
        emit attachError(error);
        return false;
    }
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
    if (!m_moduleBase || !resolveGameLayout()) {
        const QString error = m_gameLayout.error.isEmpty()
            ? QStringLiteral("无法解析目标程序布局。") : m_gameLayout.error;
        detachProcess();
        emit attachError(error);
        return false;
    }
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
    m_gameLayout = GameResolver::Layout{};
    emit processDetached();
}

bool MemoryManager::isProcessAlive() const
{
#ifdef Q_OS_WIN
    if (!m_handle) return false;
    DWORD exitCode = 0;
    if (!GetExitCodeProcess(m_handle, &exitCode))
        return false;
    // STILL_ACTIVE = 259 表示进程仍在运行
    return exitCode == STILL_ACTIVE;
#else
    return false;
#endif
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
bool MemoryManager::CallFunction(GameResolver::Function function,
                                 const QVector<quint64> &args,
                                 quint64 *retValue) const
{
#ifdef Q_OS_WIN
    if (!m_handle || !m_gameLayout.valid)
        return false;

    const quint32 funcRva = m_gameLayout.functionRvas[static_cast<size_t>(function)];
    if (!funcRva)
        return false;

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
    Q_UNUSED(function);
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

    bool result = CallFunction(GameResolver::Function::ScriptEvaluateStringSafe, args);

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
    
    return CallFunction(GameResolver::Function::FreeThing, args);
#else
    Q_UNUSED(curThingPtr);
    return false;
#endif
}

quint64 MemoryManager::AllocateEntity(qint8 type) const
{
#ifdef Q_OS_WIN
    if (!m_handle) return 0;
    quint64 thingPtr = 0;
    QVector<quint64> args;
    args.append(type);
    
    if (!CallFunction(GameResolver::Function::AllocateEntity, args, &thingPtr))
        return 0;
    return thingPtr;
#else
    Q_UNUSED(type);
    return 0;
#endif
}

quint64 MemoryManager::AllocateThing(qint8 subtype) const
{
#ifdef Q_OS_WIN
    if (!m_handle) return 0;
    quint64 thingPtr = 0;
    QVector<quint64> args;
    args.append(subtype);
    
    if (!CallFunction(GameResolver::Function::AllocateThing, args, &thingPtr))
        return 0;
    return thingPtr;
#else
    Q_UNUSED(subtype);
    return 0;
#endif
}

quint32 MemoryManager::AllocateCharacterSlot() const
{
#ifdef Q_OS_WIN
    if (!m_handle) return 0;
    quint64 characterPtr = 0;
    if (!CallFunction(GameResolver::Function::AllocateCharacterSlot,
                      QVector<quint64>(), &characterPtr))
        return 0;
    return static_cast<quint32>(characterPtr);
#else
    return 0;
#endif
}

bool MemoryManager::Assigncharactertothing(quint64 addr, quint32 charid) const{
#ifdef Q_OS_WIN
    if (!m_handle) return false;
    QVector<quint64> args;
    args.append(addr);
    args.append(charid);
    
    return CallFunction(GameResolver::Function::AssignCharacterToThing, args);
#else
    Q_UNUSED(addr);
    Q_UNUSED(charid);
    return false;
#endif
}

bool MemoryManager::SwitchActiveTileLayer(quint8 mapid) const{
#ifdef Q_OS_WIN
    if (!m_handle) return false;
    QVector<quint64> args;
    args.append(mapid);
    
    return CallFunction(GameResolver::Function::SwitchActiveTileLayer, args);
#else
    Q_UNUSED(mapid);
    return false;
#endif
}

bool MemoryManager::SetCurrentPlayerThing(quint64 curThingPtr) const
{
#ifdef Q_OS_WIN
    if (!m_handle) return false;
    QVector<quint64> args;
    args.append(curThingPtr);
    
    return CallFunction(GameResolver::Function::SetCurrentPlayerThing, args);
#else
    Q_UNUSED(curThingPtr);
    return false;
#endif
}

// bool MemoryManager::RecruitCharacter(quint32 charId, quint8 location) const
// {
// #ifdef Q_OS_WIN
//     if (!m_handle) return false;
//     QVector<quint64> args;
//     args.append(charId);
//     args.append(location);
    
//     return CallFunction(0x0492f0, args);
// #else
//     Q_UNUSED(charId);
//     Q_UNUSED(location);
//     return false;
// #endif
// }
