#include "imgui_ui.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"
#include "dr2c_memory.h"
#include "dr2c_offsets.h"
#include "translation.h"
#include <windows.h>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <cfloat>
#include <cstdio>
#include <fstream>
#include <vector>
#include <string>

using namespace dr2c;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam);

namespace {

using SlotIndex = int;

// ==================== 模块基址 + 函数指针（一次缓存） ====================
// 地址统一为 dr2c::Address（见 dr2c_memory.h），偏移统一取自 dr2c_offsets.h
Address g_moduleBase = 0;

using fn_AllocateEntity_t   = Address(__cdecl*)(std::uint8_t);
using fn_FreeThing_t        = void(__cdecl*)(Address);
using fn_GetCharacterData_t = Address(__cdecl*)(std::uint32_t);
using fn_GetFrameRate       = std::uint32_t(__cdecl*)();
using fn_SetFrameRate       = std::uint32_t(__cdecl*)(std::uint32_t);
// using fn_GetCurrentMapLayer = std::uint32_t(__cdecl*)();
// using fn_SetCurrentMapLayer = std::uint32_t(__cdecl*)(std::uint32_t);
using fn_SetCurrentPlayerThing = void(__cdecl*)(Address);

fn_AllocateEntity_t   pAllocateEntity   = nullptr;
fn_FreeThing_t        pFreeThing        = nullptr;
fn_GetCharacterData_t pGetCharacterData = nullptr;
fn_GetFrameRate       pGetFrameRate     = nullptr;
fn_SetFrameRate       pSetFrameRate     = nullptr;
fn_SetCurrentPlayerThing pSetCurrentPlayerThing = nullptr;
// fn_GetCurrentMapLayer pGetCurrentMapLayer = nullptr;
// fn_SetCurrentMapLayer pSetCurrentMapLayer = nullptr;


void CacheGameFunctions()
{
    g_moduleBase      = Addr(GetModuleHandleW(nullptr));
    pAllocateEntity   = AsFn<fn_AllocateEntity_t>  (g_moduleBase + Offset::Fn::AllocateEntity);
    pFreeThing        = AsFn<fn_FreeThing_t>       (g_moduleBase + Offset::Fn::FreeThing);
    pGetCharacterData = AsFn<fn_GetCharacterData_t>(g_moduleBase + Offset::Fn::GetCharacterData);
    pGetFrameRate     = AsFn<fn_GetFrameRate>      (g_moduleBase + Offset::Fn::GetFrameRate);
    pSetFrameRate     = AsFn<fn_SetFrameRate>      (g_moduleBase + Offset::Fn::SetFrameRate);
    pSetCurrentPlayerThing = AsFn<fn_SetCurrentPlayerThing>(g_moduleBase + Offset::Fn::SetCurrentPlayerThing);
    // pGetCurrentMapLayer     = AsFn<fn_GetCurrentMapLayer>(g_moduleBase + 0x07D2E0u);
    // pSetCurrentMapLayer     = AsFn<fn_SetCurrentMapLayer>(g_moduleBase + 0x07D310u);
}

// ==================== 槽位数量 ====================
// 全部偏移/大小/数量只在 dr2c_offsets.h 中声明，这里只做短别名
constexpr SlotIndex kThingMaxSlots  = Offset::Thing::MaxSlots;
constexpr SlotIndex kWeaponMaxCount = Offset::Weapon::MaxCount;

// 实体槽位 -> 内存地址（唯一入口）
inline Address EntityAddress(SlotIndex slot)
{
    return g_moduleBase + Offset::Global::ThingPool
           + static_cast<Address>(slot) * Offset::Thing::Stride;
}

// 实体槽位 -> 字节指针（仅用于按偏移访问字段）
inline BytePtr EntityBytes(SlotIndex slot)
{
    return Ptr(EntityAddress(slot));
}

// ==================== 模块内全局量读写（宽度由 T 决定） ====================
template <typename T> inline T ReadRva(Rva rva)
{
    return Load<T>(g_moduleBase + rva);
}

template <typename T> inline void WriteRva(Rva rva, T value)
{
    Store<T>(g_moduleBase + rva, value);
}

// ==================== 全局状态 ====================
HWND g_window = nullptr;
HMODULE g_module = nullptr;           // DLL 自身句柄（用于定位同目录 .log / .ini）
bool g_initialized = false;
bool g_showDebugPanel = true;
bool g_dragEntity = false;

SlotIndex g_hoveredSlot = -1;
SlotIndex g_editTargetSlot = -1;
SlotIndex g_draggingSlot = -1;
int  g_fps = 60;
int  g_currentMapID = 0;

float g_dragStartMouseX = 0.0f;
float g_dragStartMouseY = 0.0f;
float g_dragStartEntityX = 0.0f;
float g_dragStartEntityY = 0.0f;
bool  g_prevLeftDown = false;
bool  g_prevRightDown = false;

float g_pickRadiusWorld = 8.0f;

struct DebugInfo {
    float screenScaleRaw = 0.0f;
    float cameraX = 0.0f;
    float cameraY = 0.0f;
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    bool  mouseInside = false;
    float effectiveScale = 1.0f;
    float mouseWorldX = 0.0f;
    float mouseWorldY = 0.0f;
    float hitEntityX = 0.0f;
    float hitEntityY = 0.0f;
    float hitDistance = 0.0f;
};
DebugInfo g_debug;

// ==================== Log ====================
std::ofstream g_logFile;
bool g_logToFile = false;
int g_logFrameCounter = 0;

// ==================== DLL 同目录文件路径（.log / .ini 唯一来源） ====================
bool BuildDllSidecarPath(HMODULE module, const char *extension, char *outPath, std::size_t outSize)
{
    if (!module || !extension || !outPath || outSize == 0) return false;

    DWORD len = GetModuleFileNameA(module, outPath, static_cast<DWORD>(outSize));
    if (len == 0 || static_cast<std::size_t>(len) >= outSize) return false;

    char *dot = strrchr(outPath, '.');
    char *sep = strrchr(outPath, '\\');
    if (dot && (!sep || dot > sep)) *dot = 0;

    const std::size_t used = strlen(outPath);
    if (used + strlen(extension) + 1 > outSize) return false;
    snprintf(outPath + used, outSize - used, "%s", extension);
    return true;
}

void LogInit(HMODULE module)
{
    char path[MAX_PATH] = {};
    if (!BuildDllSidecarPath(module, ".log", path, sizeof(path))) return;
    g_logFile.open(path, std::ios::out | std::ios::trunc);
    if (g_logFile.is_open())
        g_logFile << "=== DR2C overlay log ===" << std::endl;
}

void LogShutdown()
{
    if (g_logFile.is_open()) {
        g_logFile << "=== end ===" << std::endl;
        g_logFile.close();
    }
}

void LogLine(const char *fmt, ...)
{
    if (!g_logFile.is_open() || !g_logToFile) return;
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    g_logFile << buf << std::endl;
    g_logFile.flush();
}

// ==================== 语言设置（DLL 同目录 .ini） ====================
constexpr const char *kSettingsExtension = ".ini";
constexpr const char *kLanguageKey       = "language=";

void LoadLanguageSetting(HMODULE module)
{
    char path[MAX_PATH] = {};
    if (!BuildDllSidecarPath(module, kSettingsExtension, path, sizeof(path))) return;

    FILE *file = fopen(path, "rb");
    if (!file) return;

    const std::size_t keyLen = strlen(kLanguageKey);
    char line[128];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, kLanguageKey, keyLen) != 0) continue;
        char *value = line + keyLen;
        value[strcspn(value, "\r\n")] = 0;
        Dr2cSetLanguage(Dr2cParseLanguage(value, DR2C_LANGUAGE_ENGLISH));
        break;
    }
    fclose(file);
}

