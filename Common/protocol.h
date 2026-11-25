#ifndef DR2C_COMMON_PROTOCOL_H
#define DR2C_COMMON_PROTOCOL_H

// ============================================================================
// protocol.h
// Qt 前端（64 位）与注入 Agent（32 位）之间的唯一协议声明。
//
// 约定：
//   - 不依赖 Qt，也不依赖 Windows 头文件；只使用定宽整数。
//   - 结构体全部 1 字节对齐（#pragma pack(1)），两端逐字节一致。
//   - 小端；请求与响应一一对应，同步收发。
//   - 帧头固定 20 字节；载荷长度由帧头 payloadLen 决定。
// ============================================================================

#include <cstdint>

namespace dr2c {

// 'DR2A'：0x41='A' 0x32='2' 0x52='R' 0x44='D'（按小端读出的常量）
constexpr std::uint32_t kPipeMagic = 0x41325244u;
constexpr std::uint16_t kProtocolVersion = 1;

// 单条消息的最大载荷（.text 段回读可能达到数 MB）
constexpr std::uint32_t kMaxPayload = 32u * 1024u * 1024u;

// BatchRead 单次最多条目数
constexpr std::uint32_t kMaxBatchEntries = 4096u;

// 管道名前缀：最终名称 = \\.\pipe\DR2CTool_<pid>
// Agent 端互斥体名称 = Global\DR2CTool_<pid>
constexpr char kPipeNamePrefix[] = "\\\\.\\pipe\\DR2CTool_";

enum class Command : std::uint16_t {
    Ping = 1,
    GetInfo = 2,
    Read = 3,
    Write = 4,
    BatchRead = 5,
    GetLayout = 6,
    Call = 7,
    HoverThing = 8,
    DragBegin = 9,
    DragEnd = 10,
    Shutdown = 11,
    DragMove = 12,
};

enum class Status : std::uint32_t {
    Ok = 0,
    Error = 1,        // 一般性失败（读取越界、调用返回失败等）
    BadRequest = 2,   // 参数非法 / 长度不符
    Exception = 3,    // 游戏函数执行时发生异常，已被保护捕获
    Unsupported = 4,  // 该游戏版本缺少所需函数/字段
};

// 游戏函数编号。顺序必须与 Agent 端 InvokeFunction 的 switch 完全一致。
enum class FunctionId : std::uint32_t {
    ScriptEvaluateStringSafe = 0,
    FreeThing,
    AllocateEntity,
    AllocateThing,
    AllocateCharacterSlot,
    AssignCharacterToThing,
    SwitchActiveTileLayer,
    SetCurrentPlayerThing,
    CreateWeaponEntity,
    Count,
};

constexpr std::uint32_t kFunctionCount =
    static_cast<std::uint32_t>(FunctionId::Count);

#pragma pack(push, 1)

struct PacketHeader {
    std::uint32_t magic;      // kPipeMagic
    std::uint16_t version;    // kProtocolVersion
    std::uint16_t cmd;        // Command
    std::uint32_t seq;        // 请求序号，响应原样带回
    std::uint32_t status;     // 请求填 0；响应填 Status
    std::uint32_t payloadLen; // 载荷字节数
};

// ---------------- Ping ----------------
struct PingRequest {
    std::uint32_t cookie;
};

struct PingResponse {
    std::uint32_t cookie;
    std::uint32_t pid;
};

// ---------------- GetInfo ----------------
struct InfoResponse {
    std::uint32_t pid;
    std::uint32_t moduleBase; // 游戏主模块基址（32 位进程内地址）
    std::uint32_t imageSize;
    std::uint32_t agentVersion;
};

// ---------------- Read / Write ----------------
struct ReadRequest {
    std::uint64_t address;
    std::uint32_t size;
};

struct WriteRequestHeader {
    std::uint64_t address;
    std::uint32_t size;
    // 之后紧跟 size 字节数据
};

// BatchRead 请求：count 个 {address,size} 条目；
// 响应：按顺序拼接的数据。任一条目失败则整体返回 Status::Error，
// 载荷为失败的条目下标（uint32）。
struct BatchReadHeader {
    std::uint32_t count;
};

struct BatchReadEntry {
    std::uint64_t address;
    std::uint32_t size;
};

// ---------------- GetLayout ----------------
struct LayoutInfo {
    std::uint32_t valid;
    std::uint32_t imageBase;
    std::uint32_t imageSize;

    std::uint32_t functionRvas[kFunctionCount];

    std::uint32_t thingPoolRva;
    std::uint32_t weaponPoolRva;
    std::uint32_t missionStateRva;
    std::uint32_t characterPoolPointerRva;
    std::uint32_t characterPoolRva;
    std::uint32_t mapLayerMetaRva;
    std::uint32_t currentMapIdRva;
    std::uint32_t nextThingIdRva;
    std::uint32_t cameraMapLayerRva;
    std::uint32_t playerThingArrayRva;

    std::uint32_t thingStride;
    std::uint32_t characterStride;
    std::uint32_t weaponStride;
    std::uint32_t mapMetaStride;

    std::uint32_t codeSectionRva;
    std::uint32_t codeSectionSize;

    char warning[256];
    char error[256];
};

// ---------------- Call ----------------
// payload = CallRequest + argCount 个 uint64 参数。
// 特殊约定：functionId == ScriptEvaluateStringSafe 时忽略 args[0]，
// 参数之后剩余字节被视为 UTF-8 脚本字符串（不需要 NUL 结尾）。
struct CallRequest {
    std::uint32_t functionId;
    std::uint32_t argCount;
};

struct CallResponse {
    std::uint32_t ok; // 0 失败，1 成功
    std::uint32_t reserved;
    std::uint64_t returnValue;
};

// ---------------- HoverThing ----------------
// 鼠标坐标与客户区尺寸均由前端提供，避免 Agent 猜测窗口。
struct HoverRequest {
    std::int32_t mouseX;      // 游戏客户区坐标
    std::int32_t mouseY;
    std::int32_t clientWidth;
    std::int32_t clientHeight;
    std::uint32_t pickRadiusPx; // 屏幕像素半径，0 表示使用默认值
};

struct HoverResponse {
    std::uint32_t found;
    std::uint32_t thingId;
    std::uint32_t slot;
    std::uint32_t type;
    std::uint32_t subtype;
    std::uint32_t mapId;
    std::uint32_t spriteId;
    std::int32_t screenX; // 实体中心，客户区坐标
    std::int32_t screenY;
    std::int32_t rectLeft; // 高亮矩形，客户区坐标
    std::int32_t rectTop;
    std::int32_t rectRight;
    std::int32_t rectBottom;
    float worldX;
    float worldY;
    std::uint64_t thingAddress; // 该实体在目标进程中的地址
};

// ---------------- Drag ----------------
struct DragBeginRequest {
    std::uint64_t address;
};

// 拖动过程中持续上报鼠标位置，由 Agent 换算世界坐标并写回实体
struct DragMoveRequest {
    std::uint64_t address;
    std::int32_t mouseX;
    std::int32_t mouseY;
    std::int32_t clientWidth;
    std::int32_t clientHeight;
};

// 拖动结束
struct DragEndRequest {
    std::uint64_t address;
};

#pragma pack(pop)

static_assert(sizeof(PacketHeader) == 20, "PacketHeader 必须为 20 字节");
static_assert(sizeof(ReadRequest) == 12, "ReadRequest 必须为 12 字节");
static_assert(sizeof(WriteRequestHeader) == 12, "WriteRequestHeader 必须为 12 字节");

} // namespace dr2c

#endif // DR2C_COMMON_PROTOCOL_H
