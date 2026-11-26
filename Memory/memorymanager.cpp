#include "memorymanager.h"

#include "../Common/protocol.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>

#include <algorithm>
#include <cstring>
#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <cwchar>
#endif

namespace {

#ifdef Q_OS_WIN

constexpr DWORD kProcessAccess =
    PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
    PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ;

// 定义见下方“注入失败诊断”，此处先声明以前面的模块解析复用。
QString describeWin32Failure(const QString &stage, DWORD code);

bool readProcess(HANDLE process, quint32 address, void *buffer, SIZE_T size)
{
    SIZE_T done = 0;
    return ReadProcessMemory(process,
                             reinterpret_cast<LPCVOID>(static_cast<quintptr>(address)),
                             buffer, size, &done) != FALSE
           && done == size;
}

// 读取目标进程内 32 位 kernel32 的导出表，定位 LoadLibraryW 的真实地址。
// 64 位前端不能直接使用自己 kernel32 的地址，必须按目标进程的模块布局解析。
bool resolveRemoteLoadLibrary(HANDLE process, DWORD pid, quint64 *address, QString *error)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) {
        const DWORD code = GetLastError();
        *error = describeWin32Failure(
            QCoreApplication::translate("MemoryManager", "枚举目标进程模块（CreateToolhelp32Snapshot）"),
            code);
        return false;
    }

    quint32 kernel32Base = 0;
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Module32FirstW(snapshot, &entry)) {
        do {
            const quintptr base = reinterpret_cast<quintptr>(entry.modBaseAddr);
            // WOW64 进程里同时存在 32 位与 64 位 kernel32；只认 32 位那份
            if (base > 0xFFFFFFFFull)
                continue;
            if (lstrcmpiW(entry.szModule, L"kernel32.dll") == 0) {
                kernel32Base = static_cast<quint32>(base);
                break;
            }
        } while (Module32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);

    if (!kernel32Base) {
        *error = QStringLiteral("目标进程中找不到 kernel32.dll。");
        return false;
    }

    IMAGE_DOS_HEADER dos{};
    if (!readProcess(process, kernel32Base, &dos, sizeof(dos)) ||
        dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0) {
        *error = QStringLiteral("无法读取目标 kernel32 的 PE 头。");
        return false;
    }

    IMAGE_NT_HEADERS32 nt{};
    if (!readProcess(process, kernel32Base + static_cast<quint32>(dos.e_lfanew),
                     &nt, sizeof(nt)) ||
        nt.Signature != IMAGE_NT_SIGNATURE ||
        nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        *error = QStringLiteral("目标 kernel32 不是有效的 32 位 PE。");
        return false;
    }

    const IMAGE_DATA_DIRECTORY directory =
        nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (directory.VirtualAddress == 0 || directory.Size == 0 ||
        directory.Size > nt.OptionalHeader.SizeOfImage) {
        *error = QStringLiteral("目标 kernel32 缺少导出表。");
        return false;
    }

    IMAGE_EXPORT_DIRECTORY exports{};
    if (!readProcess(process, kernel32Base + directory.VirtualAddress,
                     &exports, sizeof(exports)) ||
        exports.NumberOfNames == 0 || exports.NumberOfNames > 8192 ||
        exports.NumberOfFunctions == 0 || exports.NumberOfFunctions > 65536) {
        *error = QStringLiteral("目标 kernel32 导出表异常。");
        return false;
    }

    std::vector<DWORD> nameRvas(exports.NumberOfNames);
    std::vector<WORD> ordinals(exports.NumberOfNames);
    std::vector<DWORD> functionRvas(exports.NumberOfFunctions);
    if (!readProcess(process, kernel32Base + exports.AddressOfNames,
                     nameRvas.data(), exports.NumberOfNames * sizeof(DWORD)) ||
        !readProcess(process, kernel32Base + exports.AddressOfNameOrdinals,
                     ordinals.data(), exports.NumberOfNames * sizeof(WORD)) ||
        !readProcess(process, kernel32Base + exports.AddressOfFunctions,
                     functionRvas.data(), exports.NumberOfFunctions * sizeof(DWORD))) {
        *error = QStringLiteral("无法读取目标 kernel32 导出表内容。");
        return false;
    }

    for (DWORD i = 0; i < exports.NumberOfNames; ++i) {
        char name[64] = {0};
        if (!readProcess(process, kernel32Base + nameRvas[i], name, sizeof(name) - 1))
            continue;
        name[sizeof(name) - 1] = '\0';
        if (std::strcmp(name, "LoadLibraryW") != 0)
            continue;

        const WORD ordinal = ordinals[i];
        if (ordinal >= exports.NumberOfFunctions)
            break;
        const DWORD functionRva = functionRvas[ordinal];
        if (functionRva == 0)
            break;

        // 转发导出的 RVA 落在导出目录内部，指向 "KERNELBASE.LoadLibraryW" 字符串，
        // 直接当函数地址执行会让注入线程跑飞，这里明确拒绝。
        if (functionRva >= directory.VirtualAddress &&
            functionRva < directory.VirtualAddress + directory.Size) {
            *error = QCoreApplication::translate(
                "MemoryManager", "目标 kernel32 的 LoadLibraryW 是转发导出，无法作为注入入口。");
            return false;
        }

        *address = static_cast<quint64>(kernel32Base) + functionRva;
        return true;
    }

    *error = QStringLiteral("目标 kernel32 中找不到 LoadLibraryW 导出。");
    return false;
}

// ---------------------------------------------------------------------------
// 注入失败诊断
//
// 直接抛一句“无法注入”对反馈者毫无帮助：失败可能来自权限、杀软拦截、位数不符、
// DLL 依赖缺失等完全不同的原因。下面统一把阶段 + GetLastError + 可操作建议
// 拼进错误信息，便于用户自助排查。
// ---------------------------------------------------------------------------

QString win32ErrorText(DWORD code)
{
    wchar_t *buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);

    QString text;
    if (length && buffer)
        text = QString::fromWCharArray(buffer, static_cast<int>(length)).trimmed();
    if (buffer)
        LocalFree(buffer);

    // FormatMessage 的文本自带句号/换行，拼到我们自己的句子里会重复
    while (!text.isEmpty() && QStringLiteral("。.\r\n ").contains(text.back()))
        text.chop(1);
    return text;
}