void SaveLanguageSetting(HMODULE module)
{
    char path[MAX_PATH] = {};
    if (!BuildDllSidecarPath(module, kSettingsExtension, path, sizeof(path))) return;

    FILE *file = fopen(path, "wb");
    if (!file) return;
    fprintf(file, "%s%s\n", kLanguageKey, Dr2cLanguageToString(Dr2cGetLanguage()));
    fclose(file);
}

double EffectiveScale()
{
    float s = ReadRva<float>(Offset::Global::ScreenScale);
    if (!(s > 0.5f && s < 32.0f)) s = 4.0f;
    return static_cast<double>(s);
}

// ==================== 鼠标 / 窗口 ====================
void GetClientMousePos(float &mx, float &my, bool &insideWindow)
{
    mx = 0.0f; my = 0.0f; insideWindow = false;
    if (!g_window) return;
    POINT pt;
    if (!GetCursorPos(&pt)) return;
    if (!ScreenToClient(g_window, &pt)) return;
    RECT rect;
    if (!GetClientRect(g_window, &rect)) return;
    mx = static_cast<float>(pt.x);
    my = static_cast<float>(pt.y);
    insideWindow = (pt.x >= rect.left && pt.x < rect.right
                    && pt.y >= rect.top && pt.y < rect.bottom);
}

bool GetClientSize(float &w, float &h)
{
    if (!g_window) return false;
    RECT rect;
    if (!GetClientRect(g_window, &rect)) return false;
    w = static_cast<float>(rect.right - rect.left);
    h = static_cast<float>(rect.bottom - rect.top);
    return (w > 0.0f && h > 0.0f);
}

bool IsLeftMouseDown()  { return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0; }
bool IsRightMouseDown() { return (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0; }
bool IsImGuiCapturingMouse() { return ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow); }

bool ComputeMouseWorldPos(float &outWorldX, float &outWorldY)
{
    float cw = 0.0f, ch = 0.0f;
    if (!GetClientSize(cw, ch)) return false;
    float mx = 0.0f, my = 0.0f;
    bool inside = false;
    GetClientMousePos(mx, my, inside);
    const double scale = EffectiveScale();
    outWorldX = g_debug.cameraX + static_cast<float>((mx - cw * 0.5) / scale);
    outWorldY = g_debug.cameraY + static_cast<float>((my - ch * 0.5) / scale);
    return true;
}

bool WorldToScreen(const float world[3], float &outScreenX, float &outScreenY)
{
    float cw = 0.0f, ch = 0.0f;
    if (!GetClientSize(cw, ch)) return false;
    const double scale = EffectiveScale();
    outScreenX = static_cast<float>((world[0] - g_debug.cameraX) * scale + cw * 0.5);
    outScreenY = static_cast<float>((world[1] - g_debug.cameraY) * scale + ch * 0.5);
    return true;
}

void RefreshDebug()
{
    g_debug.screenScaleRaw = ReadRva<float>(Offset::Global::ScreenScale);
    g_debug.cameraX = ReadRva<float>(Offset::Global::CameraX);
    g_debug.cameraY = ReadRva<float>(Offset::Global::CameraY);
    GetClientMousePos(g_debug.mouseX, g_debug.mouseY, g_debug.mouseInside);
    g_debug.effectiveScale = static_cast<float>(EffectiveScale());
    g_currentMapID = ReadRva<std::uint8_t>(Offset::Global::MapLayer);
}

// ==================== 实体读写 ====================
Dr2cEntityView g_entities[kThingMaxSlots] = {};
Dr2cEntityView g_pendingEntity = {};
SlotIndex g_pendingSlot = 0;
volatile LONG g_pendingEntityWrite = 0;
bool g_keepEntityWrite = false;
unsigned int g_pendingWriteMask = 0;

void ReadEntity(SlotIndex slot, Dr2cEntityView &entity)
{
    const Address base = EntityAddress(slot);
    entity.id        = Load<std::uint16_t>(base + Offset::Thing::Id);
    entity.type      = Load<std::uint8_t>(base + Offset::Thing::Type);
    entity.subtype   = Load<std::uint8_t>(base + Offset::Thing::Subtype);
    entity.mapId     = Load<std::uint8_t>(base + Offset::Thing::MapId);
    entity.noCollide = Load<std::uint8_t>(base + Offset::Thing::NoCollide);
    entity.noPick    = Load<std::uint8_t>(base + Offset::Thing::NoPick);
    entity.unseen    = Load<std::uint8_t>(base + Offset::Thing::Unseen);
    entity.invisible = Load<std::uint8_t>(base + Offset::Thing::Invisible);
    LoadArray(base + Offset::Thing::Position, entity.position);
    LoadArray(base + Offset::Thing::Velocity, entity.velocity);
    LoadArray(base + Offset::Thing::Physics,  entity.physics);
    entity.glow      = Load<std::uint8_t>(base + Offset::Thing::Glow);
    entity.spriteId  = Load<std::uint16_t>(base + Offset::Thing::SpriteId);
    entity.hitpoints = Load<std::int32_t>(base + Offset::Thing::Hitpoints);
    entity.noHit     = Load<std::uint8_t>(base + Offset::Thing::NoHit);
    entity.aiState   = Load<std::uint32_t>(base + Offset::Thing::AiState);
}

