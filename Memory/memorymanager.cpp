#include "memorymanager.h"

#include "../Common/protocol.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

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
        *error = QStringLiteral("无法枚举目标进程模块（需要管理员权限？）。");
        return false;
    }

    quint32 kernel32Base = 0;
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Module32FirstW(snapshot, &entry)) {
        do {
            if (lstrcmpiW(entry.szModule, L"kernel32.dll") == 0) {
                kernel32Base = static_cast<quint32>(reinterpret_cast<quintptr>(entry.modBaseAddr));
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

        *address = static_cast<quint64>(kernel32Base) + functionRva;
        return true;
    }

    *error = QStringLiteral("目标 kernel32 中找不到 LoadLibraryW 导出。");
    return false;
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
        emit attachError(QStringLiteral("无法打开进程 PID: %1（请以管理员身份运行）。").arg(pid));
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
        *error = QStringLiteral("未找到注入模块：%1").arg(dllPath);
        return false;
    }

    HANDLE process = static_cast<HANDLE>(m_handle);
    quint64 loadLibrary = 0;
    if (!resolveRemoteLoadLibrary(process, m_processId, &loadLibrary, error))
        return false;

    // 把 DLL 路径以 UTF-16（含结尾 NUL）写入目标进程
    const QByteArray pathBytes(
        reinterpret_cast<const char *>(dllPath.utf16()),
        (dllPath.size() + 1) * static_cast<int>(sizeof(ushort)));

    void *remotePath = VirtualAllocEx(process, nullptr,
                                      static_cast<SIZE_T>(pathBytes.size()),
                                      MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remotePath) {
        *error = QStringLiteral("无法在目标进程中分配注入参数内存。");
        return false;
    }

    SIZE_T written = 0;
    if (!WriteProcessMemory(process, remotePath, pathBytes.constData(),
                            static_cast<SIZE_T>(pathBytes.size()), &written) ||
        written != static_cast<SIZE_T>(pathBytes.size())) {
        VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        *error = QStringLiteral("写入注入参数失败。");
        return false;
    }

    HANDLE thread = CreateRemoteThread(
        process, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(
            static_cast<quintptr>(loadLibrary)),
        remotePath, 0, nullptr);
    if (!thread) {
        VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        *error = QStringLiteral("无法在目标进程中创建注入线程。");
        return false;
    }

    if (WaitForSingleObject(thread, 10000) != WAIT_OBJECT_0) {
        CloseHandle(thread);
        VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        *error = QStringLiteral("注入线程等待超时（LoadLibrary 未返回）。");
        return false;
    }

    DWORD moduleHandle = 0;
    GetExitCodeThread(thread, &moduleHandle);
    CloseHandle(thread);
    VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);

    if (moduleHandle == 0) {
        *error = QStringLiteral("注入 DR2CAgent.dll 失败（LoadLibrary 返回 0）。");
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