QString win32ErrorHint(DWORD code)
{
    switch (code) {
    case ERROR_ACCESS_DENIED:
        return QCoreApplication::translate(
            "MemoryManager",
            "通常是被杀毒/安全软件拦截了远程线程，或目标进程以更高权限运行。"
            "请以管理员身份运行本工具，并把本程序与 DR2CAgent.dll 加入白名单。");
    case ERROR_INVALID_PARAMETER:
        return QCoreApplication::translate(
            "MemoryManager",
            "参数无效，可能是目标进程状态异常或注入起始地址无效，请确认游戏仍在运行。");
    case ERROR_NOT_ENOUGH_MEMORY:
        return QCoreApplication::translate(
            "MemoryManager", "目标进程内存不足，无法分配注入所需空间。");
    case ERROR_INVALID_HANDLE:
        return QCoreApplication::translate(
            "MemoryManager", "进程句柄已失效，游戏可能已经退出。");
    case ERROR_PARTIAL_COPY:
        return QCoreApplication::translate(
            "MemoryManager", "只能复制部分目标内存，目标进程可能受到保护。");
    case ERROR_NOT_FOUND:
    case ERROR_MOD_NOT_FOUND:
        return QCoreApplication::translate(
            "MemoryManager",
            "找不到目标线程或模块，请确认游戏仍在运行且未被其他注入器/调试器占用。");
    default:
        return QString();
    }
}

QString describeWin32Failure(const QString &stage, DWORD code)
{
    const QString system = win32ErrorText(code);
    const QString hint = win32ErrorHint(code);

    QString text = QCoreApplication::translate("MemoryManager", "%1：失败，错误码 %2")
                       .arg(stage)
                       .arg(code);
    if (!system.isEmpty())
        text += QCoreApplication::translate("MemoryManager", "（%1）").arg(system);
    if (!hint.isEmpty())
        text += QCoreApplication::translate("MemoryManager", "。%1").arg(hint);
    return text;
}

bool isCurrentProcessElevated()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return false;

    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    const bool ok = GetTokenInformation(token, TokenElevation, &elevation,
                                        sizeof(elevation), &size) != FALSE;
    CloseHandle(token);
    return ok && elevation.TokenIsElevated != 0;
}

// 读取本地 DLL 的 PE Machine 字段，确认它确实是 32 位模块。
bool queryDllMachineType(const QString &path, quint16 *machine)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QByteArray dosHeader = file.read(0x40);
    if (dosHeader.size() < 0x40 ||
        dosHeader.at(0) != 'M' || dosHeader.at(1) != 'Z')
        return false;

    quint32 peOffset = 0;
    std::memcpy(&peOffset, dosHeader.constData() + 0x3C, sizeof(peOffset));
    if (!file.seek(static_cast<qint64>(peOffset)))
        return false;

    const QByteArray ntHeader = file.read(6);
    if (ntHeader.size() < 6 ||
        ntHeader.at(0) != 'P' || ntHeader.at(1) != 'E' ||
        ntHeader.at(2) != '\0' || ntHeader.at(3) != '\0')
        return false;

    *machine = static_cast<quint16>(
        static_cast<quint8>(ntHeader.at(4)) |
        (static_cast<quint16>(static_cast<quint8>(ntHeader.at(5))) << 8));
    return true;
}

bool isModuleLoaded(DWORD pid, const wchar_t *moduleName)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE)
        return false;

    bool found = false;
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Module32FirstW(snapshot, &entry)) {
        do {
            if (lstrcmpiW(entry.szModule, moduleName) == 0) {
                found = true;
                break;
            }
        } while (Module32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return found;
}

bool waitForModule(DWORD pid, const wchar_t *moduleName, int timeoutMs)
{
    constexpr int kStep = 100;
    for (int waited = 0; waited <= timeoutMs; waited += kStep) {
        if (isModuleLoaded(pid, moduleName))
            return true;
        Sleep(kStep);
    }
    return false;
}

// 方式一：CreateRemoteThread。最经典，但最容易被安全软件 hook 或拦截。
bool injectViaCreateRemoteThread(HANDLE process, quint64 loadLibrary, void *remotePath,
                                 QString *detail)
{
    SetLastError(ERROR_SUCCESS);
    HANDLE thread = CreateRemoteThread(
        process, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(static_cast<quintptr>(loadLibrary)),
        remotePath, 0, nullptr);
    const DWORD createError = GetLastError();
    if (!thread) {
        *detail = describeWin32Failure(
            QCoreApplication::translate("MemoryManager", "CreateRemoteThread"), createError);
        return false;
    }

    const DWORD wait = WaitForSingleObject(thread, 10000);
    DWORD exitCode = 0;
    GetExitCodeThread(thread, &exitCode);
    CloseHandle(thread);

    if (wait != WAIT_OBJECT_0) {
        *detail = QCoreApplication::translate(
            "MemoryManager", "CreateRemoteThread：注入线程 10 秒内未返回，LoadLibraryW 可能被卡住");
        return false;
    }
    if (exitCode == 0) {
        *detail = QCoreApplication::translate(
            "MemoryManager",
            "CreateRemoteThread：LoadLibraryW 返回 0（DLL 依赖缺失、位数不符或被安全软件拦截）");
        return false;
    }
    if (exitCode >= 0xC0000000u) {
        *detail = QCoreApplication::translate(
                      "MemoryManager", "CreateRemoteThread：注入线程异常退出（0x%1）")
                      .arg(exitCode, 8, 16, QLatin1Char('0'));
        return false;
    }
    *detail = QCoreApplication::translate("MemoryManager", "CreateRemoteThread：已创建注入线程");
    return true;
}

using NtCreateThreadExFunction = LONG (NTAPI *)(PHANDLE, DWORD, void *, HANDLE,
                                                void *, void *, DWORD, SIZE_T,
                                                SIZE_T, SIZE_T, void *);
using RtlNtStatusToDosErrorFunction = ULONG (NTAPI *)(LONG);

// 方式二：NtCreateThreadEx。绕过用户态对 CreateRemoteThread 的 hook。
bool injectViaNtCreateThreadEx(HANDLE process, quint64 loadLibrary, void *remotePath,
                               QString *detail)
{
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) {
        *detail = QCoreApplication::translate("MemoryManager", "NtCreateThreadEx：无法获取 ntdll.dll");
        return false;
    }

    const auto createThreadEx = reinterpret_cast<NtCreateThreadExFunction>(
        GetProcAddress(ntdll, "NtCreateThreadEx"));
    if (!createThreadEx) {
        *detail = QCoreApplication::translate("MemoryManager",
                                              "NtCreateThreadEx：当前系统不支持该接口");
        return false;
    }

    HANDLE thread = nullptr;
    const LONG status = createThreadEx(
        &thread, THREAD_ALL_ACCESS, nullptr, process,
        reinterpret_cast<void *>(static_cast<quintptr>(loadLibrary)),
        remotePath, 0, 0, 0, 0, nullptr);
    if (status < 0 || !thread) {
        const auto rtlNtStatusToDosError = reinterpret_cast<RtlNtStatusToDosErrorFunction>(
            GetProcAddress(ntdll, "RtlNtStatusToDosError"));
        const DWORD code = (rtlNtStatusToDosError && status < 0)
            ? static_cast<DWORD>(rtlNtStatusToDosError(status))
            : GetLastError();
        const QString stage = QCoreApplication::translate(
                                  "MemoryManager", "NtCreateThreadEx（NTSTATUS 0x%1）")
                                  .arg(static_cast<quint32>(status), 8, 16, QLatin1Char('0'));
        *detail = describeWin32Failure(stage, code);
        if (thread)
            CloseHandle(thread);
        return false;
    }

    const DWORD wait = WaitForSingleObject(thread, 10000);
    DWORD exitCode = 0;
    GetExitCodeThread(thread, &exitCode);
    CloseHandle(thread);

    if (wait != WAIT_OBJECT_0) {
        *detail = QCoreApplication::translate("MemoryManager",
                                              "NtCreateThreadEx：注入线程 10 秒内未返回");
        return false;
    }
    if (exitCode == 0) {
        *detail = QCoreApplication::translate("MemoryManager",
                                              "NtCreateThreadEx：LoadLibraryW 返回 0");
        return false;
    }
    *detail = QCoreApplication::translate("MemoryManager", "NtCreateThreadEx：已创建注入线程");
    return true;
}