void WriteEntity(SlotIndex slot, const Dr2cEntityView &entity, unsigned int mask)
{
    const Address base = EntityAddress(slot);
    if (mask & DR2C_ENTITY_WRITE_MAP)
        Store<std::uint8_t>(base + Offset::Thing::MapId, entity.mapId);
    if (mask & DR2C_ENTITY_WRITE_FLAGS) {
        Store<std::uint8_t>(base + Offset::Thing::NoCollide, entity.noCollide);
        Store<std::uint8_t>(base + Offset::Thing::NoPick,    entity.noPick);
        Store<std::uint8_t>(base + Offset::Thing::Unseen,    entity.unseen);
        Store<std::uint8_t>(base + Offset::Thing::Invisible, entity.invisible);
        Store<std::uint8_t>(base + Offset::Thing::Glow,      entity.glow);
        Store<std::uint8_t>(base + Offset::Thing::NoHit,     entity.noHit);
    }
    if (mask & DR2C_ENTITY_WRITE_POSITION)
        StoreArray(base + Offset::Thing::Position, entity.position);
    if (mask & DR2C_ENTITY_WRITE_VELOCITY)
        StoreArray(base + Offset::Thing::Velocity, entity.velocity);
    if (mask & DR2C_ENTITY_WRITE_PHYSICS)
        StoreArray(base + Offset::Thing::Physics, entity.physics);
    if (mask & DR2C_ENTITY_WRITE_SPRITE)
        Store<std::uint16_t>(base + Offset::Thing::SpriteId, entity.spriteId);
    if (mask & DR2C_ENTITY_WRITE_HITPOINTS)
        Store<std::int32_t>(base + Offset::Thing::Hitpoints, entity.hitpoints);
    if (mask & DR2C_ENTITY_WRITE_AI)
        Store<std::uint32_t>(base + Offset::Thing::AiState, entity.aiState);
}

void UpdateEntityStats()
{
    for (SlotIndex i = 0; i < kThingMaxSlots; ++i)
        ReadEntity(i, g_entities[i]);
}

// ==================== 复制 / 销毁 ====================
void CloneEntity(SlotIndex slot)
{
    if (slot < 0 || slot >= kThingMaxSlots) return;
    if (g_entities[slot].id == 0 || !pAllocateEntity) return;

    const Address src = EntityAddress(slot);
    const Address dst = pAllocateEntity(g_entities[slot].type);
    if (!dst) return;

    // 从 +0x02 复制到末尾，保留新分配的 id
    std::memcpy(Ptr(dst + 0x02), Ptr(src + 0x02), Offset::Thing::Stride - 0x02);
}

void DestroyEntity(SlotIndex slot)
{
    if (slot < 0 || slot >= kThingMaxSlots) return;
    if (g_entities[slot].id == 0 || !pFreeThing) return;
    pFreeThing(EntityAddress(slot));
}

// ==================== Character 读写 ====================
struct CharacterView {
    bool     active = false;
    Address  charPtr = 0;                                   // 角色数据地址
    std::uint32_t charid = 0;

    char name[Offset::Char::NameLength]               = {};
    char perk[Offset::Char::PerkLength]               = {};
    char trait[Offset::Char::TraitLength]             = {};
    char description[Offset::Char::DescriptionLength] = {};

    std::int32_t female = 0;
    std::int32_t pet    = 0;
    std::int32_t health = 0;
    float        speed_bonus = 0.0f;

    std::int32_t baseStat[Offset::Char::StatCount]  = {};
    std::int32_t bonusStat[Offset::Char::StatCount] = {};

    std::int32_t resource[Offset::Char::ResourceCount] = {};

    std::int32_t weaponStack[Offset::Char::WeaponSlotCount] = {};
    std::int32_t weaponID[Offset::Char::WeaponSlotCount]    = {};
    std::int32_t weaponLock[Offset::Char::WeaponSlotCount]  = {};
};

CharacterView g_charBuf;

void WriteStringAt(BytePtr p, const char *src, std::size_t maxLen)
{
    std::memset(p, 0, maxLen);
    std::size_t len = 0;
    while (len < maxLen && src[len] != 0) ++len;
    if (len > 0) std::memcpy(p, src, len);
}

void ReadCharacterFromPtr(Address charAddress, CharacterView &v)
{
    if (!charAddress) { v.active = false; return; }

    LoadArray(charAddress + Offset::Char::Name,        v.name);
    LoadArray(charAddress + Offset::Char::Perk,        v.perk);
    LoadArray(charAddress + Offset::Char::Trait,       v.trait);
    LoadArray(charAddress + Offset::Char::Description, v.description);
    v.name[sizeof(v.name) - 1]               = 0;
    v.perk[sizeof(v.perk) - 1]               = 0;
    v.trait[sizeof(v.trait) - 1]             = 0;
    v.description[sizeof(v.description) - 1] = 0;

    v.female = Load<std::int16_t>(charAddress + Offset::Char::Female);
    v.pet    = Load<std::int16_t>(charAddress + Offset::Char::Pet);
    v.health = Load<std::int32_t>(charAddress + Offset::Char::Health);
    v.speed_bonus = Load<float>(charAddress + Offset::Char::SpeedBonus);

    for (int i = 0; i < Offset::Char::StatCount; ++i) {
        v.baseStat[i]  = Load<std::int8_t>(charAddress + Offset::Char::BaseStat + i);
        v.bonusStat[i] = Load<std::int8_t>(charAddress + Offset::Char::BonusStat + i);
    }

    for (int i = 0; i < Offset::Char::ResourceCount; ++i)
        v.resource[i] = Load<std::int32_t>(charAddress + Offset::Char::Resource
                                           + static_cast<Address>(i) * sizeof(std::int32_t));

    for (int i = 0; i < Offset::Char::WeaponSlotCount; ++i) {
        const Address slotBase = charAddress + Offset::Char::WeaponSlot
                                 + static_cast<Address>(i) * Offset::WeaponSlot::Size;
        v.weaponStack[i] = Load<std::int32_t>(slotBase + Offset::WeaponSlot::Stack);
        v.weaponID[i]    = Load<std::int32_t>(slotBase + Offset::WeaponSlot::Id);
        v.weaponLock[i]  = Load<std::int32_t>(slotBase + Offset::WeaponSlot::Lock);
    }

    v.active = true;
}

