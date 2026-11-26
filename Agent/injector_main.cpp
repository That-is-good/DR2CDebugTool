// ============================================================================
// injector_main.cpp —— DR2CInjector32
//
// 一个最小的 32 位命令行 DLL 注入器，供 64 位 Qt 前端在自动注入失败时调用。
//
// 为什么需要它：
//   前端是 64 位进程，游戏是 32 位 WOW64 进程。从 64 位进程向 32 位进程
//   CreateRemoteThread 属于跨位数注入，在部分 Windows/安全软件组合下会被拒绝
//   （只拿到一句模糊的失败）。交给同为 32 位的本程序做 32→32 注入最稳定。
//
// 用法：DR2CInjector32.exe <pid> <dll 路径>
//   退出码 0 表示成功；其他值表示失败，失败原因以 UTF-8 写入 stderr。
// ============================================================================

#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>

#include <cstdio>
#include <cstring>
#include <cwchar>

namespace {

constexpr DWORD kProcessAccess =
    PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
    PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ;

void WriteStderr(const char *text)
{
    HANDLE handle = GetStdHandle(STD_ERROR_HANDLE);
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
        return;

    DWORD written = 0;
    WriteFile(handle, text, static_cast<DWORD>(std::strlen(text)), &written, nullptr);
}

// 统一的失败输出：阶段 + GetLastError，前端会原样展示给用户。
int FailWithLastError(int exitCode, const char *stage)
{
    const DWORD error = GetLastError();
    char buffer[512] = {0};
    std::snprintf(buffer, sizeof(buffer), "%s（Win32 错误 %lu）\r\n", stage,
                  static_cast<unsigned long>(error));
    WriteStderr(buffer);
    return exitCode;
}

bool FindKernel32Base(DWORD pid, unsigned long *base)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE)
        return false;

    bool found = false;
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Module32FirstW(snapshot, &entry)) {
        do {
            if (lstrcmpiW(entry.szModule, L"kernel32.dll") == 0) {
                *base = static_cast<unsigned long>(
                    reinterpret_cast<unsigned long long>(entry.modBaseAddr));
                found = true;
                break;
            }
        } while (Module32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return found;
}

// 校验“本进程里的 LoadLibraryW 地址在目标进程里同样有效”。
// kernel32/kernelbase 属于 KnownDLL，正常情况下全系统 32 位进程共用一个基址；
// 这里再读一次目标内存做兜底确认，避免在非标准环境下把线程带到错误地址。
bool VerifyLoadLibraryAddress(HANDLE process, const void *address)
{
    unsigned char localBytes[8] = {0};
    unsigned char remoteBytes[8] = {0};
    std::memcpy(localBytes, address, sizeof(localBytes));

    SIZE_T done = 0;
    if (!ReadProcessMemory(process, address, remoteBytes, sizeof(remoteBytes), &done) ||
        done != sizeof(remoteBytes))
        return false;
    return std::memcmp(localBytes, remoteBytes, sizeof(localBytes)) == 0;
}

int Inject(DWORD pid, const wchar_t *dllPath)
{
    if (GetFileAttributesW(dllPath) == INVALID_FILE_ATTRIBUTES) {
        WriteStderr("找不到要注入的 DLL 文件\r\n");
        return 10;
    }

    HANDLE process = OpenProcess(kProcessAccess, FALSE, pid);
    if (!process)
        return FailWithLastError(3, "OpenProcess 打开目标进程失败");

    // 本程序是 32 位：目标若不是 WOW64（即 64 位进程），就不能注入 32 位 DLL。
    // 只有在 64 位系统上 IsWow64Process 才有区分意义。
    SYSTEM_INFO systemInfo{};
    GetNativeSystemInfo(&systemInfo);
    if (systemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) {
        BOOL isWow64 = FALSE;
        if (IsWow64Process(process, &isWow64) && !isWow64) {
            CloseHandle(process);
            WriteStderr("目标进程不是 32 位程序，无法注入 32 位 DLL\r\n");
            return 2;
        }
    }

    HMODULE ownKernel32 = GetModuleHandleW(L"kernel32.dll");
    const FARPROC ownLoadLibrary = ownKernel32
        ? GetProcAddress(ownKernel32, "LoadLibraryW") : nullptr;
    if (!ownLoadLibrary) {
        CloseHandle(process);
        WriteStderr("本进程无法解析 LoadLibraryW\r\n");
        return 4;
    }

    // kernel32 属于 KnownDLL，映射地址在全系统 32 位进程中一致，
    // 所以本进程解析出的 LoadLibraryW 地址在目标进程里同样可用。
    // 先读目标内存确认一次，避免非标准环境下把线程带到错误地址。
    const unsigned long long ownLoadLibraryAddress =
        reinterpret_cast<unsigned long long>(ownLoadLibrary);
    if (!VerifyLoadLibraryAddress(process,
                                  reinterpret_cast<const void *>(ownLoadLibraryAddress))) {
        // 地址不一致时退回“目标 kernel32 基址 + 本进程导出偏移”的算法。
        unsigned long targetKernel32 = 0;
        if (!FindKernel32Base(pid, &targetKernel32)) {
            const int result = FailWithLastError(
                4, "目标进程中的 LoadLibraryW 地址与本进程不一致，且无法定位 kernel32.dll");
            CloseHandle(process);
            return result;
        }
        const unsigned long selfKernel32 = static_cast<unsigned long>(
            reinterpret_cast<unsigned long long>(ownKernel32));
        const unsigned long adjusted =
            targetKernel32 +
            (static_cast<unsigned long>(ownLoadLibraryAddress) - selfKernel32);
        if (!VerifyLoadLibraryAddress(process, reinterpret_cast<const void *>(adjusted))) {
            CloseHandle(process);
            WriteStderr("无法在目标进程中定位可用的 LoadLibraryW\r\n");
            return 4;
        }
    }

    const unsigned long loadLibraryAddress =
        static_cast<unsigned long>(ownLoadLibraryAddress);

    const SIZE_T pathSize = (wcslen(dllPath) + 1) * sizeof(wchar_t);
    void *remotePath = VirtualAllocEx(process, nullptr, pathSize,
                                      MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remotePath) {
        const int result = FailWithLastError(5, "VirtualAllocEx 分配参数内存失败");
        CloseHandle(process);
        return result;
    }

    SIZE_T written = 0;
    if (!WriteProcessMemory(process, remotePath, dllPath, pathSize, &written) ||
        written != pathSize) {
        const int result = FailWithLastError(6, "WriteProcessMemory 写入 DLL 路径失败");
        VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        CloseHandle(process);
        return result;
    }

    HANDLE thread = CreateRemoteThread(
        process, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(loadLibraryAddress),
        remotePath, 0, nullptr);
    if (!thread) {
        const int result = FailWithLastError(7, "CreateRemoteThread 创建注入线程失败");
        VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        CloseHandle(process);
        return result;
    }

    const DWORD wait = WaitForSingleObject(thread, 15000);
    DWORD moduleHandle = 0;
    GetExitCodeThread(thread, &moduleHandle);
    CloseHandle(thread);
    VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
    CloseHandle(process);

    if (wait != WAIT_OBJECT_0) {
        WriteStderr("等待 LoadLibraryW 返回超时\r\n");
        return 8;
    }
    if (moduleHandle == 0) {
        WriteStderr("LoadLibraryW 返回 0（DLL 依赖缺失、位数不符或被安全软件拦截）\r\n");
        return 9;
    }

    WriteStderr("注入成功\r\n");
    return 0;
}

} // namespace

int main()
{
    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        WriteStderr("无法解析命令行\r\n");
        return 1;
    }
    if (argc < 3) {
        WriteStderr("用法：DR2CInjector32.exe <pid> <dll 路径>\r\n");
        LocalFree(argv);
        return 1;
    }

    const DWORD pid = static_cast<DWORD>(wcstoul(argv[1], nullptr, 10));
    if (pid == 0) {
        WriteStderr("PID 无效\r\n");
        LocalFree(argv);
        return 1;
    }

    const int result = Inject(pid, argv[2]);
    LocalFree(argv);
    return result;
}