// 方式三：调用随附的 32 位注入器。
// 部分环境下 64 位进程无法向 WOW64 进程创建远程线程，交给同为 32 位的进程处理最稳。
bool injectViaHelperProcess(DWORD pid, const QString &dllPath, QString *detail)
{
    const QString helper = QDir::toNativeSeparators(
        QCoreApplication::applicationDirPath() + QStringLiteral("/DR2CInjector32.exe"));
    if (!QFileInfo::exists(helper)) {
        *detail = QCoreApplication::translate(
            "MemoryManager", "32 位注入器：未找到 DR2CInjector32.exe（可选组件，已跳过）");
        return false;
    }

    QProcess process;
    process.setProgram(helper);
    process.setArguments({QString::number(pid), QDir::toNativeSeparators(dllPath)});
    process.setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments *arguments) {
            arguments->flags |= CREATE_NO_WINDOW;
        });
    process.start();
    if (!process.waitForStarted(5000)) {
        *detail = QCoreApplication::translate("MemoryManager", "32 位注入器：启动失败（%1）")
                      .arg(process.errorString());
        return false;
    }
    if (!process.waitForFinished(20000)) {
        process.kill();
        process.waitForFinished(2000);
        *detail = QCoreApplication::translate("MemoryManager", "32 位注入器：执行超时");
        return false;
    }

    const QString output = QString::fromUtf8(process.readAllStandardError()).trimmed();
    if (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
        *detail = QCoreApplication::translate("MemoryManager", "32 位注入器：已加载 Agent");
        return true;
    }
    *detail = output.isEmpty()
        ? QCoreApplication::translate("MemoryManager", "32 位注入器：执行失败（退出码 %1）")
              .arg(process.exitCode())
        : QCoreApplication::translate("MemoryManager", "32 位注入器：%1").arg(output);
    return false;
}

DWORD findThreadForHijack(DWORD pid)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;

    DWORD best = 0;
    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    if (Thread32First(snapshot, &entry)) {
        do {
            if (entry.th32OwnerProcessID == pid &&
                (best == 0 || entry.th32ThreadID < best))
                best = entry.th32ThreadID;
        } while (Thread32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return best;
}

// 方式四：线程劫持。挂起游戏线程，把 32 位上下文改到 LoadLibraryW 存根上，
// 加载完成后再原样恢复。不经过任何“创建远程线程”接口，能绕开多数 hook。
bool injectViaThreadHijack(HANDLE process, DWORD pid, quint64 loadLibrary,
                           void *remotePath, QString *detail)
{
    const DWORD threadId = findThreadForHijack(pid);
    if (!threadId) {
        *detail = QCoreApplication::translate("MemoryManager", "线程劫持：找不到可用的游戏线程");
        return false;
    }

    HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                               FALSE, threadId);
    if (!thread) {
        *detail = describeWin32Failure(
            QCoreApplication::translate("MemoryManager", "线程劫持（OpenThread）"), GetLastError());
        return false;
    }

    if (SuspendThread(thread) == static_cast<DWORD>(-1)) {
        *detail = describeWin32Failure(
            QCoreApplication::translate("MemoryManager", "线程劫持（SuspendThread）"), GetLastError());
        CloseHandle(thread);
        return false;
    }

    WOW64_CONTEXT saved{};
    saved.ContextFlags = WOW64_CONTEXT_CONTROL | WOW64_CONTEXT_INTEGER;
    if (!Wow64GetThreadContext(thread, &saved)) {
        *detail = describeWin32Failure(
            QCoreApplication::translate("MemoryManager", "线程劫持（Wow64GetThreadContext）"),
            GetLastError());
        ResumeThread(thread);
        CloseHandle(thread);
        return false;
    }

    // 32 位存根：push <路径>; mov eax, <LoadLibraryW>; call eax; jmp $
    unsigned char stub[14] = {
        0x68, 0, 0, 0, 0,
        0xB8, 0, 0, 0, 0,
        0xFF, 0xD0,
        0xEB, 0xFE,
    };
    const quint32 pathLow = static_cast<quint32>(reinterpret_cast<quintptr>(remotePath));
    const quint32 loadLow = static_cast<quint32>(loadLibrary);
    std::memcpy(stub + 1, &pathLow, sizeof(pathLow));
    std::memcpy(stub + 6, &loadLow, sizeof(loadLow));

    void *stubAddress = VirtualAllocEx(process, nullptr, sizeof(stub),
                                       MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stubAddress) {
        *detail = describeWin32Failure(
            QCoreApplication::translate("MemoryManager", "线程劫持（分配存根内存）"), GetLastError());
        ResumeThread(thread);
        CloseHandle(thread);
        return false;
    }

    SIZE_T written = 0;
    if (!WriteProcessMemory(process, stubAddress, stub, sizeof(stub), &written) ||
        written != sizeof(stub)) {
        *detail = describeWin32Failure(
            QCoreApplication::translate("MemoryManager", "线程劫持（写入存根）"), GetLastError());
        VirtualFreeEx(process, stubAddress, 0, MEM_RELEASE);
        ResumeThread(thread);
        CloseHandle(thread);
        return false;
    }

    WOW64_CONTEXT hijacked = saved;
    hijacked.ContextFlags = WOW64_CONTEXT_CONTROL | WOW64_CONTEXT_INTEGER;
    hijacked.Eip = static_cast<DWORD>(reinterpret_cast<quintptr>(stubAddress));
    if (!Wow64SetThreadContext(thread, &hijacked)) {
        *detail = describeWin32Failure(
            QCoreApplication::translate("MemoryManager", "线程劫持（Wow64SetThreadContext）"),
            GetLastError());
        VirtualFreeEx(process, stubAddress, 0, MEM_RELEASE);
        ResumeThread(thread);
        CloseHandle(thread);
        return false;
    }

    ResumeThread(thread);
    const bool loaded = waitForModule(pid, L"DR2CAgent.dll", 10000);
    if (loaded)
        Sleep(300); // 模块出现时 DllMain 可能还没跑完，稍等再还原线程

    // 无论成功与否都要还原被劫持的线程，否则游戏会停在我们写入的存根里
    SuspendThread(thread);
    saved.ContextFlags = WOW64_CONTEXT_CONTROL | WOW64_CONTEXT_INTEGER;
    Wow64SetThreadContext(thread, &saved);
    ResumeThread(thread);
    CloseHandle(thread);
    VirtualFreeEx(process, stubAddress, 0, MEM_RELEASE);

    *detail = loaded
        ? QCoreApplication::translate("MemoryManager", "线程劫持：已加载 Agent")
        : QCoreApplication::translate("MemoryManager",
                                      "线程劫持：已恢复线程，但未检测到 DR2CAgent.dll 模块");
    return loaded;
}