void WriteCharacter()
{
    if (!g_charBuf.active || !g_charBuf.charPtr) return;
    const Address p = g_charBuf.charPtr;

    WriteStringAt(Ptr(p + Offset::Char::Name),        g_charBuf.name,        Offset::Char::NameLength);
    WriteStringAt(Ptr(p + Offset::Char::Perk),        g_charBuf.perk,        Offset::Char::PerkLength);
    WriteStringAt(Ptr(p + Offset::Char::Trait),       g_charBuf.trait,       Offset::Char::TraitLength);
    WriteStringAt(Ptr(p + Offset::Char::Description), g_charBuf.description, Offset::Char::DescriptionLength);

    Store<std::int16_t>(p + Offset::Char::Female, static_cast<std::int16_t>(g_charBuf.female));
    Store<std::int16_t>(p + Offset::Char::Pet,    static_cast<std::int16_t>(g_charBuf.pet));
    Store<std::int32_t>(p + Offset::Char::Health, g_charBuf.health);
    Store<float>(p + Offset::Char::SpeedBonus, g_charBuf.speed_bonus);

    for (int i = 0; i < Offset::Char::StatCount; ++i) {
        Store<std::int8_t>(p + Offset::Char::BaseStat  + i, static_cast<std::int8_t>(g_charBuf.baseStat[i]));
        Store<std::int8_t>(p + Offset::Char::BonusStat + i, static_cast<std::int8_t>(g_charBuf.bonusStat[i]));
    }

    for (int i = 0; i < Offset::Char::ResourceCount; ++i)
        Store<std::int32_t>(p + Offset::Char::Resource
                            + static_cast<Address>(i) * sizeof(std::int32_t),
                            g_charBuf.resource[i]);

    for (int i = 0; i < Offset::Char::WeaponSlotCount; ++i) {
        const Address slotBase = p + Offset::Char::WeaponSlot
                                 + static_cast<Address>(i) * Offset::WeaponSlot::Size;
        Store<std::int32_t>(slotBase + Offset::WeaponSlot::Stack, g_charBuf.weaponStack[i]);
        Store<std::int32_t>(slotBase + Offset::WeaponSlot::Id,    g_charBuf.weaponID[i]);
        Store<std::int32_t>(slotBase + Offset::WeaponSlot::Lock,  g_charBuf.weaponLock[i]);
    }
}

// ==================== 武器名缓存 ====================
std::vector<std::string> g_weaponNames;
bool g_weaponNamesLoaded = false;

void LoadWeaponNames()
{
    g_weaponNames.clear();
    g_weaponNames.reserve(static_cast<std::size_t>(kWeaponMaxCount));

    for (SlotIndex i = 0; i < kWeaponMaxCount; ++i) {
        const char *namePtr = AsConstPtr<char>(
            g_moduleBase + Offset::Global::WeaponPool
            + static_cast<Address>(i) * Offset::Weapon::Stride);
        std::size_t len = strnlen(namePtr, Offset::Weapon::NameLength);
        if (len == 0) break;
        std::string name(namePtr, len);
        if (name == "UNDEFINED") break;
        g_weaponNames.push_back(std::move(name));
    }
    g_weaponNamesLoaded = true;
}

// ==================== 悬停 ====================
void UpdateHoveredEntity()
{
    if (g_draggingSlot >= 0) { g_hoveredSlot = g_draggingSlot; return; }
    if (g_editTargetSlot >= 0) { g_hoveredSlot = g_editTargetSlot; return; }

    g_hoveredSlot = -1;
    g_debug.hitEntityX = 0.0f;
    g_debug.hitEntityY = 0.0f;
    g_debug.hitDistance = 0.0f;

    if (!g_window) return;
    if (IsImGuiCapturingMouse()) return;

    float mx = 0.0f, my = 0.0f;
    bool inside = false;
    GetClientMousePos(mx, my, inside);
    if (!inside) return;

    float worldX = 0.0f, worldY = 0.0f;
    if (!ComputeMouseWorldPos(worldX, worldY)) return;
    g_debug.mouseWorldX = worldX;
    g_debug.mouseWorldY = worldY;

    const float pickRadiusSq = g_pickRadiusWorld * g_pickRadiusWorld;
    float bestDistSq = pickRadiusSq;
    SlotIndex bestSlot = -1;

    for (SlotIndex slot = 1; slot < kThingMaxSlots; ++slot) {
        const Address thing = EntityAddress(slot);
        const std::uint16_t id    = Load<std::uint16_t>(thing + Offset::Thing::Id);
        const std::uint8_t  mapid = Load<std::uint8_t>(thing + Offset::Thing::MapId);
        g_currentMapID = ReadRva<std::uint8_t>(Offset::Global::MapLayer);
        if (id == 0 || mapid != g_currentMapID) continue;
        const float px = Load<float>(thing + Offset::Thing::Position);
        const float py = Load<float>(thing + Offset::Thing::Position + sizeof(float));
        const float dx = px - worldX;
        const float dy = py - worldY;
        const float distSq = dx * dx + dy * dy;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestSlot = slot;
            g_debug.hitEntityX = px;
            g_debug.hitEntityY = py;
            g_debug.hitDistance = std::sqrt(distSq);
        }
    }
    g_hoveredSlot = bestSlot;
}

// ==================== 拖动 ====================
void UpdateDragging()
{
    if (!g_initialized || !g_window) return;

    const bool leftDown = IsLeftMouseDown();
    const bool leftClicked = leftDown && !g_prevLeftDown;
    g_prevLeftDown = leftDown;

    float mx = 0.0f, my = 0.0f;
    bool inside = false;
    GetClientMousePos(mx, my, inside);
    const bool mouseOnGame = inside && !IsImGuiCapturingMouse();

    if (g_draggingSlot < 0 && g_dragEntity && leftClicked && mouseOnGame
        && g_editTargetSlot < 0
        && g_hoveredSlot >= 0 && g_entities[g_hoveredSlot].id != 0) {
        g_dragStartMouseX = mx;
        g_dragStartMouseY = my;
        g_dragStartEntityX = g_entities[g_hoveredSlot].position[0];
        g_dragStartEntityY = g_entities[g_hoveredSlot].position[1];
        g_draggingSlot = g_hoveredSlot;
    }

    if (g_draggingSlot >= 0) {
        if (leftDown) {
            const double scale = EffectiveScale();
            const float deltaScreenX = mx - g_dragStartMouseX;
            const float deltaScreenY = my - g_dragStartMouseY;
            Dr2cEntityView &entity = g_entities[g_draggingSlot];
            entity.position[0] = g_dragStartEntityX + static_cast<float>(deltaScreenX / scale);
            entity.position[1] = g_dragStartEntityY + static_cast<float>(deltaScreenY / scale);
            QueueEntityWrite(g_draggingSlot, entity, DR2C_ENTITY_WRITE_POSITION);
        } else {
            ClearPendingEntityWrite();
            g_draggingSlot = -1;
        }
    }
}

// ==================== 右键触发编辑 ====================
// 弹窗标题：显示文本随语言变化，### 之后的 ID 固定，保证 OpenPopup / BeginPopup 永远匹配
const char *EntityEditPopupId()
{
    static char buffer[96];
    snprintf(buffer, sizeof(buffer), "%s###dr2c_entity_edit", Tr("Entity Edit"));
    return buffer;
}

