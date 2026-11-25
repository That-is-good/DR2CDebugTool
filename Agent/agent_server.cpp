#include "agent_server.h"

#include "../Common/protocol.h"
#include "agent_memory.h"
#include "agent_offsets.h"
#include "agent_resolver.h"

#include <windows.h>

#include <algorithm>
#include <csetjmp>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

namespace dr2c {
namespace {

constexpr std::uint32_t kAgentVersion = 1;
constexpr std::uint32_t kPipeBufferSize = 64u * 1024u;

bool EnsureLayoutResolved();

bool g_stop = false;
HANDLE g_mutex = nullptr;
HINSTANCE g_instance = nullptr;

LayoutInfo g_layout{};
std::uint32_t g_moduleBase = 0;
bool g_layoutResolved = false;

// ---------------------------------------------------------------------------
// 函数调用保护
// GCC/MinGW 32 位没有 __try/__except，这里用“向量化异常处理 + setjmp”兜底：
// 调用游戏函数期间一旦发生访问冲突，直接跳回调用点并标记失败，避免整局崩溃。
// ---------------------------------------------------------------------------
thread_local jmp_buf *t_jumpTarget = nullptr;

LONG CALLBACK GuardHandler(PEXCEPTION_POINTERS info)
{
    if (t_jumpTarget &&
        (info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ||
         info->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION ||
         info->ExceptionRecord->ExceptionCode == EXCEPTION_INT_DIVIDE_BY_ZERO)) {
        jmp_buf *target = t_jumpTarget;
        t_jumpTarget = nullptr;
        longjmp(*target, 1);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

template <typename Callable>
bool RunGuarded(Callable &&callable)
{
    jmp_buf buffer;
    t_jumpTarget = &buffer;
    if (setjmp(buffer) == 0) {
        callable();
        t_jumpTarget = nullptr;
        return true;
    }
    t_jumpTarget = nullptr;
    return false;
}

// Route 函数统一签名：所有参数都按 32 位整数传入。
using FnScript   = void(__cdecl *)(const char *);
using FnVoidPtr  = void(__cdecl *)(void *);
using FnPtrInt   = void *(__cdecl *)(int);
using FnPtrVoid  = void *(__cdecl *)();
using FnIntPtr   = int(__cdecl *)(void *, unsigned int);
using FnUInt     = void(__cdecl *)(unsigned int);

bool InvokeFunction(std::uint32_t functionId, const std::uint64_t *args,
                    std::uint32_t argCount, const std::string &scriptText,
                    std::uint64_t *returnValue)
{
    if (!returnValue)
        return false;
    *returnValue = 0;
    if (!EnsureLayoutResolved() || functionId >= kFunctionCount)
        return false;

    const std::uint32_t rva = g_layout.functionRvas[functionId];
    if (rva == 0)
        return false;

    const Address target = static_cast<Address>(g_moduleBase) + rva;
    auto arg = [&](std::uint32_t index) -> std::uint32_t {
        return index < argCount ? static_cast<std::uint32_t>(args[index]) : 0u;
    };

    switch (static_cast<FunctionId>(functionId)) {
    case FunctionId::ScriptEvaluateStringSafe: {
        FnScript fn = reinterpret_cast<FnScript>(target);
        const char *text = scriptText.c_str();
        return RunGuarded([&] { fn(text); });
    }
    case FunctionId::FreeThing: {
        FnVoidPtr fn = reinterpret_cast<FnVoidPtr>(target);
        void *pointer = reinterpret_cast<void *>(static_cast<Address>(arg(0)));
        return RunGuarded([&] { fn(pointer); });
    }
    case FunctionId::AllocateEntity: {
        FnPtrInt fn = reinterpret_cast<FnPtrInt>(target);
        void *result = nullptr;
        const bool ok = RunGuarded([&] { result = fn(static_cast<int>(arg(0))); });
        if (ok)
            *returnValue = static_cast<std::uint32_t>(reinterpret_cast<Address>(result));
        return ok;
    }
    case FunctionId::AllocateThing: {
        FnPtrInt fn = reinterpret_cast<FnPtrInt>(target);
        void *result = nullptr;
        const bool ok = RunGuarded([&] { result = fn(static_cast<int>(arg(0))); });
        if (ok)
            *returnValue = static_cast<std::uint32_t>(reinterpret_cast<Address>(result));
        return ok;
    }
    case FunctionId::AllocateCharacterSlot: {
        FnPtrVoid fn = reinterpret_cast<FnPtrVoid>(target);
        void *result = nullptr;
        const bool ok = RunGuarded([&] { result = fn(); });
        if (ok)
            *returnValue = static_cast<std::uint32_t>(reinterpret_cast<Address>(result));
        return ok;
    }
    case FunctionId::AssignCharacterToThing: {
        FnIntPtr fn = reinterpret_cast<FnIntPtr>(target);
        void *thing = reinterpret_cast<void *>(static_cast<Address>(arg(0)));
        int result = 0;
        const bool ok = RunGuarded([&] { result = fn(thing, arg(1)); });
        if (ok)
            *returnValue = static_cast<std::uint32_t>(result);
        return ok;
    }
    case FunctionId::SwitchActiveTileLayer: {
        FnUInt fn = reinterpret_cast<FnUInt>(target);
        return RunGuarded([&] { fn(arg(0)); });
    }
    case FunctionId::SetCurrentPlayerThing: {
        FnVoidPtr fn = reinterpret_cast<FnVoidPtr>(target);
        void *thing = reinterpret_cast<void *>(static_cast<Address>(arg(0)));
        return RunGuarded([&] { fn(thing); });
    }
    case FunctionId::CreateWeaponEntity: {
        FnPtrInt fn = reinterpret_cast<FnPtrInt>(target);
        void *result = nullptr;
        const bool ok = RunGuarded([&] { result = fn(static_cast<int>(arg(0))); });
        if (ok)
            *returnValue = static_cast<std::uint32_t>(reinterpret_cast<Address>(result));
        return ok;
    }
    default:
        return false;
    }
}

// ---------------------------------------------------------------------------
// 布局解析
// ---------------------------------------------------------------------------
std::uint32_t ReadImageSize(std::uint32_t moduleBase)
{
    IMAGE_DOS_HEADER dos{};
    if (!SafeRead(moduleBase, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE)
        return 0;
    const Address nt = static_cast<Address>(moduleBase) + static_cast<std::uint32_t>(dos.e_lfanew);
    std::uint32_t signature = 0;
    IMAGE_FILE_HEADER fileHeader{};
    IMAGE_OPTIONAL_HEADER32 optional{};
    if (!SafeRead(nt, &signature, sizeof(signature)) || signature != IMAGE_NT_SIGNATURE ||
        !SafeRead(nt + sizeof(signature), &fileHeader, sizeof(fileHeader)) ||
        !SafeRead(nt + sizeof(signature) + sizeof(fileHeader), &optional, sizeof(optional)))
        return 0;
    return optional.SizeOfImage;
}

bool EnsureLayoutResolved()
{
    if (g_layoutResolved)
        return g_layout.valid != 0;

    g_moduleBase = static_cast<std::uint32_t>(
        reinterpret_cast<Address>(GetModuleHandleW(nullptr)));
    g_layoutResolved = true;
    return ResolveLayout(g_moduleBase, &g_layout);
}

// ---------------------------------------------------------------------------
// 命令实现
// ---------------------------------------------------------------------------
template <typename T>
bool ReadStruct(const std::uint8_t *payload, std::uint32_t size, T *value)
{
    if (!value || size < sizeof(T))
        return false;
    std::memcpy(value, payload, sizeof(T));
    return true;
}

template <typename T>
void WriteStruct(std::vector<std::uint8_t> &output, const T &value)
{
    output.resize(sizeof(T));
    std::memcpy(output.data(), &value, sizeof(T));
}

std::uint32_t HandleRead(const std::uint8_t *payload, std::uint32_t size,
                         std::vector<std::uint8_t> &output)
{
    ReadRequest request{};
    if (!ReadStruct(payload, size, &request))
        return static_cast<std::uint32_t>(Status::BadRequest);
    if (request.size > kMaxPayload)
        return static_cast<std::uint32_t>(Status::BadRequest);
    if (request.size == 0)
        return static_cast<std::uint32_t>(Status::Ok);

    const Address address = static_cast<Address>(request.address);
    output.resize(request.size);
    if (!SafeRead(address, output.data(), request.size)) {
        output.clear();
        return static_cast<std::uint32_t>(Status::Error);
    }
    return static_cast<std::uint32_t>(Status::Ok);
}

std::uint32_t HandleWrite(const std::uint8_t *payload, std::uint32_t size)
{
    WriteRequestHeader header{};
    if (!ReadStruct(payload, size, &header))
        return static_cast<std::uint32_t>(Status::BadRequest);
    if (header.size > kMaxPayload || size != sizeof(header) + header.size)
        return static_cast<std::uint32_t>(Status::BadRequest);
    if (header.size == 0)
        return static_cast<std::uint32_t>(Status::Ok);

    if (!SafeWrite(static_cast<Address>(header.address), payload + sizeof(header), header.size))
        return static_cast<std::uint32_t>(Status::Error);
    return static_cast<std::uint32_t>(Status::Ok);
}

std::uint32_t HandleBatchRead(const std::uint8_t *payload, std::uint32_t size,
                              std::vector<std::uint8_t> &output)
{
    BatchReadHeader header{};
    if (!ReadStruct(payload, size, &header))
        return static_cast<std::uint32_t>(Status::BadRequest);
    if (header.count == 0 || header.count > kMaxBatchEntries)
        return static_cast<std::uint32_t>(Status::BadRequest);
    if (size != sizeof(header) + header.count * sizeof(BatchReadEntry))
        return static_cast<std::uint32_t>(Status::BadRequest);

    const BatchReadEntry *entries =
        reinterpret_cast<const BatchReadEntry *>(payload + sizeof(header));

    std::uint64_t total = 0;
    for (std::uint32_t i = 0; i < header.count; ++i)
        total += entries[i].size;
    if (total > kMaxPayload)
        return static_cast<std::uint32_t>(Status::BadRequest);

    output.resize(static_cast<std::size_t>(total));
    std::size_t offset = 0;
    for (std::uint32_t i = 0; i < header.count; ++i) {
        if (entries[i].size == 0)
            continue;
        if (!SafeRead(static_cast<Address>(entries[i].address),
                      output.data() + offset, entries[i].size)) {
            output.resize(sizeof(std::uint32_t));
            std::memcpy(output.data(), &i, sizeof(std::uint32_t));
            return static_cast<std::uint32_t>(Status::Error);
        }
        offset += entries[i].size;
    }
    return static_cast<std::uint32_t>(Status::Ok);
}

std::uint32_t HandleGetLayout(std::vector<std::uint8_t> &output)
{
    g_moduleBase = static_cast<std::uint32_t>(
        reinterpret_cast<Address>(GetModuleHandleW(nullptr)));
    g_layoutResolved = true;
    ResolveLayout(g_moduleBase, &g_layout);
    WriteStruct(output, g_layout);
    return static_cast<std::uint32_t>(Status::Ok);
}

std::uint32_t HandleCall(const std::uint8_t *payload, std::uint32_t size,
                         std::vector<std::uint8_t> &output)
{
    CallRequest request{};
    if (!ReadStruct(payload, size, &request))
        return static_cast<std::uint32_t>(Status::BadRequest);
    if (request.argCount > 8 ||
        size < sizeof(request) + request.argCount * sizeof(std::uint64_t))
        return static_cast<std::uint32_t>(Status::BadRequest);

    const std::uint64_t *args =
        reinterpret_cast<const std::uint64_t *>(payload + sizeof(request));

    std::string scriptText;
    if (request.functionId ==
        static_cast<std::uint32_t>(FunctionId::ScriptEvaluateStringSafe)) {
        const std::size_t used = sizeof(request) + request.argCount * sizeof(std::uint64_t);
        scriptText.assign(reinterpret_cast<const char *>(payload + used), size - used);
    }

    std::uint64_t returnValue = 0;
    const bool ok = InvokeFunction(request.functionId, args, request.argCount,
                                   scriptText, &returnValue);

    CallResponse response{};
    response.ok = ok ? 1u : 0u;
    response.returnValue = returnValue;
    WriteStruct(output, response);
    return static_cast<std::uint32_t>(ok ? Status::Ok : Status::Exception);
}

std::uint32_t HandleHover(const std::uint8_t *payload, std::uint32_t size,
                          std::vector<std::uint8_t> &output)
{
    HoverRequest request{};
    if (!ReadStruct(payload, size, &request))
        return static_cast<std::uint32_t>(Status::BadRequest);
    if (!EnsureLayoutResolved())
        return static_cast<std::uint32_t>(Status::Unsupported);

    HoverResponse response{};
    float cameraX = 0.0f;
    float cameraY = 0.0f;
    float scale = 0.0f;
    if (!SafeLoad(static_cast<Address>(g_moduleBase) + Offset::CameraX, cameraX) ||
        !SafeLoad(static_cast<Address>(g_moduleBase) + Offset::CameraY, cameraY) ||
        !SafeLoad(static_cast<Address>(g_moduleBase) + Offset::ScreenScale, scale)) {
        WriteStruct(output, response);
        return static_cast<std::uint32_t>(Status::Error);
    }
    if (!(scale > 0.5f && scale < 32.0f))
        scale = 4.0f;

    std::int32_t mapId = 0;
    SafeLoad(static_cast<Address>(g_moduleBase) + g_layout.currentMapIdRva, mapId);

    const std::uint32_t stride = g_layout.thingStride
        ? g_layout.thingStride : Offset::Thing::Stride;
    const std::uint32_t poolSize = stride * Offset::Thing::MaxSlots;
    if (g_layout.thingPoolRva == 0 || poolSize == 0)
        return static_cast<std::uint32_t>(Status::Unsupported);

    static std::vector<std::uint8_t> pool;
    if (pool.size() < poolSize)
        pool.resize(poolSize);

    const Address poolBase = static_cast<Address>(g_moduleBase) + g_layout.thingPoolRva;
    if (!SafeRead(poolBase, pool.data(), poolSize)) {
        WriteStruct(output, response);
        return static_cast<std::uint32_t>(Status::Error);
    }

    const float worldX =
        cameraX + (static_cast<float>(request.mouseX) - request.clientWidth * 0.5f) / scale;
    const float worldY =
        cameraY + (static_cast<float>(request.mouseY) - request.clientHeight * 0.5f) / scale;

    float bestDistance = 1.0e30f;
    std::uint32_t bestSlot = 0;
    for (std::uint32_t slot = 1; slot < Offset::Thing::MaxSlots; ++slot) {
        const std::uint8_t *thing = pool.data() + static_cast<std::size_t>(slot) * stride;
        std::uint16_t id = 0;
        std::memcpy(&id, thing + Offset::Thing::Id, sizeof(id));
        if (id == 0)
            continue;
        if (thing[Offset::Thing::MapId] != static_cast<std::uint8_t>(mapId))
            continue;
        float x = 0.0f;
        float y = 0.0f;
        std::memcpy(&x, thing + Offset::Thing::Position, sizeof(x));
        std::memcpy(&y, thing + Offset::Thing::Position + 4, sizeof(y));
        const float dx = x - worldX;
        const float dy = y - worldY;
        const float distance = dx * dx + dy * dy;
        if (distance < bestDistance) {
            bestDistance = distance;
            bestSlot = slot;
        }
    }

    float pickRadius = Offset::PickRadiusWorld;
    if (request.pickRadiusPx > 0)
        pickRadius = static_cast<float>(request.pickRadiusPx) / scale;
    if (bestSlot == 0 || bestDistance > pickRadius * pickRadius) {
        WriteStruct(output, response); // found = 0
        return static_cast<std::uint32_t>(Status::Ok);
    }

    const std::uint8_t *thing = pool.data() + static_cast<std::size_t>(bestSlot) * stride;
    float x = 0.0f;
    float y = 0.0f;
    std::memcpy(&x, thing + Offset::Thing::Position, sizeof(x));
    std::memcpy(&y, thing + Offset::Thing::Position + 4, sizeof(y));

    response.found = 1;
    std::memcpy(&response.thingId, thing + Offset::Thing::Id, sizeof(std::uint16_t));
    response.slot = bestSlot;
    response.type = thing[Offset::Thing::Type];
    response.subtype = thing[Offset::Thing::Subtype];
    response.mapId = thing[Offset::Thing::MapId];
    std::memcpy(&response.spriteId, thing + Offset::Thing::SpriteId, sizeof(std::uint16_t));
    response.screenX = static_cast<std::int32_t>(
        (x - cameraX) * scale + request.clientWidth * 0.5f);
    response.screenY = static_cast<std::int32_t>(
        (y - cameraY) * scale + request.clientHeight * 0.5f);
    const float box = std::max(Offset::MinHighlightPx, pickRadius * scale);
    response.rectLeft = static_cast<std::int32_t>(response.screenX - box);
    response.rectTop = static_cast<std::int32_t>(response.screenY - box);
    response.rectRight = static_cast<std::int32_t>(response.screenX + box);
    response.rectBottom = static_cast<std::int32_t>(response.screenY + box);
    response.worldX = x;
    response.worldY = y;
    response.thingAddress =
        static_cast<std::uint64_t>(poolBase) + static_cast<std::uint64_t>(bestSlot) * stride;
    WriteStruct(output, response);
    return static_cast<std::uint32_t>(Status::Ok);
}

bool IsThingAddress(std::uint64_t address)
{
    if (!EnsureLayoutResolved() || g_layout.thingPoolRva == 0)
        return false;
    const std::uint32_t stride = g_layout.thingStride
        ? g_layout.thingStride : Offset::Thing::Stride;
    const std::uint64_t poolBase = static_cast<std::uint64_t>(g_moduleBase) + g_layout.thingPoolRva;
    const std::uint64_t poolEnd = poolBase + static_cast<std::uint64_t>(stride) * Offset::Thing::MaxSlots;
    return address >= poolBase && address + stride <= poolEnd;
}

std::uint32_t HandleDragBegin(const std::uint8_t *payload, std::uint32_t size)
{
    DragBeginRequest request{};
    if (!ReadStruct(payload, size, &request))
        return static_cast<std::uint32_t>(Status::BadRequest);
    if (!IsThingAddress(request.address))
        return static_cast<std::uint32_t>(Status::BadRequest);
    return static_cast<std::uint32_t>(Status::Ok);
}

std::uint32_t HandleDragMove(const std::uint8_t *payload, std::uint32_t size)
{
    DragMoveRequest request{};
    if (!ReadStruct(payload, size, &request))
        return static_cast<std::uint32_t>(Status::BadRequest);
    if (!IsThingAddress(request.address))
        return static_cast<std::uint32_t>(Status::BadRequest);

    float cameraX = 0.0f;
    float cameraY = 0.0f;
    float scale = 0.0f;
    if (!SafeLoad(static_cast<Address>(g_moduleBase) + Offset::CameraX, cameraX) ||
        !SafeLoad(static_cast<Address>(g_moduleBase) + Offset::CameraY, cameraY) ||
        !SafeLoad(static_cast<Address>(g_moduleBase) + Offset::ScreenScale, scale))
        return static_cast<std::uint32_t>(Status::Error);
    if (!(scale > 0.5f && scale < 32.0f))
        scale = 4.0f;

    const float worldX =
        cameraX + (static_cast<float>(request.mouseX) - request.clientWidth * 0.5f) / scale;
    const float worldY =
        cameraY + (static_cast<float>(request.mouseY) - request.clientHeight * 0.5f) / scale;

    const Address position = static_cast<Address>(request.address) + Offset::Thing::Position;
    const float value[2] = {worldX, worldY};
    return static_cast<std::uint32_t>(
        SafeWrite(position, value, sizeof(value)) ? Status::Ok : Status::Error);
}

std::uint32_t HandleDragEnd(const std::uint8_t *payload, std::uint32_t size)
{
    DragEndRequest request{};
    if (!ReadStruct(payload, size, &request))
        return static_cast<std::uint32_t>(Status::BadRequest);
    if (!IsThingAddress(request.address))
        return static_cast<std::uint32_t>(Status::BadRequest);
    return static_cast<std::uint32_t>(Status::Ok);
}

// ---------------------------------------------------------------------------
// 管道服务
// ---------------------------------------------------------------------------
std::uint32_t Dispatch(std::uint16_t command, const std::uint8_t *payload,
                       std::uint32_t size, std::vector<std::uint8_t> &output)
{
    switch (static_cast<Command>(command)) {
    case Command::Ping: {
        PingRequest request{};
        if (!ReadStruct(payload, size, &request))
            return static_cast<std::uint32_t>(Status::BadRequest);
        PingResponse response{};
        response.cookie = request.cookie;
        response.pid = GetCurrentProcessId();
        WriteStruct(output, response);
        return static_cast<std::uint32_t>(Status::Ok);
    }
    case Command::GetInfo: {
        const std::uint32_t moduleBase = static_cast<std::uint32_t>(
            reinterpret_cast<Address>(GetModuleHandleW(nullptr)));
        InfoResponse response{};
        response.pid = GetCurrentProcessId();
        response.moduleBase = moduleBase;
        response.imageSize = ReadImageSize(moduleBase);
        response.agentVersion = kAgentVersion;
        WriteStruct(output, response);
        return static_cast<std::uint32_t>(Status::Ok);
    }
    case Command::Read:
        return HandleRead(payload, size, output);
    case Command::Write:
        return HandleWrite(payload, size);
    case Command::BatchRead:
        return HandleBatchRead(payload, size, output);
    case Command::GetLayout:
        return HandleGetLayout(output);
    case Command::Call:
        return HandleCall(payload, size, output);
    case Command::HoverThing:
        return HandleHover(payload, size, output);
    case Command::DragBegin:
        return HandleDragBegin(payload, size);
    case Command::DragMove:
        return HandleDragMove(payload, size);
    case Command::DragEnd:
        return HandleDragEnd(payload, size);
    case Command::Shutdown:
        g_stop = true;
        return static_cast<std::uint32_t>(Status::Ok);
    default:
        return static_cast<std::uint32_t>(Status::Unsupported);
    }
}

// 把无符号整数以十进制追加到宽字符缓冲区（保证 NUL 结尾），返回新长度。
std::size_t AppendUnsigned(wchar_t *buffer, std::size_t length,
                           std::size_t capacity, unsigned value)
{
    wchar_t digits[16];
    std::size_t count = 0;
    do {
        digits[count++] = static_cast<wchar_t>(L'0' + value % 10u);
        value /= 10u;
    } while (value != 0u);

    while (count > 0 && length + 1 < capacity)
        buffer[length++] = digits[--count];
    buffer[length] = L'\0';
    return length;
}

bool ReadExact(HANDLE pipe, void *buffer, std::uint32_t size)
{
    std::uint8_t *data = static_cast<std::uint8_t *>(buffer);
    std::uint32_t total = 0;
    while (total < size) {
        DWORD received = 0;
        if (!ReadFile(pipe, data + total, size - total, &received, nullptr) || received == 0)
            return false;
        total += received;
    }
    return true;
}

bool WriteExact(HANDLE pipe, const void *buffer, std::uint32_t size)
{
    const std::uint8_t *data = static_cast<const std::uint8_t *>(buffer);
    std::uint32_t total = 0;
    while (total < size) {
        DWORD written = 0;
        if (!WriteFile(pipe, data + total, size - total, &written, nullptr) || written == 0)
            return false;
        total += written;
    }
    return true;
}

void ServeClient(HANDLE pipe)
{
    while (!g_stop) {
        PacketHeader header{};
        if (!ReadExact(pipe, &header, sizeof(header)))
            return;
        if (header.magic != kPipeMagic || header.version != kProtocolVersion ||
            header.payloadLen > kMaxPayload)
            return;

        std::vector<std::uint8_t> payload(header.payloadLen);
        if (header.payloadLen && !ReadExact(pipe, payload.data(), header.payloadLen))
            return;

        std::vector<std::uint8_t> response;
        const std::uint32_t status = Dispatch(header.cmd,
                                              payload.empty() ? nullptr : payload.data(),
                                              header.payloadLen, response);

        PacketHeader reply{};
        reply.magic = kPipeMagic;
        reply.version = kProtocolVersion;
        reply.cmd = header.cmd;
        reply.seq = header.seq;
        reply.status = status;
        reply.payloadLen = static_cast<std::uint32_t>(response.size());
        if (!WriteExact(pipe, &reply, sizeof(reply)))
            return;
        if (!response.empty() && !WriteExact(pipe, response.data(),
                                             static_cast<std::uint32_t>(response.size())))
            return;
    }
}

DWORD WINAPI ServerThread(LPVOID)
{
    // 手工拼接进程号，避免依赖 MinGW 的 swprintf 变体。
    wchar_t pipeName[64] = L"\\\\.\\pipe\\DR2CTool_";
    AppendUnsigned(pipeName, std::wcslen(pipeName), 64,
                   static_cast<unsigned>(GetCurrentProcessId()));

    while (!g_stop) {
        HANDLE pipe = CreateNamedPipeW(
            pipeName, PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1, kPipeBufferSize, kPipeBufferSize, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            Sleep(500);
            continue;
        }

        const BOOL connected = ConnectNamedPipe(pipe, nullptr);
        if (!connected && GetLastError() != ERROR_PIPE_CONNECTED) {
            CloseHandle(pipe);
            continue;
        }

        ServeClient(pipe);

        FlushFileBuffers(pipe);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }
    return 0;
}

} // namespace

void StartServer(HINSTANCE instance)
{
    g_instance = instance;

    wchar_t mutexName[64] = L"Global\\DR2CTool_";
    AppendUnsigned(mutexName, std::wcslen(mutexName), 64,
                   static_cast<unsigned>(GetCurrentProcessId()));
    g_mutex = CreateMutexW(nullptr, FALSE, mutexName);
    if (g_mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        // 已经注入过，服务线程在运行，无需重复启动。
        CloseHandle(g_mutex);
        g_mutex = nullptr;
        return;
    }

    AddVectoredExceptionHandler(1, GuardHandler);

    HANDLE thread = CreateThread(nullptr, 0, ServerThread, nullptr, 0, nullptr);
    if (thread)
        CloseHandle(thread);
}

} // namespace dr2c