bool writeAll(HANDLE pipe, const void *buffer, quint32 size)
{
    const char *data = static_cast<const char *>(buffer);
    quint32 total = 0;
    while (total < size) {
        DWORD written = 0;
        if (!WriteFile(pipe, data + total, size - total, &written, nullptr) || written == 0)
            return false;
        total += written;
    }
    return true;
}

bool readAll(HANDLE pipe, void *buffer, quint32 size)
{
    char *data = static_cast<char *>(buffer);
    quint32 total = 0;
    while (total < size) {
        DWORD received = 0;
        if (!ReadFile(pipe, data + total, size - total, &received, nullptr) || received == 0)
            return false;
        total += received;
    }
    return true;
}

struct WindowSearch {
    DWORD pid = 0;
    HWND result = nullptr;
    LONG bestArea = -1;
};

BOOL CALLBACK findGameWindowProc(HWND window, LPARAM parameter)
{
    WindowSearch *search = reinterpret_cast<WindowSearch *>(parameter);
    DWORD windowPid = 0;
    GetWindowThreadProcessId(window, &windowPid);
    if (windowPid != search->pid || !IsWindowVisible(window) ||
        GetWindow(window, GW_OWNER) != nullptr)
        return TRUE;

    RECT client{};
    if (!GetClientRect(window, &client))
        return TRUE;
    const LONG area = (client.right - client.left) * (client.bottom - client.top);
    if (area > search->bestArea) {
        search->bestArea = area;
        search->result = window;
    }
    return TRUE;
}

#endif // Q_OS_WIN

} // namespace

MemoryManager::MemoryManager(QObject *parent)
    : QObject(parent)
    , m_attached(false)
    , m_processId(0)
    , m_moduleBase(0)
    , m_handle(nullptr)
    , m_pipe(nullptr)
{
}

MemoryManager::~MemoryManager()
{
    detachProcess();
}

bool MemoryManager::hasFunction(GameResolver::Function function) const
{
    return m_gameLayout.valid &&
        m_gameLayout.functionRvas[static_cast<std::size_t>(function)] != 0;
}

QString MemoryManager::processImagePath() const
{
#ifdef Q_OS_WIN
    if (!m_handle) return QString();
    wchar_t buffer[MAX_PATH * 2] = {0};
    DWORD size = static_cast<DWORD>(MAX_PATH * 2);
    if (QueryFullProcessImageNameW(static_cast<HANDLE>(m_handle), 0, buffer, &size))
        return QString::fromWCharArray(buffer, static_cast<int>(size));
#endif
    return QString();
}

QString MemoryManager::gameDirectory() const
{
    const QString path = processImagePath();
    if (path.isEmpty()) return QString();
    return QFileInfo(path).absolutePath();
}

QList<QVariantMap> MemoryManager::enumerateProcesses()
{
    QList<QVariantMap> processes;
#ifdef Q_OS_WIN
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return processes;

    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(PROCESSENTRY32W);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            QVariantMap process;
            process["pid"] = static_cast<quint32>(entry.th32ProcessID);
            process["name"] = QString::fromWCharArray(entry.szExeFile);
            processes.append(process);
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
#endif
    return processes;
}

bool MemoryManager::attachProcess(const QString &processName)
{
#ifdef Q_OS_WIN
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        emit attachError(QStringLiteral("无法枚举进程列表。"));
        return false;
    }

    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(PROCESSENTRY32W);
    quint32 foundPid = 0;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            const QString name = QString::fromWCharArray(entry.szExeFile);
            if (name.compare(processName, Qt::CaseInsensitive) == 0) {
                foundPid = static_cast<quint32>(entry.th32ProcessID);
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);

    if (!foundPid) {
        emit attachError(QStringLiteral("未找到进程: %1").arg(processName));
        return false;
    }
    return attachProcessById(foundPid);
#else
    Q_UNUSED(processName);
    emit attachError(QStringLiteral("仅支持 Windows 平台。"));
    return false;
#endif
}