void CheckRightClickToEdit()
{
    if (!g_window) return;
    if (g_draggingSlot >= 0) return;
    if (g_editTargetSlot >= 0) return;
    if (IsImGuiCapturingMouse()) return;

    const bool rightDown = IsRightMouseDown();
    const bool rightClicked = rightDown && !g_prevRightDown;
    g_prevRightDown = rightDown;

    if (!rightClicked) return;
    if (g_hoveredSlot < 0) return;
    if (g_entities[g_hoveredSlot].id == 0) return;

    g_editTargetSlot = g_hoveredSlot;
    g_charBuf = CharacterView{};

    // type == 1
    if (g_entities[g_hoveredSlot].type == 1) {
        const Address thing = EntityAddress(g_hoveredSlot);
        const std::uint32_t charid     = Load<std::uint32_t>(thing + Offset::Thing::CharId);
        g_charBuf.charid = charid;

        if (charid > 0 && charid < 256 && pGetCharacterData) {
            const Address charAddress = pGetCharacterData(charid);
            if (charAddress) {
                g_charBuf.charPtr = charAddress;
                ReadCharacterFromPtr(charAddress, g_charBuf);
                g_charBuf.charid  = charid;
                g_charBuf.charPtr = charAddress;
            }
        }
    }

    ImGui::OpenPopup(EntityEditPopupId());
}

// ==================== 高亮 ====================
void DrawEntityHighlight()
{
    float cw = 0.0f, ch = 0.0f;
    if (!GetClientSize(cw, ch)) return;

    const SlotIndex slot = (g_draggingSlot >= 0) ? g_draggingSlot
                          : (g_editTargetSlot >= 0) ? g_editTargetSlot
                                                    : g_hoveredSlot;
    if (slot < 0 || slot >= kThingMaxSlots) return;
    const Dr2cEntityView &entity = g_entities[slot];
    if (entity.id == 0) return;

    float screenX = 0.0f, screenY = 0.0f;
    if (!WorldToScreen(entity.position, screenX, screenY)) return;

    ImU32 color = IM_COL32(255, 220, 80, 255);
    if (g_draggingSlot == slot)        color = IM_COL32(255, 80, 80, 255);
    else if (g_editTargetSlot == slot) color = IM_COL32(80, 200, 255, 255);

    ImDrawList *fg = ImGui::GetForegroundDrawList();
    const double scale = EffectiveScale();
    const float halfSize = static_cast<float>(g_pickRadiusWorld * scale);
    const float boxSize = halfSize < 18.0f ? 18.0f : halfSize;

    fg->AddRect(ImVec2(screenX - boxSize, screenY - boxSize),
                ImVec2(screenX + boxSize, screenY + boxSize),
                color, 0.0f, 0, 2.5f);
    fg->AddCircle(ImVec2(screenX, screenY), boxSize + 2.0f, color, 0, 1.5f);
}

// ==================== Debug 面板 ====================
void DrawDebugPanel()
{
    if (!g_showDebugPanel){
        return;
    }

    // 标题随语言变化，### 之后的 ID 固定（切换语言不会丢窗口位置/大小）
    char title[128];
    snprintf(title, sizeof(title), "%s###dr2c_debug", Tr("DR2C Debug"));

    ImGui::SetNextWindowSize(ImVec2(380.0f, 300.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin(title, &g_showDebugPanel);

    ImGui::Checkbox(Tr("Drag entity"), &g_dragEntity);
    ImGui::SameLine();
    ImGui::Checkbox(Tr("Log to file"), &g_logToFile);

    ImGui::SliderFloat(Tr("Pick radius (world)"), &g_pickRadiusWorld, 2.0f, 64.0f, "%.1f");

    if (ImGui::SliderInt(Tr("Game FrameRate"), &g_fps, 1, 1024)){
        pSetFrameRate(g_fps);
    };

    if (ImGui::InputInt(Tr("Current Map"), &g_currentMapID)){
        WriteRva<std::uint32_t>(Offset::Global::MapLayer,
                                static_cast<std::uint8_t>(g_currentMapID));
    }

    // ---------------- 语言切换（中英文） ----------------
    int languageIndex = static_cast<int>(Dr2cGetLanguage());
    const bool cjkFontReady = Dr2cIsCjkFontAvailable();
    if (!cjkFontReady)
        ImGui::BeginDisabled();
    if (ImGui::Combo(Tr("Language"), &languageIndex, u8"English\0中文\0")) {
        Dr2cSetLanguage(static_cast<Dr2cLanguage>(languageIndex));
        SaveLanguageSetting(g_module);
    }
    if (!cjkFontReady) {
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextUnformatted(Tr("(CJK font not found)"));
    }

    ImGui::Separator();
    ImGui::Text("moduleBase    = %p", AsPtr<void>(g_moduleBase));
    ImGui::Text("g_ScreenScale = %.4f", g_debug.screenScaleRaw);
    ImGui::Text("camera        = (%.2f, %.2f)", g_debug.cameraX, g_debug.cameraY);
    ImGui::Text("mouse         = (%.1f, %.1f) %s",
                g_debug.mouseX, g_debug.mouseY, g_debug.mouseInside ? "in" : "OUT");
    ImGui::Text("hover=%d  edit=%d  drag=%d",
                g_hoveredSlot, g_editTargetSlot, g_draggingSlot);

    ImGui::Separator();
    ImGui::TextUnformatted(Tr("Right-click entity to edit"));

    ImGui::End();
}

// ==================== 属性 / 资源名 ====================
const char *kStatNames[Offset::Char::StatCount] = {
    "Morale", "Attitude", "Composure", "Charm", "Wits", "Loyalty",
    "Medical", "Mechanical", "Shooting", "Strength", "Dexterity",
    "Fitness", "Vitality"
};

const char *kResourceNames[Offset::Char::ResourceCount] = {
    "None", "Food", "Gas", "Medical", "Bullet", "Rifle", "Shell", "Junk"
};

// ==================== 布局小工具 ====================
const char *GroupWithFields(const char *group, const char *field0,
                            const char *field1, const char *field2)
{
    static char buffer[192];
    snprintf(buffer, sizeof(buffer), "%s (%s / %s / %s)", group, field0, field1, field2);
    return buffer;
}

const char *FieldList3(const char *field0, const char *field1, const char *field2)
{
    static char buffer[192];
    snprintf(buffer, sizeof(buffer), "%s / %s / %s", field0, field1, field2);
    return buffer;
}

float TwoColumnItemWidth(const char *label0, const char *label1)
{
    const ImGuiStyle &style = ImGui::GetStyle();
    const float avail = ImGui::GetContentRegionAvail().x;
    const float labels = ImGui::CalcTextSize(label0).x + ImGui::CalcTextSize(label1).x;
    const float each = (avail - labels - style.ItemSpacing.x * 2.0f) * 0.5f;
    return each > 60.0f ? each : 60.0f;
}

// ==================== Character 编辑块 ====================
void DrawCharacterSection(SlotIndex slot)
{
    if (!g_charBuf.active || !g_charBuf.charPtr) {
        ImGui::TextUnformatted(Tr("(no character attached)"));
        if (g_charBuf.charid != 0)
            ImGui::Text("charid = %u", g_charBuf.charid);
        return;
    }

    ImGui::Text("charPtr = %p   charid = %u", AsPtr<void>(g_charBuf.charPtr), g_charBuf.charid);
    ImGui::Separator();

    ImGui::PushItemWidth(240.0f);
    const bool nameChanged = ImGui::InputText(Tr("Name"), g_charBuf.name, sizeof(g_charBuf.name));
    ImGui::PopItemWidth();
    if (nameChanged)
        WriteCharacter();

    bool valuesChanged = false;
    ImGui::SetNextItemWidth(TwoColumnItemWidth(Tr("Health"), Tr("Speed Bonus")));
    valuesChanged |= ImGui::InputInt(Tr("Health"), &g_charBuf.health);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(TwoColumnItemWidth(Tr("Health"), Tr("Speed Bonus")));
    valuesChanged |= ImGui::InputFloat(Tr("Speed Bonus"), &g_charBuf.speed_bonus, 0.0f, 0.0f, "%.2f");
    if (valuesChanged)
        WriteCharacter();

    if (ImGui::Button(Tr("Control Human"))){
        if (!g_charBuf.active || !g_charBuf.charPtr) return;
        pSetCurrentPlayerThing(EntityAddress(slot));
    }

    if (ImGui::TreeNode(Tr("Stats"))) {
        ImGui::TextUnformatted(Tr("         Base  Bonus  Total"));
        for (int i = 0; i < Offset::Char::StatCount; ++i) {
            ImGui::PushID(i);
            ImGui::Text("%-11s", Tr(kStatNames[i]));
            ImGui::SameLine();
            ImGui::PushItemWidth(60.0f);
            bool changed = false;
            changed |= ImGui::InputInt("##base",  &g_charBuf.baseStat[i],  0, 0);
            ImGui::SameLine();
            changed |= ImGui::InputInt("##bonus", &g_charBuf.bonusStat[i], 0, 0);
            ImGui::PopItemWidth();
            ImGui::SameLine();
            ImGui::Text("%d", g_charBuf.baseStat[i] + g_charBuf.bonusStat[i]);
            if (changed) WriteCharacter();
            ImGui::PopID();
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode(Tr("Resources"))) {
        for (int i = 0; i < Offset::Char::ResourceCount; ++i) {
            ImGui::PushID(100 + i);
            if (ImGui::InputInt(Tr(kResourceNames[i]), &g_charBuf.resource[i]))
                WriteCharacter();
            ImGui::PopID();
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode(Tr("Weapons"))) {
        if (!g_weaponNamesLoaded) LoadWeaponNames();

        for (int slotIdx = 0; slotIdx < Offset::Char::WeaponSlotCount; ++slotIdx) {
            ImGui::PushID(200 + slotIdx);
            ImGui::Text(Tr("Slot %d"), slotIdx);
            ImGui::SameLine();

            std::int32_t curId = g_charBuf.weaponID[slotIdx];
            const char *curName = Tr("(empty)");
            if (curId > 0 && curId < static_cast<std::int32_t>(g_weaponNames.size()))
                curName = g_weaponNames[curId].c_str();

            ImGui::PushItemWidth(180.0f);
            bool changed = false;
            if (ImGui::BeginCombo("##weapon", curName)) {
                if (ImGui::Selectable(Tr("(empty)"), curId == 0)) {
                    g_charBuf.weaponID[slotIdx] = 0;
                    changed = true;
                }
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(g_weaponNames.size()));
                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                        bool selected = (curId == i);
                        ImGui::PushID(i);
                        if (ImGui::Selectable(g_weaponNames[i].c_str(), selected)) {
                            g_charBuf.weaponID[slotIdx] = i;
                            changed = true;
                        }
                        ImGui::PopID();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            ImGui::SameLine();
            ImGui::PushItemWidth(60.0f);
            changed |= ImGui::InputInt(Tr("Stack"), &g_charBuf.weaponStack[slotIdx], 0, 0);
            ImGui::SameLine();
            bool lockFlag = g_charBuf.weaponLock[slotIdx] != 0;
            changed |= ImGui::Checkbox(Tr("Lock"), &lockFlag);
            g_charBuf.weaponLock[slotIdx] = lockFlag ? 1 : 0;
            //changed |= ImGui::InputInt("Lock",  &g_charBuf.weaponLock[slotIdx],  0, 0);
            ImGui::PopItemWidth();

            if (changed) WriteCharacter();
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
}

// ==================== Vehicle 编辑块 ====================
void DrawVehicleSection(int slot)
{
    const Address p = EntityAddress(slot);

    std::int32_t chassis     = Load<std::int8_t>(p + Offset::Vehicle::Chassis);
    std::int32_t chassisMax  = Load<std::int8_t>(p + Offset::Vehicle::ChassisMax);
    std::int32_t engine      = Load<std::int8_t>(p + Offset::Vehicle::Engine);
    std::int32_t engineMax   = Load<std::int8_t>(p + Offset::Vehicle::EngineMax);
    std::int32_t armour      = Load<std::int8_t>(p + Offset::Vehicle::Armour);
    std::int32_t armourMax   = Load<std::int8_t>(p + Offset::Vehicle::ArmourMax);
    std::int32_t carspeed    = Load<std::int8_t>(p + Offset::Vehicle::Speed);
    std::int32_t carspeedMax = Load<std::int8_t>(p + Offset::Vehicle::SpeedMax);
    std::int32_t repair      = Load<std::int32_t>(p + Offset::Vehicle::Repair);
    float        mpg         = Load<float>(p + Offset::Vehicle::Mpg);

    ImGui::PushItemWidth(120.0f);
    bool changed = false;
    changed |= ImGui::InputInt(Tr("Chassis"), &chassis);
    changed |= ImGui::InputInt(Tr("Chassis Max"), &chassisMax);
    changed |= ImGui::InputInt(Tr("Engine"), &engine);
    changed |= ImGui::InputInt(Tr("Engine Max"), &engineMax);
    changed |= ImGui::InputInt(Tr("Armour"), &armour);
    changed |= ImGui::InputInt(Tr("Armour Max"), &armourMax);
    changed |= ImGui::InputInt(Tr("Speed"), &carspeed);
    changed |= ImGui::InputInt(Tr("Speed Max"), &carspeedMax);
    changed |= ImGui::InputInt(Tr("Repair"), &repair);
    changed |= ImGui::InputFloat(Tr("MPG"), &mpg, 0.0f, 0.0f, "%.2f");
    ImGui::PopItemWidth();

    if (changed) {
        Store<std::int8_t>(p + Offset::Vehicle::Chassis,    static_cast<std::int8_t>(chassis));
        Store<std::int8_t>(p + Offset::Vehicle::ChassisMax, static_cast<std::int8_t>(chassisMax));
        Store<std::int8_t>(p + Offset::Vehicle::Engine,     static_cast<std::int8_t>(engine));
        Store<std::int8_t>(p + Offset::Vehicle::EngineMax,  static_cast<std::int8_t>(engineMax));
        Store<std::int8_t>(p + Offset::Vehicle::Armour,     static_cast<std::int8_t>(armour));
        Store<std::int8_t>(p + Offset::Vehicle::ArmourMax,  static_cast<std::int8_t>(armourMax));
        Store<std::int8_t>(p + Offset::Vehicle::Speed,      static_cast<std::int8_t>(carspeed));
        Store<std::int8_t>(p + Offset::Vehicle::SpeedMax,   static_cast<std::int8_t>(carspeedMax));
        Store<std::int32_t>(p + Offset::Vehicle::Repair, repair);
        Store<float>(p + Offset::Vehicle::Mpg, mpg);
    }
}

// ==================== Item 编辑块 ====================
void DrawItemSection(SlotIndex slot)
{
    const Address p = EntityAddress(slot);

    std::int32_t amount = Load<std::int32_t>(p + Offset::Item::Amount);
    std::int32_t loot   = Load<std::uint8_t>(p + Offset::Item::Loot);

    ImGui::PushItemWidth(120.0f);
    bool changed = false;
    changed |= ImGui::InputInt(Tr("Amount"), &amount);
    //changed |= ImGui::InputInt("Loot", &loot);
    const char *curLootName = (loot >= 0 && loot < Offset::Char::ResourceCount)
                                  ? Tr(kResourceNames[loot])
                                  : Tr("(invalid)");
    if (ImGui::BeginCombo(Tr("Loot"), curLootName)) {
        for (int i = 0; i < Offset::Char::ResourceCount; ++i) {
            const bool selected = (loot == i);
            ImGui::PushID(i);
            if (ImGui::Selectable(Tr(kResourceNames[i]), selected)) {
                loot = i;
                changed = true;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    if (changed) {
        Store<std::int32_t>(p + Offset::Item::Amount, amount);
        Store<std::uint8_t>(p + Offset::Item::Loot, static_cast<std::uint8_t>(loot));
    }
}

// ==================== 编辑 Popup ====================
void DrawEntityEditPopup()
{
    if (g_editTargetSlot < 0) return;

    ImGui::SetNextWindowSize(ImVec2(380.0f, 500.0f), ImGuiCond_Appearing);

    if (ImGui::BeginPopup(EntityEditPopupId())) {
        const SlotIndex slot = g_editTargetSlot;
        if (slot < 0 || slot >= kThingMaxSlots
            || g_entities[slot].id == 0) {
            ImGui::TextUnformatted(Tr("(entity no longer exists)"));
            if (ImGui::Button(Tr("Close"))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        Dr2cEntityView &entity = g_entities[slot];

        ImGui::Text(Tr("Slot %d   ID %u   Type %u / %u"),
                    slot, entity.id, entity.type, entity.subtype);
        ImGui::Separator();

        if (ImGui::Button(Tr("Clone"), ImVec2(80, 0))) CloneEntity(slot);
        ImGui::SameLine();
        if (ImGui::Button(Tr("Destroy"), ImVec2(80, 0))) {
            DestroyEntity(slot);
            g_editTargetSlot = -1;
            ClearPendingEntityWrite();
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        ImGui::SameLine();
        if (ImGui::Button(Tr("Close"), ImVec2(80, 0))) ImGui::CloseCurrentPopup();

        ImGui::Separator();

        // ---------- 实体字段 ----------
        if (ImGui::CollapsingHeader(Tr("Entity"), ImGuiTreeNodeFlags_None)) {
            // 区域 ID
            int mapId = entity.mapId;
            ImGui::TextUnformatted(Tr("Map"));
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputInt("##map_id", &mapId)) {
                entity.mapId = static_cast<unsigned char>(mapId);
                QueueEntityWrite(slot, entity, DR2C_ENTITY_WRITE_MAP);
            }
            if (ImGui::InputFloat3(Tr("Position"), entity.position))
                QueueEntityWrite(slot, entity, DR2C_ENTITY_WRITE_POSITION);
            if (ImGui::InputFloat3(Tr("Velocity"), entity.velocity))
                QueueEntityWrite(slot, entity, DR2C_ENTITY_WRITE_VELOCITY);

            ImGui::TextUnformatted(GroupWithFields(Tr("Physics"), Tr("Mass"),
                                                  Tr("Friction"), Tr("Bounce")));
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputFloat3("##physics", entity.physics))
                QueueEntityWrite(slot, entity, DR2C_ENTITY_WRITE_PHYSICS);

            int entityNumbers[3] = {
                entity.hitpoints,
                static_cast<int>(entity.spriteId),
                static_cast<int>(entity.aiState)
            };
            ImGui::TextUnformatted(FieldList3(Tr("Hitpoints"), Tr("Sprite"), Tr("AI state")));
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputInt3("##entity_numbers", entityNumbers)) {
                entity.hitpoints = entityNumbers[0];
                entity.spriteId  = static_cast<unsigned short>(entityNumbers[1]);
                entity.aiState   = static_cast<std::uint32_t>(entityNumbers[2]);
                QueueEntityWrite(slot, entity, DR2C_ENTITY_WRITE_HITPOINTS
                                              | DR2C_ENTITY_WRITE_SPRITE
                                              | DR2C_ENTITY_WRITE_AI);
            }

            bool noCollide = entity.noCollide != 0;
            bool noPick = entity.noPick != 0;
            bool unseen = entity.unseen != 0;
            bool invisible = entity.invisible != 0;
            bool noHit = entity.noHit != 0;
            bool glow = entity.glow != 0;

            bool flagsChanged = false;
            flagsChanged |= ImGui::Checkbox(Tr("No collision"), &noCollide);
            ImGui::SameLine();
            flagsChanged |= ImGui::Checkbox(Tr("No pickup"), &noPick);
            ImGui::SameLine();
            flagsChanged |= ImGui::Checkbox(Tr("Unseen"), &unseen);
            flagsChanged |= ImGui::Checkbox(Tr("Invisible"), &invisible);
            ImGui::SameLine();
            flagsChanged |= ImGui::Checkbox(Tr("No hit"), &noHit);
            ImGui::SameLine();
            flagsChanged |= ImGui::Checkbox(Tr("Glow"), &glow);

            entity.noCollide = static_cast<unsigned char>(noCollide);
            entity.noPick    = static_cast<unsigned char>(noPick);
            entity.unseen    = static_cast<unsigned char>(unseen);
            entity.invisible = static_cast<unsigned char>(invisible);
            entity.noHit     = static_cast<unsigned char>(noHit);
            entity.glow      = static_cast<unsigned char>(glow);

            if (flagsChanged)
                QueueEntityWrite(slot, entity, DR2C_ENTITY_WRITE_FLAGS);
        }

        // ---------- Character（type == 1)  ----------
        if (entity.type == 1) {
            if (ImGui::CollapsingHeader(Tr("Character"), ImGuiTreeNodeFlags_None))
                DrawCharacterSection(slot);
        }

        // ---------- Item（type == 3 且 subtype == 1） ----------
        if (entity.type == 3 && entity.subtype == 1) {
            if (ImGui::CollapsingHeader(Tr("Item"), ImGuiTreeNodeFlags_None))
                DrawItemSection(slot);
        }

        // ---------- Vehicle（type == 3 且 subtype == 3） ----------
        if (entity.type == 3 && entity.subtype == 3) {
            if (ImGui::CollapsingHeader(Tr("Vehicle"), ImGuiTreeNodeFlags_None))
                DrawVehicleSection(slot);
        }

        ImGui::EndPopup();
    } else {
        g_editTargetSlot = -1;
        g_charBuf.active = false;
        ClearPendingEntityWrite();
    }
}

} // namespace

// ==================== 导出接口 ====================

void CheckPanelHotkey()
{
    static bool prevInsert = false;
    const bool curInsert = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    if (curInsert && !prevInsert) {
        g_showDebugPanel = !g_showDebugPanel;
    }
    prevInsert = curInsert;
}

void QueueEntityWrite(SlotIndex slot, const Dr2cEntityView &entity, unsigned int mask)
{
    if (slot < 0 || slot >= kThingMaxSlots) return;
    g_pendingSlot = slot;
    g_pendingEntity = entity;
    g_pendingWriteMask |= mask;
    g_keepEntityWrite = true;
    WriteEntity(g_pendingSlot, g_pendingEntity, mask);
    InterlockedExchange(&g_pendingEntityWrite, 1);
}

void ApplyPendingEntityWrite()
{
    if (g_keepEntityWrite && InterlockedCompareExchange(&g_pendingEntityWrite, 1, 1) == 1) {
        WriteEntity(g_pendingSlot, g_pendingEntity, g_pendingWriteMask);
        ReadEntity(g_pendingSlot, g_entities[g_pendingSlot]);
    }
}

void ClearPendingEntityWrite()
{
    g_keepEntityWrite = false;
    g_pendingWriteMask = 0;
    InterlockedExchange(&g_pendingEntityWrite, 0);
}

// ==================== 界面字体（含中文） ====================
constexpr float kUiFontSize = 16.0f;

bool LoadUiFont()
{
    ImGuiIO &io = ImGui::GetIO();
    static const char *kFontCandidates[] = {
        "C:\\Windows\\Fonts\\simhei.ttf",  // 黑体
        "C:\\Windows\\Fonts\\msyh.ttc",    // 微软雅黑
        "C:\\Windows\\Fonts\\msyh.ttf",
        "C:\\Windows\\Fonts\\msyhl.ttc",   // 微软雅黑 Light
        "C:\\Windows\\Fonts\\Deng.ttf",    // 等线
        "C:\\Windows\\Fonts\\simsun.ttc",  // 宋体
    };

    for (const char *fontPath : kFontCandidates) {
        if (GetFileAttributesA(fontPath) == INVALID_FILE_ATTRIBUTES)
            continue;
        ImFontConfig config;
        config.FontNo = 0;                 // .ttc 字体集合取第一个字面
        if (io.Fonts->AddFontFromFileTTF(fontPath, kUiFontSize, &config) != nullptr)
            return true;
    }

    io.Fonts->AddFontDefault();
    return false;
}

bool InitializeInternalUi(HWND window, HMODULE module)
{
    if (g_initialized || !window) return g_initialized;

    CacheGameFunctions();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(1.0f);

    // 必须在后端初始化前加载字体
    Dr2cSetCjkFontAvailable(LoadUiFont());

    if (!ImGui_ImplWin32_Init(window) || !ImGui_ImplOpenGL3_Init("#version 130")) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }
    g_window = window;
    g_module = module;
    g_initialized = true;
    LogInit(module);
    // 字体就绪后再读语言设置（没有中文字体时不会切到中文）
    LoadLanguageSetting(module);
    g_fps = (int)pGetFrameRate();
    return true;
}

void ShutdownInternalUi()
{
    if (!g_initialized) return;
    LogShutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    g_window = nullptr;
    g_initialized = false;
}

void RenderInternalUi()
{
    if (!g_initialized) return;

    UpdateEntityStats();
    ApplyPendingEntityWrite();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    RefreshDebug();
    UpdateHoveredEntity();
    UpdateDragging();
    CheckRightClickToEdit();
    CheckPanelHotkey();

    static int s_lastHovered = -1;
    if (s_lastHovered != g_hoveredSlot
        && g_draggingSlot < 0 && g_editTargetSlot < 0) {
        ClearPendingEntityWrite();
        s_lastHovered = g_hoveredSlot;
    }

    DrawDebugPanel();
    DrawEntityEditPopup();
    DrawEntityHighlight();

    if (g_logToFile && ++g_logFrameCounter >= 30) {
        g_logFrameCounter = 0;
        LogLine("[auto] eff=%.4f cam=(%.2f,%.2f) mouse=(%.1f,%.1f) in=%d hover=%d edit=%d drag=%d",
                g_debug.effectiveScale, g_debug.cameraX, g_debug.cameraY,
                g_debug.mouseX, g_debug.mouseY, g_debug.mouseInside ? 1 : 0,
                g_hoveredSlot, g_editTargetSlot, g_draggingSlot);
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

LRESULT HandleInternalWindowMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (g_initialized && window == g_window
        && ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam)) {
        return 1;
    }
    return 0;
}