bool MemoryManager::attachProcessById(quint32 pid)
{
#ifdef Q_OS_WIN
    detachProcess();

    HANDLE process = OpenProcess(kProcessAccess, FALSE, pid);
    if (!process) {
        const DWORD code = GetLastError();
        emit attachError(tr("无法打开进程 PID: %1。\n%2")
                             .arg(pid)
                             .arg(describeWin32Failure(tr("OpenProcess"), code)));
        return false;
    }

    BOOL isWow64 = FALSE;
    if (!IsWow64Process(process, &isWow64) || !isWow64) {
        CloseHandle(process);
        emit attachError(QStringLiteral("目标进程不是 32 位程序，无法注入 Agent。"));
        return false;
    }

    m_handle = process;
    m_processId = pid;

    wchar_t exeName[MAX_PATH] = {0};
    DWORD size = MAX_PATH;
    if (QueryFullProcessImageNameW(process, 0, exeName, &size)) {
        m_processName = QString::fromWCharArray(exeName);
        const int lastSlash = m_processName.lastIndexOf('\\');
        if (lastSlash >= 0)
            m_processName = m_processName.mid(lastSlash + 1);
    }

    QString error;
    if (!injectAgent(&error) || !connectAgent(&error)) {
        detachProcess();
        emit attachError(error);
        return false;
    }

    // 获取模块基址
    QByteArray response;
    quint32 status = 0;
    if (!sendRequest(static_cast<quint16>(dr2c::Command::GetInfo), QByteArray(),
                     &response, &status) ||
        status != static_cast<quint32>(dr2c::Status::Ok) ||
        response.size() < static_cast<int>(sizeof(dr2c::InfoResponse))) {
        detachProcess();
        emit attachError(QStringLiteral("Agent 未返回有效的进程信息。"));
        return false;
    }
    dr2c::InfoResponse info{};
    std::memcpy(&info, response.constData(), sizeof(info));
    m_moduleBase = info.moduleBase;

    if (!m_moduleBase || !fetchLayout(&error)) {
        const QString message = error.isEmpty()
            ? QStringLiteral("无法解析目标程序布局。") : error;
        detachProcess();
        emit attachError(message);
        return false;
    }

    m_attached = true;
    emit processAttached();
    return true;
#else
    Q_UNUSED(pid);
    emit attachError(QStringLiteral("仅支持 Windows 平台。"));
    return false;
#endif
}

void MemoryManager::detachProcess()
{
    const bool wasAttached = m_attached;
#ifdef Q_OS_WIN
    if (m_pipe) {
        CloseHandle(static_cast<HANDLE>(m_pipe));
        m_pipe = nullptr;
    }
    if (m_handle) {
        CloseHandle(static_cast<HANDLE>(m_handle));
        m_handle = nullptr;
    }
#endif
    m_attached = false;
    m_agentInjected = false;
    m_processId = 0;
    m_processName.clear();
    m_moduleBase = 0;
    m_codeSectionRva = 0;
    m_codeSectionSize = 0;
    m_gameLayout = GameResolver::Layout{};
    // 只有真正处于附加状态时才通知界面，避免重连过程中的无意义“已分离”提示
    if (wasAttached)
        emit processDetached();
}

bool MemoryManager::isProcessAlive() const
{
#ifdef Q_OS_WIN
    if (!m_handle) return false;
    DWORD exitCode = 0;
    if (!GetExitCodeProcess(static_cast<HANDLE>(m_handle), &exitCode))
        return false;
    return exitCode == STILL_ACTIVE;
#else
    return false;
#endif
}

bool MemoryManager::injectAgent(QString *error)
{
#ifdef Q_OS_WIN
    const QString dllPath = QDir::toNativeSeparators(
        QCoreApplication::applicationDirPath() + QStringLiteral("/DR2CAgent.dll"));
    if (!QFileInfo::exists(dllPath)) {
        *error = tr("未找到注入模块：%1\n请确认 DR2CAgent.dll 与 DR2CDebugTool.exe 在同一目录。")
                     .arg(dllPath);
        return false;
    }

    // 前端是 64 位，Agent 必须是 32 位；放错 DLL 时先明确报出来，
    // 否则只会得到含糊的“注入失败”。
    quint16 machine = 0;
    if (!queryDllMachineType(dllPath, &machine)) {
        *error = tr("无法读取 %1 的 PE 头，文件可能已损坏。").arg(dllPath);
        return false;
    }
    if (machine != IMAGE_FILE_MACHINE_I386) {
        *error = tr("DR2CAgent.dll 不是 32 位模块（Machine=0x%1），无法注入 32 位游戏。\n"
                    "请用 32 位 MinGW 工具链重新构建 Agent。")
                     .arg(static_cast<uint>(machine), 4, 16, QLatin1Char('0'));
        return false;
    }

    // 重复“附加”（例如上一次只是断开连接）时 DLL 还在进程里，直接复用。
    if (isModuleLoaded(m_processId, L"DR2CAgent.dll")) {
        m_agentInjected = true;
        return true;
    }

    HANDLE process = static_cast<HANDLE>(m_handle);
    quint64 loadLibrary = 0;
    if (!resolveRemoteLoadLibrary(process, m_processId, &loadLibrary, error))
        return false;
    if (loadLibrary == 0 || loadLibrary > 0xFFFFFFFFull) {
        *error = tr("解析出的 LoadLibraryW 地址无效（0x%1），无法注入 32 位 DLL。")
                     .arg(loadLibrary, 0, 16);
        return false;
    }

    // 把 DLL 路径以 UTF-16（含结尾 NUL）写入目标进程
    const QByteArray pathBytes(
        reinterpret_cast<const char *>(dllPath.utf16()),
        (dllPath.size() + 1) * static_cast<int>(sizeof(ushort)));

    void *remotePath = VirtualAllocEx(process, nullptr,
                                      static_cast<SIZE_T>(pathBytes.size()),
                                      MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remotePath) {
        *error = describeWin32Failure(tr("在目标进程中分配参数内存（VirtualAllocEx）"),
                                      GetLastError());
        return false;
    }

    SIZE_T written = 0;
    if (!WriteProcessMemory(process, remotePath, pathBytes.constData(),
                            static_cast<SIZE_T>(pathBytes.size()), &written) ||
        written != static_cast<SIZE_T>(pathBytes.size())) {
        const DWORD code = GetLastError();
        VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        *error = describeWin32Failure(tr("写入 DLL 路径（WriteProcessMemory）"), code);
        return false;
    }

    // 依次尝试多种注入方式。是否成功以“目标进程里真的出现了 DR2CAgent.dll”为准，
    // 而不看单个 API 的返回值——跨位数时 CreateRemoteThread 可能成功返回但线程跑飞。
    QStringList attempts;
    QString detail;
    bool loaded = false;

    injectViaCreateRemoteThread(process, loadLibrary, remotePath, &detail);
    attempts << detail;
    loaded = waitForModule(m_processId, L"DR2CAgent.dll", 1500);

    if (!loaded) {
        injectViaNtCreateThreadEx(process, loadLibrary, remotePath, &detail);
        attempts << detail;
        loaded = waitForModule(m_processId, L"DR2CAgent.dll", 1500);
    }

    if (!loaded) {
        injectViaHelperProcess(m_processId, dllPath, &detail);
        attempts << detail;
        loaded = waitForModule(m_processId, L"DR2CAgent.dll", 2000);
    }

    if (!loaded) {
        injectViaThreadHijack(process, m_processId, loadLibrary, remotePath, &detail);
        attempts << detail;
        loaded = waitForModule(m_processId, L"DR2CAgent.dll", 2000);
    }

    VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);

    if (!loaded) {
        const QString target = m_processName.isEmpty()
            ? QStringLiteral("PID %1").arg(m_processId)
            : tr("%1（PID %2）").arg(m_processName).arg(m_processId);

        QString message = tr("无法把 DR2CAgent.dll 注入目标进程 %1。").arg(target);
        message += tr("\n已尝试以下方式：");
        for (const QString &line : attempts)
            message += tr("\n  · ") + line;
        message += tr("\n\n环境：本工具为 %1 位进程，%2管理员权限；目标进程为 32 位。")
                       .arg(static_cast<int>(sizeof(void *)) * 8)
                       .arg(isCurrentProcessElevated() ? tr("已获得") : tr("未获得"));
        message += tr("\n建议：1) 以管理员身份重新运行本工具；"
                      "2) 把 DR2CDebugTool.exe、DR2CAgent.dll 加入杀毒/安全软件白名单；"
                      "3) 关闭其他注入器/调试器后重试。");
        *error = message;
        return false;
    }

    m_agentInjected = true;
    return true;
#else
    *error = QStringLiteral("仅支持 Windows 平台。");
    return false;
#endif
}

bool MemoryManager::connectAgent(QString *error)
{
#ifdef Q_OS_WIN
    const QString pipePath =
        QStringLiteral("\\\\.\\pipe\\DR2CTool_") + QString::number(m_processId);

    HANDLE pipe = INVALID_HANDLE_VALUE;
    for (int attempt = 0; attempt < 50; ++attempt) {
        pipe = CreateFileW(reinterpret_cast<LPCWSTR>(pipePath.utf16()),
                           GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                           OPEN_EXISTING, 0, nullptr);
        if (pipe != INVALID_HANDLE_VALUE)
            break;
        WaitNamedPipeW(reinterpret_cast<LPCWSTR>(pipePath.utf16()), 100);
        Sleep(50);
    }

    if (pipe == INVALID_HANDLE_VALUE) {
        *error = QStringLiteral("无法连接到 Agent 管道（注入后服务未就绪）。");
        return false;
    }

    m_pipe = pipe;

    dr2c::PingRequest ping{};
    ping.cookie = 0x44523243u; // 'DR2C'
    QByteArray request(reinterpret_cast<const char *>(&ping), sizeof(ping));
    QByteArray response;
    quint32 status = 0;
    bool ok = sendRequest(static_cast<quint16>(dr2c::Command::Ping),
                          request, &response, &status);
    if (!ok || status != static_cast<quint32>(dr2c::Status::Ok) ||
        response.size() < static_cast<int>(sizeof(dr2c::PingResponse))) {
        CloseHandle(pipe);
        m_pipe = nullptr;
        *error = QStringLiteral("Agent 未通过握手校验。");
        return false;
    }

    dr2c::PingResponse reply{};
    std::memcpy(&reply, response.constData(), sizeof(reply));
    if (reply.cookie != ping.cookie || reply.pid != m_processId) {
        CloseHandle(pipe);
        m_pipe = nullptr;
        *error = QStringLiteral("Agent 握手信息不匹配。");
        return false;
    }
    return true;
#else
    *error = QStringLiteral("仅支持 Windows 平台。");
    return false;
#endif
}

bool MemoryManager::fetchLayout(QString *error)
{
    QByteArray response;
    quint32 status = 0;
    if (!sendRequest(static_cast<quint16>(dr2c::Command::GetLayout), QByteArray(),
                     &response, &status) ||
        status != static_cast<quint32>(dr2c::Status::Ok) ||
        response.size() < static_cast<int>(sizeof(dr2c::LayoutInfo))) {
        *error = QStringLiteral("Agent 未返回布局信息。");
        return false;
    }

    dr2c::LayoutInfo info{};
    std::memcpy(&info, response.constData(), sizeof(info));

    m_gameLayout = GameResolver::Layout{};
    m_gameLayout.valid = info.valid != 0;
    m_gameLayout.imageBase = info.imageBase;
    m_gameLayout.imageSize = info.imageSize;
    for (std::size_t i = 0; i < GameResolver::FunctionCount; ++i)
        m_gameLayout.functionRvas[i] = info.functionRvas[i];
    m_gameLayout.thingPoolRva = info.thingPoolRva;
    m_gameLayout.weaponPoolRva = info.weaponPoolRva;
    m_gameLayout.missionStateRva = info.missionStateRva;
    m_gameLayout.characterPoolPointerRva = info.characterPoolPointerRva;
    m_gameLayout.characterPoolRva = info.characterPoolRva;
    m_gameLayout.mapLayerMetaRva = info.mapLayerMetaRva;
    m_gameLayout.currentMapIdRva = info.currentMapIdRva;
    m_gameLayout.nextThingIdRva = info.nextThingIdRva;
    m_gameLayout.cameraMapLayerRva = info.cameraMapLayerRva;
    m_gameLayout.playerThingArrayRva = info.playerThingArrayRva;
    m_gameLayout.thingStride = info.thingStride;
    m_gameLayout.characterStride = info.characterStride;
    m_gameLayout.weaponStride = info.weaponStride;
    m_gameLayout.mapMetaStride = info.mapMetaStride;
    m_gameLayout.warning = QString::fromUtf8(info.warning);
    m_gameLayout.error = QString::fromUtf8(info.error);

    m_codeSectionRva = info.codeSectionRva;
    m_codeSectionSize = info.codeSectionSize;

    if (!m_gameLayout.valid) {
        *error = m_gameLayout.error.isEmpty()
            ? QStringLiteral("无法解析游戏内存布局。") : m_gameLayout.error;
        return false;
    }
    return true;
}

bool MemoryManager::sendRequest(quint16 command, const QByteArray &request,
                                QByteArray *response, quint32 *status) const
{
#ifdef Q_OS_WIN
    HANDLE pipe = static_cast<HANDLE>(m_pipe);
    if (!pipe)
        return false;

    dr2c::PacketHeader header{};
    header.magic = dr2c::kPipeMagic;
    header.version = dr2c::kProtocolVersion;
    header.cmd = command;
    header.seq = ++m_sequence;
    header.status = 0;
    header.payloadLen = static_cast<quint32>(request.size());

    if (!writeAll(pipe, &header, sizeof(header)))
        return false;
    if (!request.isEmpty() &&
        !writeAll(pipe, request.constData(), static_cast<quint32>(request.size())))
        return false;

    dr2c::PacketHeader reply{};
    if (!readAll(pipe, &reply, sizeof(reply)))
        return false;
    if (reply.magic != dr2c::kPipeMagic ||
        reply.version != dr2c::kProtocolVersion ||
        reply.seq != header.seq ||
        reply.payloadLen > dr2c::kMaxPayload)
        return false;

    if (response) {
        response->resize(static_cast<int>(reply.payloadLen));
        if (reply.payloadLen &&
            !readAll(pipe, response->data(), reply.payloadLen))
            return false;
    } else if (reply.payloadLen) {
        QByteArray discard(static_cast<int>(reply.payloadLen), '\0');
        if (!readAll(pipe, discard.data(), reply.payloadLen))
            return false;
    }

    if (status)
        *status = reply.status;
    return true;
#else
    Q_UNUSED(command);
    Q_UNUSED(request);
    Q_UNUSED(response);
    Q_UNUSED(status);
    return false;
#endif
}

bool MemoryManager::readMemory(quint64 address, void *buffer, quint64 size) const
{
    if (!buffer || size == 0 || size > dr2c::kMaxPayload)
        return false;

    dr2c::ReadRequest request{};
    request.address = address;
    request.size = static_cast<quint32>(size);

    QByteArray payload(reinterpret_cast<const char *>(&request), sizeof(request));
    QByteArray response;
    quint32 status = 0;
    if (!sendRequest(static_cast<quint16>(dr2c::Command::Read), payload,
                     &response, &status) ||
        status != static_cast<quint32>(dr2c::Status::Ok) ||
        static_cast<quint64>(response.size()) != size)
        return false;

    std::memcpy(buffer, response.constData(), static_cast<size_t>(size));
    return true;
}

bool MemoryManager::writeMemory(quint64 address, const void *buffer, quint64 size) const
{
    if (!buffer || size == 0 || size > dr2c::kMaxPayload)
        return false;

    dr2c::WriteRequestHeader header{};
    header.address = address;
    header.size = static_cast<quint32>(size);

    QByteArray payload(reinterpret_cast<const char *>(&header), sizeof(header));
    payload.append(static_cast<const char *>(buffer), static_cast<int>(size));

    quint32 status = 0;
    if (!sendRequest(static_cast<quint16>(dr2c::Command::Write), payload,
                     nullptr, &status))
        return false;
    return status == static_cast<quint32>(dr2c::Status::Ok);
}

QByteArray MemoryManager::readCodeSection() const
{
    if (!m_codeSectionSize || !m_moduleBase)
        return QByteArray();
    QByteArray data(static_cast<int>(m_codeSectionSize), '\0');
    if (!readMemory(m_moduleBase + m_codeSectionRva, data.data(),
                    static_cast<quint64>(m_codeSectionSize)))
        return QByteArray();
    return data;
}

QString MemoryManager::readString(quint64 address, quint64 maxLen) const
{
    QByteArray data(static_cast<int>(maxLen), '\0');
    if (!readMemory(address, data.data(), maxLen))
        return QString();

    const int nullPos = data.indexOf('\0');
    if (nullPos >= 0)
        data.truncate(nullPos);

    return QString::fromUtf8(data);
}

bool MemoryManager::writeString(quint64 address, const QString &str, quint64 maxLen) const
{
    QByteArray data = str.toUtf8();
    if (static_cast<quint64>(data.size()) >= maxLen)
        data = data.left(static_cast<int>(maxLen) - 1);
    data.append('\0');

    return writeMemory(address, data.constData(), data.size());
}

// ==================== 游戏函数调用 ====================
bool MemoryManager::CallFunction(GameResolver::Function function,
                                 const QVector<quint64> &args,
                                 quint64 *retValue) const
{
    if (!m_pipe || !m_gameLayout.valid)
        return false;
    const quint32 functionRva = m_gameLayout.functionRvas[static_cast<std::size_t>(function)];
    if (!functionRva)
        return false;

    dr2c::CallRequest request{};
    request.functionId = static_cast<quint32>(function);
    request.argCount = static_cast<quint32>(args.size());

    QByteArray payload(reinterpret_cast<const char *>(&request), sizeof(request));
    for (quint64 arg : args)
        payload.append(reinterpret_cast<const char *>(&arg), sizeof(arg));

    QByteArray response;
    quint32 status = 0;
    if (!sendRequest(static_cast<quint16>(dr2c::Command::Call), payload,
                     &response, &status) ||
        response.size() < static_cast<int>(sizeof(dr2c::CallResponse)))
        return false;

    dr2c::CallResponse reply{};
    std::memcpy(&reply, response.constData(), sizeof(reply));
    if (retValue)
        *retValue = reply.returnValue;
    return status == static_cast<quint32>(dr2c::Status::Ok) && reply.ok != 0;
}

bool MemoryManager::ScriptEvaluateStringSafe(const QString &script) const
{
    if (!m_pipe || !m_gameLayout.valid)
        return false;

    dr2c::CallRequest request{};
    request.functionId = static_cast<quint32>(GameResolver::Function::ScriptEvaluateStringSafe);
    request.argCount = 0; // 脚本字符串紧随请求头

    QByteArray payload(reinterpret_cast<const char *>(&request), sizeof(request));
    payload.append(script.toUtf8());

    QByteArray response;
    quint32 status = 0;
    if (!sendRequest(static_cast<quint16>(dr2c::Command::Call), payload,
                     &response, &status) ||
        response.size() < static_cast<int>(sizeof(dr2c::CallResponse)))
        return false;

    dr2c::CallResponse reply{};
    std::memcpy(&reply, response.constData(), sizeof(reply));
    return status == static_cast<quint32>(dr2c::Status::Ok) && reply.ok != 0;
}

bool MemoryManager::FreeThing(quint64 curThingPtr) const
{
    return CallFunction(GameResolver::Function::FreeThing, QVector<quint64>{curThingPtr});
}

quint64 MemoryManager::AllocateEntity(qint8 type) const
{
    quint64 thingPtr = 0;
    if (!CallFunction(GameResolver::Function::AllocateEntity,
                      QVector<quint64>{static_cast<quint64>(type)}, &thingPtr))
        return 0;
    return thingPtr;
}

quint64 MemoryManager::AllocateThing(qint8 subtype) const
{
    quint64 thingPtr = 0;
    if (!CallFunction(GameResolver::Function::AllocateThing,
                      QVector<quint64>{static_cast<quint64>(subtype)}, &thingPtr))
        return 0;
    return thingPtr;
}

quint32 MemoryManager::AllocateCharacterSlot() const
{
    quint64 characterPtr = 0;
    if (!CallFunction(GameResolver::Function::AllocateCharacterSlot, QVector<quint64>(), &characterPtr))
        return 0;
    return static_cast<quint32>(characterPtr);
}

bool MemoryManager::Assigncharactertothing(quint64 addr, quint32 charid) const
{
    return CallFunction(GameResolver::Function::AssignCharacterToThing,
                        QVector<quint64>{addr, static_cast<quint64>(charid)});
}

quint64 MemoryManager::CreateWeaponEntity(qint32 weaponId) const
{
    if (!hasFunction(GameResolver::Function::CreateWeaponEntity))
        return 0;
    quint64 thingPtr = 0;
    if (!CallFunction(GameResolver::Function::CreateWeaponEntity,
                      QVector<quint64>{static_cast<quint64>(
                          static_cast<quint32>(weaponId))},
                      &thingPtr))
        return 0;
    return thingPtr;
}

bool MemoryManager::SwitchActiveTileLayer(quint8 mapid) const
{
    return CallFunction(GameResolver::Function::SwitchActiveTileLayer,
                        QVector<quint64>{static_cast<quint64>(mapid)});
}

bool MemoryManager::SetCurrentPlayerThing(quint64 curThingPtr) const
{
    return CallFunction(GameResolver::Function::SetCurrentPlayerThing,
                        QVector<quint64>{curThingPtr});
}

// ==================== 世界交互 ====================
bool MemoryManager::hoverThing(int mouseX, int mouseY, int clientWidth, int clientHeight,
                               HoverInfo *out) const
{
    if (out)
        *out = HoverInfo{};
    if (!m_pipe || !out)
        return false;

    dr2c::HoverRequest request{};
    request.mouseX = mouseX;
    request.mouseY = mouseY;
    request.clientWidth = clientWidth;
    request.clientHeight = clientHeight;
    request.pickRadiusPx = 0;

    QByteArray payload(reinterpret_cast<const char *>(&request), sizeof(request));
    QByteArray response;
    quint32 status = 0;
    if (!sendRequest(static_cast<quint16>(dr2c::Command::HoverThing), payload,
                     &response, &status) ||
        status != static_cast<quint32>(dr2c::Status::Ok) ||
        response.size() < static_cast<int>(sizeof(dr2c::HoverResponse)))
        return false;

    dr2c::HoverResponse reply{};
    std::memcpy(&reply, response.constData(), sizeof(reply));
    if (!reply.found)
        return true;

    out->found = true;
    out->thingId = reply.thingId;
    out->slot = reply.slot;
    out->mapId = reply.mapId;
    out->worldX = reply.worldX;
    out->worldY = reply.worldY;
    out->thingAddress = reply.thingAddress;
    out->rect = QRect(reply.rectLeft, reply.rectTop,
                      reply.rectRight - reply.rectLeft,
                      reply.rectBottom - reply.rectTop);
    return true;
}

bool MemoryManager::dragBegin(quint64 thingAddress) const
{
    if (!m_pipe)
        return false;
    dr2c::DragBeginRequest request{};
    request.address = thingAddress;

    QByteArray payload(reinterpret_cast<const char *>(&request), sizeof(request));
    quint32 status = 0;
    if (!sendRequest(static_cast<quint16>(dr2c::Command::DragBegin), payload,
                     nullptr, &status))
        return false;
    return status == static_cast<quint32>(dr2c::Status::Ok);
}

bool MemoryManager::dragMove(quint64 thingAddress, int mouseX, int mouseY,
                             int clientWidth, int clientHeight) const
{
    if (!m_pipe)
        return false;
    dr2c::DragMoveRequest request{};
    request.address = thingAddress;
    request.mouseX = mouseX;
    request.mouseY = mouseY;
    request.clientWidth = clientWidth;
    request.clientHeight = clientHeight;

    QByteArray payload(reinterpret_cast<const char *>(&request), sizeof(request));
    quint32 status = 0;
    if (!sendRequest(static_cast<quint16>(dr2c::Command::DragMove), payload,
                     nullptr, &status))
        return false;
    return status == static_cast<quint32>(dr2c::Status::Ok);
}

bool MemoryManager::dragEnd(quint64 thingAddress) const
{
    if (!m_pipe)
        return false;
    dr2c::DragEndRequest request{};
    request.address = thingAddress;

    QByteArray payload(reinterpret_cast<const char *>(&request), sizeof(request));
    quint32 status = 0;
    if (!sendRequest(static_cast<quint16>(dr2c::Command::DragEnd), payload,
                     nullptr, &status))
        return false;
    return status == static_cast<quint32>(dr2c::Status::Ok);
}

quint64 MemoryManager::gameWindowHandle() const
{
#ifdef Q_OS_WIN
    if (!m_processId)
        return 0;
    WindowSearch search;
    search.pid = m_processId;
    EnumWindows(findGameWindowProc, reinterpret_cast<LPARAM>(&search));
    return reinterpret_cast<quint64>(search.result);
#else
    return 0;
#endif
}
