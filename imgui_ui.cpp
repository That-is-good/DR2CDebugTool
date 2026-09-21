#include "imgui_ui.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"
#include <windows.h>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <vector>
#include <string>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam);

namespace {

// ==================== 模块基址 + 函数指针（一次缓存） ====================
uintptr_t g_moduleBase = 0;

using fn_AllocateEntity_t   = uintptr_t(__cdecl*)(uint8_t);
using fn_FreeThing_t        = void(__cdecl*)(uintptr_t);
using fn_GetCharacterData_t = void*(__cdecl*)(uint32_t);
using fn_GetFrameRate       = uint32_t(__cdecl*)();
using fn_SetFrameRate       = uint32_t(__cdecl*)(uint32_t);
using fn_GetCurrentMapLayer = uint32_t(__cdecl*)();
using fn_SetCurrentMapLayer = uint32_t(__cdecl*)(uint32_t);

fn_AllocateEntity_t   pAllocateEntity   = nullptr;
fn_FreeThing_t        pFreeThing        = nullptr;
fn_GetCharacterData_t pGetCharacterData = nullptr;
fn_GetFrameRate       pGetFrameRate     = nullptr;
fn_SetFrameRate       pSetFrameRate     = nullptr;
// fn_GetCurrentMapLayer pGetCurrentMapLayer = nullptr;
// fn_SetCurrentMapLayer pSetCurrentMapLayer = nullptr;


void CacheGameFunctions()
{
    g_moduleBase      = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    pAllocateEntity   = reinterpret_cast<fn_AllocateEntity_t>  (g_moduleBase + 0x52710u);
    pFreeThing        = reinterpret_cast<fn_FreeThing_t>       (g_moduleBase + 0x52AA0u);
    pGetCharacterData = reinterpret_cast<fn_GetCharacterData_t>(g_moduleBase + 0x28AE0u);
    pGetFrameRate     = reinterpret_cast<fn_GetFrameRate>      (g_moduleBase + 0x04C50u);
    pSetFrameRate     = reinterpret_cast<fn_SetFrameRate>      (g_moduleBase + 0x04B30u);
    // pGetCurrentMapLayer     = reinterpret_cast<fn_GetCurrentMapLayer>      (g_moduleBase + 0x07D2E0u);
    // pSetCurrentMapLayer     = reinterpret_cast<fn_SetCurrentMapLayer>      (g_moduleBase + 0x07D310u);
}

// ==================== 直接读全局值 ====================
constexpr uintptr_t kThingPoolRva   = 0x5632E0u;
constexpr uintptr_t kCameraXRva     = 0x610340u;
constexpr uintptr_t kCameraYRva     = 0x610344u;
constexpr uintptr_t kScreenScaleRva = 0x0D14ACu;
constexpr uintptr_t kWeaponPoolRva  = 0x4E0080u;
constexpr uintptr_t g_CameraRenderMapLayer  = 0x5D6294u;
constexpr uintptr_t kThingStride    = 0x304u;
constexpr uintptr_t kWeaponStride   = 0x1C4u;
constexpr unsigned int kThingMaxSlots = 610u;
constexpr unsigned int kWeaponMaxCount = 0x401u;

inline unsigned char *EntityPtr(unsigned int slot)
{
    return reinterpret_cast<unsigned char *>(g_moduleBase + kThingPoolRva + slot * kThingStride);
}

inline float ReadFloat(uintptr_t rva)
{
    return *reinterpret_cast<const float *>(g_moduleBase + rva);
}

inline uint8_t ReadUInt8(uintptr_t rva)
{
    return *reinterpret_cast<const uint8_t *>(g_moduleBase + rva);
}

inline void WriteInt(uint32_t data, uintptr_t rva){
    std::memcpy((unsigned char*)(g_moduleBase + rva), &data, sizeof(uint32_t));
}

// ==================== 全局状态 ====================
HWND g_window = nullptr;
bool g_initialized = false;
bool g_showDebugPanel = true;
bool g_dragEntity = false;

int  g_hoveredSlot = -1;
int  g_editTargetSlot = -1;
int  g_draggingSlot = -1;
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

void LogInit(HMODULE module)
{
    char path[MAX_PATH] = {};
    DWORD len = GetModuleFileNameA(module, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return;
    char *dot = strrchr(path, '.');
    char *sep = strrchr(path, '\\');
    if (dot && (!sep || dot > sep)) *dot = 0;
    const size_t used = strlen(path);
    if (used + 4 < MAX_PATH) {
        snprintf(path + used, MAX_PATH - used, ".log");
    }
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

double EffectiveScale()
{
    float s = ReadFloat(kScreenScaleRva);
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
    g_debug.screenScaleRaw = ReadFloat(kScreenScaleRva);
    g_debug.cameraX = ReadFloat(kCameraXRva);
    g_debug.cameraY = ReadFloat(kCameraYRva);
    GetClientMousePos(g_debug.mouseX, g_debug.mouseY, g_debug.mouseInside);
    g_debug.effectiveScale = static_cast<float>(EffectiveScale());
    g_currentMapID = ReadUInt8(g_CameraRenderMapLayer);
}

// ==================== 实体读写 ====================
Dr2cEntityView g_entities[kThingMaxSlots] = {};
Dr2cEntityView g_pendingEntity = {};
unsigned int g_pendingSlot = 0;
volatile LONG g_pendingEntityWrite = 0;
bool g_keepEntityWrite = false;
unsigned int g_pendingWriteMask = 0;

void ReadEntity(unsigned int slot, Dr2cEntityView &entity)
{
    const unsigned char *p = EntityPtr(slot);
    std::memcpy(&entity.id, p + 0x00, sizeof(entity.id));
    entity.type    = p[0x02];
    entity.subtype = p[0x03];
    entity.mapId   = p[0x04];
    entity.noCollide = p[0x0D];
    entity.noPick    = p[0x11];
    entity.unseen    = p[0x12];
    entity.invisible = p[0x13];
    std::memcpy(entity.position, p + 0x2C, sizeof(entity.position));
    std::memcpy(entity.velocity, p + 0x38, sizeof(entity.velocity));
    std::memcpy(entity.physics,  p + 0x58, sizeof(entity.physics));
    entity.glow = p[0x70];
    std::memcpy(&entity.spriteId, p + 0xD8, sizeof(entity.spriteId));
    std::memcpy(&entity.hitpoints, p + 0x254, sizeof(entity.hitpoints));
    entity.noHit = p[0x27A];
    std::memcpy(&entity.aiState, p + 0x288, sizeof(entity.aiState));
}

void WriteEntity(unsigned int slot, const Dr2cEntityView &entity, unsigned int mask)
{
    unsigned char *p = EntityPtr(slot);
    if (mask & DR2C_ENTITY_WRITE_MAP)
        std::memcpy(p + 0x04, &entity.mapId, sizeof(entity.mapId));
    if (mask & DR2C_ENTITY_WRITE_FLAGS) {
        std::memcpy(p + 0x0D, &entity.noCollide, sizeof(entity.noCollide));
        std::memcpy(p + 0x11, &entity.noPick, sizeof(entity.noPick));
        std::memcpy(p + 0x12, &entity.unseen, sizeof(entity.unseen));
        std::memcpy(p + 0x13, &entity.invisible, sizeof(entity.invisible));
        std::memcpy(p + 0x70, &entity.glow, sizeof(entity.glow));
        std::memcpy(p + 0x27A, &entity.noHit, sizeof(entity.noHit));
    }
    if (mask & DR2C_ENTITY_WRITE_POSITION)
        std::memcpy(p + 0x2C, entity.position, sizeof(entity.position));
    if (mask & DR2C_ENTITY_WRITE_VELOCITY)
        std::memcpy(p + 0x38, entity.velocity, sizeof(entity.velocity));
    if (mask & DR2C_ENTITY_WRITE_PHYSICS)
        std::memcpy(p + 0x58, entity.physics, sizeof(entity.physics));
    if (mask & DR2C_ENTITY_WRITE_SPRITE)
        std::memcpy(p + 0xD8, &entity.spriteId, sizeof(entity.spriteId));
    if (mask & DR2C_ENTITY_WRITE_HITPOINTS)
        std::memcpy(p + 0x254, &entity.hitpoints, sizeof(entity.hitpoints));
    if (mask & DR2C_ENTITY_WRITE_AI)
        std::memcpy(p + 0x288, &entity.aiState, sizeof(entity.aiState));
}

void UpdateEntityStats()
{
    for (unsigned int i = 0; i < kThingMaxSlots; ++i)
        ReadEntity(i, g_entities[i]);
}

// ==================== 复制 / 销毁 ====================
void CloneEntity(int slot)
{
    if (slot < 0 || slot >= static_cast<int>(kThingMaxSlots)) return;
    if (g_entities[slot].id == 0 || !pAllocateEntity) return;

    unsigned char *src = EntityPtr(static_cast<unsigned int>(slot));
    uintptr_t dst = pAllocateEntity(g_entities[slot].type);
    if (!dst) return;

    // 从 +0x02 复制到末尾，保留新分配的 id
    std::memcpy(reinterpret_cast<void *>(dst + 0x02),
                src + 0x02,
                kThingStride - 0x02);
}

void DestroyEntity(int slot)
{
    if (slot < 0 || slot >= static_cast<int>(kThingMaxSlots)) return;
    if (g_entities[slot].id == 0 || !pFreeThing) return;
    pFreeThing(reinterpret_cast<uintptr_t>(EntityPtr(static_cast<unsigned int>(slot))));
}

// ==================== Character 读写 ====================
struct CharacterView {
    bool     active = false;
    void*    charPtr = nullptr;
    uint32_t charid  = 0;

    char name[40]        = {};
    char perk[40]        = {};
    char trait[40]       = {};
    char description[120]= {};

    int32_t female = 0;
    int32_t pet    = 0;
    int32_t health = 0;
    float   speed_bonus = 0.0f;

    int32_t baseStat[13]  = {};
    int32_t bonusStat[13] = {};

    int32_t resource[8] = {};

    int32_t weaponStack[3] = {};
    int32_t weaponID[3]    = {};
    int32_t weaponLock[3]  = {};
};

CharacterView g_charBuf;

void WriteStringAt(unsigned char *p, const char *src, size_t maxLen)
{
    memset(p, 0, maxLen);
    size_t len = 0;
    while (len < maxLen && src[len] != 0) ++len;
    if (len > 0) memcpy(p, src, len);
}

void ReadCharacterFromPtr(const unsigned char *p, CharacterView &v)
{
    if (!p) { v.active = false; return; }

    memcpy(v.name, p + 0x1C, 40);          v.name[39] = 0;
    memcpy(v.perk, p + 0x44, 40);          v.perk[39] = 0;
    memcpy(v.trait, p + 0x6C, 40);         v.trait[39] = 0;
    memcpy(v.description, p + 0x144, 120); v.description[119] = 0;

    int16_t female = 0, pet = 0;
    memcpy(&female, p + 0x94, 2);
    memcpy(&pet,    p + 0x96, 2);
    v.female = female;
    v.pet    = pet;

    memcpy(&v.health,      p + 0x140, 4);
    memcpy(&v.speed_bonus, p + 0x1F0, 4);

    for (int i = 0; i < 13; ++i) {
        v.baseStat[i]  = static_cast<int8_t>(p[0x1C9 + i]);
        v.bonusStat[i] = static_cast<int8_t>(p[0x1E3 + i]);
    }

    for (int i = 0; i < 8; ++i)
        memcpy(&v.resource[i], p + 0x288 + i * 4, 4);

    for (int i = 0; i < 3; ++i) {
        memcpy(&v.weaponStack[i], p + 0x2B0 + i * 12 + 0, 4);
        memcpy(&v.weaponID[i],    p + 0x2B0 + i * 12 + 4, 4);
        memcpy(&v.weaponLock[i],  p + 0x2B0 + i * 12 + 8, 4);
    }

    v.active = true;
}

void WriteCharacter()
{
    if (!g_charBuf.active || !g_charBuf.charPtr) return;
    unsigned char *p = reinterpret_cast<unsigned char *>(g_charBuf.charPtr);

    WriteStringAt(p + 0x1C,  g_charBuf.name,  40);
    WriteStringAt(p + 0x44,  g_charBuf.perk,  40);
    WriteStringAt(p + 0x6C,  g_charBuf.trait, 40);
    WriteStringAt(p + 0x144, g_charBuf.description, 120);

    int16_t female = static_cast<int16_t>(g_charBuf.female);
    int16_t pet    = static_cast<int16_t>(g_charBuf.pet);
    memcpy(p + 0x94, &female, 2);
    memcpy(p + 0x96, &pet,    2);

    memcpy(p + 0x140, &g_charBuf.health, 4);
    memcpy(p + 0x1F0, &g_charBuf.speed_bonus, 4);

    for (int i = 0; i < 13; ++i) {
        p[0x1C9 + i] = static_cast<uint8_t>(static_cast<int8_t>(g_charBuf.baseStat[i]));
        p[0x1E3 + i] = static_cast<uint8_t>(static_cast<int8_t>(g_charBuf.bonusStat[i]));
    }

    for (int i = 0; i < 8; ++i)
        memcpy(p + 0x288 + i * 4, &g_charBuf.resource[i], 4);

    for (int i = 0; i < 3; ++i) {
        memcpy(p + 0x2B0 + i * 12 + 0, &g_charBuf.weaponStack[i], 4);
        memcpy(p + 0x2B0 + i * 12 + 4, &g_charBuf.weaponID[i], 4);
        memcpy(p + 0x2B0 + i * 12 + 8, &g_charBuf.weaponLock[i], 4);
    }
}

// ==================== 武器名缓存 ====================
std::vector<std::string> g_weaponNames;
bool g_weaponNamesLoaded = false;

void LoadWeaponNames()
{
    g_weaponNames.clear();
    g_weaponNames.reserve(kWeaponMaxCount);

    for (unsigned int i = 0; i < kWeaponMaxCount; ++i) {
        const char *namePtr = reinterpret_cast<const char *>(
            g_moduleBase + kWeaponPoolRva + i * kWeaponStride);
        size_t len = strnlen(namePtr, 40);
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
    int bestSlot = -1;

    for (unsigned int slot = 1; slot < kThingMaxSlots; ++slot) {
        const unsigned char *thing = EntityPtr(slot);
        unsigned short id = 0;
        uint8_t mapid = 0;
        std::memcpy(&id, thing, sizeof(id));
        std::memcpy(&mapid, thing + 0x04, sizeof(mapid));
        g_currentMapID = ReadUInt8(g_CameraRenderMapLayer);
        if (id == 0 || mapid != g_currentMapID) continue;
        float px = 0.0f, py = 0.0f;
        std::memcpy(&px, thing + 0x2C, sizeof(px));
        std::memcpy(&py, thing + 0x30, sizeof(py));
        const float dx = px - worldX;
        const float dy = py - worldY;
        const float distSq = dx * dx + dy * dy;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestSlot = static_cast<int>(slot);
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
            QueueEntityWrite(static_cast<unsigned int>(g_draggingSlot),
                             entity, DR2C_ENTITY_WRITE_POSITION);
        } else {
            ClearPendingEntityWrite();
            g_draggingSlot = -1;
        }
    }
}

// ==================== 右键触发编辑 ====================
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

    // type == 1 且不是僵尸（zombietype == 0）才读 character
    if (g_entities[g_hoveredSlot].type == 1) {
        unsigned char *thingPtr = EntityPtr(static_cast<unsigned int>(g_hoveredSlot));
        uint32_t zombietype = 0;
        memcpy(&zombietype, thingPtr + 0x14C, 4);

        uint32_t charid = 0;
        memcpy(&charid, thingPtr + 0x148, 4);
        g_charBuf.charid = charid;

        if (zombietype == 0 && charid > 0 && charid < 256 && pGetCharacterData) {
            void *charPtr = pGetCharacterData(charid);
            if (charPtr) {
                g_charBuf.charPtr = charPtr;
                ReadCharacterFromPtr(reinterpret_cast<const unsigned char *>(charPtr),
                                     g_charBuf);
                g_charBuf.charid  = charid;
                g_charBuf.charPtr = charPtr;
            }
        }
    }

    ImGui::OpenPopup("Entity Edit");
}

// ==================== 高亮 ====================
void DrawEntityHighlight()
{
    float cw = 0.0f, ch = 0.0f;
    if (!GetClientSize(cw, ch)) return;

    const int slot = (g_draggingSlot >= 0) ? g_draggingSlot
                     : (g_editTargetSlot >= 0) ? g_editTargetSlot
                                               : g_hoveredSlot;
    if (slot < 0 || slot >= static_cast<int>(kThingMaxSlots)) return;
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

    ImGui::SetNextWindowSize(ImVec2(380.0f, 300.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("DR2C Debug", &g_showDebugPanel);

    ImGui::Checkbox("Drag entity", &g_dragEntity);
    ImGui::SameLine();
    ImGui::Checkbox("Log to file", &g_logToFile);

    ImGui::SliderFloat("Pick radius (world)", &g_pickRadiusWorld, 2.0f, 64.0f, "%.1f");

    if (ImGui::SliderInt("Game FrameRate", &g_fps, 1, 1024)){
        pSetFrameRate(g_fps);
    };

    if (ImGui::InputInt("Current Map", &g_currentMapID)){
        WriteInt(g_currentMapID, g_CameraRenderMapLayer);
    }

    ImGui::Separator();
    ImGui::Text("moduleBase    = %p", reinterpret_cast<void*>(g_moduleBase));
    ImGui::Text("g_ScreenScale = %.4f", g_debug.screenScaleRaw);
    ImGui::Text("camera        = (%.2f, %.2f)", g_debug.cameraX, g_debug.cameraY);
    ImGui::Text("mouse         = (%.1f, %.1f) %s",
                g_debug.mouseX, g_debug.mouseY, g_debug.mouseInside ? "in" : "OUT");
    ImGui::Text("hover=%d  edit=%d  drag=%d",
                g_hoveredSlot, g_editTargetSlot, g_draggingSlot);

    ImGui::Separator();
    ImGui::TextUnformatted("Right-click entity to edit");

    ImGui::End();
}

// ==================== 属性 / 资源名 ====================
const char *kStatNames[13] = {
    "Morale", "Attitude", "Composure", "Charm", "Wits", "Loyalty",
    "Medical", "Mechanical", "Shooting", "Strength", "Dexterity",
    "Fitness", "Vitality"
};

const char *kResourceNames[8] = {
    "None", "Food", "Gas", "Medical", "Bullet", "Rifle", "Shell", "Junk"
};

// ==================== Character 编辑块 ====================
void DrawCharacterSection()
{
    if (!g_charBuf.active || !g_charBuf.charPtr) {
        ImGui::TextUnformatted("(no character attached)");
        if (g_charBuf.charid != 0)
            ImGui::Text("charid = %u", g_charBuf.charid);
        return;
    }

    ImGui::Text("charPtr = %p   charid = %u", g_charBuf.charPtr, g_charBuf.charid);
    ImGui::Separator();

    ImGui::PushItemWidth(240.0f);

    if (ImGui::InputText("Name", g_charBuf.name, sizeof(g_charBuf.name)))
        WriteCharacter();
    if (ImGui::InputInt("Health", &g_charBuf.health))
        WriteCharacter();
    if (ImGui::InputFloat("Speed Bonus", &g_charBuf.speed_bonus, 0.1f, 1.0f, "%.2f"))
        WriteCharacter();

    if (ImGui::TreeNode("Stats")) {
        ImGui::Text("         Base  Bonus  Total");
        for (int i = 0; i < 13; ++i) {
            ImGui::PushID(i);
            ImGui::Text("%-11s", kStatNames[i]);
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

    if (ImGui::TreeNode("Resources")) {
        for (int i = 0; i < 8; ++i) {
            ImGui::PushID(100 + i);
            if (ImGui::InputInt(kResourceNames[i], &g_charBuf.resource[i]))
                WriteCharacter();
            ImGui::PopID();
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Weapons")) {
        if (!g_weaponNamesLoaded) LoadWeaponNames();

        for (int slotIdx = 0; slotIdx < 3; ++slotIdx) {
            ImGui::PushID(200 + slotIdx);
            ImGui::Text("Slot %d", slotIdx);
            ImGui::SameLine();

            int32_t curId = g_charBuf.weaponID[slotIdx];
            const char *curName = "(empty)";
            if (curId > 0 && curId < static_cast<int32_t>(g_weaponNames.size()))
                curName = g_weaponNames[curId].c_str();

            ImGui::PushItemWidth(180.0f);
            bool changed = false;
            if (ImGui::BeginCombo("##weapon", curName)) {
                if (ImGui::Selectable("(empty)", curId == 0)) {
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
            changed |= ImGui::InputInt("Stack", &g_charBuf.weaponStack[slotIdx], 0, 0);
            ImGui::SameLine();
            bool lockFlag = g_charBuf.weaponLock[slotIdx] != 0;
            changed |= ImGui::Checkbox("Lock", &lockFlag);
            g_charBuf.weaponLock[slotIdx] = lockFlag ? 1 : 0;
            //changed |= ImGui::InputInt("Lock",  &g_charBuf.weaponLock[slotIdx],  0, 0);
            ImGui::PopItemWidth();

            if (changed) WriteCharacter();
            ImGui::PopID();
        }
        ImGui::TreePop();
    }

    ImGui::PopItemWidth();
}

// ==================== Vehicle 编辑块 ====================
void DrawVehicleSection(int slot)
{
    unsigned char *p = EntityPtr(static_cast<unsigned int>(slot));

    int32_t chassis      = static_cast<int8_t>(p[0x208]);
    int32_t chassisMax   = static_cast<int8_t>(p[0x209]);
    int32_t engine       = static_cast<int8_t>(p[0x20A]);
    int32_t engineMax    = static_cast<int8_t>(p[0x20B]);
    int32_t armour       = static_cast<int8_t>(p[0x20C]);
    int32_t armourMax    = static_cast<int8_t>(p[0x20D]);
    int32_t carspeed     = static_cast<int8_t>(p[0x20E]);
    int32_t carspeedMax  = static_cast<int8_t>(p[0x20F]);
    int32_t repair = 0;
    float   mpg    = 0.0f;
    memcpy(&repair, p + 0x210, 4);
    memcpy(&mpg,    p + 0x214, 4);

    ImGui::PushItemWidth(120.0f);
    bool changed = false;
    changed |= ImGui::InputInt("Chassis", &chassis);
    changed |= ImGui::InputInt("Chassis Max", &chassisMax);
    changed |= ImGui::InputInt("Engine", &engine);
    changed |= ImGui::InputInt("Engine Max", &engineMax);
    changed |= ImGui::InputInt("Armour", &armour);
    changed |= ImGui::InputInt("Armour Max", &armourMax);
    changed |= ImGui::InputInt("Speed", &carspeed);
    changed |= ImGui::InputInt("Speed Max", &carspeedMax);
    changed |= ImGui::InputInt("Repair", &repair);
    changed |= ImGui::InputFloat("MPG", &mpg, 0.1f, 1.0f, "%.2f");
    ImGui::PopItemWidth();

    if (changed) {
        p[0x208] = static_cast<unsigned char>(static_cast<int8_t>(chassis));
        p[0x209] = static_cast<unsigned char>(static_cast<int8_t>(chassisMax));
        p[0x20A] = static_cast<unsigned char>(static_cast<int8_t>(engine));
        p[0x20B] = static_cast<unsigned char>(static_cast<int8_t>(engineMax));
        p[0x20C] = static_cast<unsigned char>(static_cast<int8_t>(armour));
        p[0x20D] = static_cast<unsigned char>(static_cast<int8_t>(armourMax));
        p[0x20E] = static_cast<unsigned char>(static_cast<int8_t>(carspeed));
        p[0x20F] = static_cast<unsigned char>(static_cast<int8_t>(carspeedMax));
        memcpy(p + 0x210, &repair, 4);
        memcpy(p + 0x214, &mpg, 4);
    }
}

// ==================== Item 编辑块 ====================
void DrawItemSection(int slot)
{
    unsigned char *p = EntityPtr(static_cast<unsigned int>(slot));

    int32_t amount = 0;
    int32_t loot = static_cast<uint8_t>(p[0xE8]);
    memcpy(&amount, p + 0xE4, 4);

    ImGui::PushItemWidth(120.0f);
    bool changed = false;
    changed |= ImGui::InputInt("Amount", &amount);
    //changed |= ImGui::InputInt("Loot", &loot);
    const char *curLootName = (loot >= 0 && loot < 8) ? kResourceNames[loot] : "(invalid)";
    if (ImGui::BeginCombo("Loot", curLootName)) {
        for (int i = 0; i < 8; ++i) {
            const bool selected = (loot == i);
            ImGui::PushID(i);
            if (ImGui::Selectable(kResourceNames[i], selected)) {
                loot = i;
                changed = true;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    if (changed) {
        memcpy(p + 0xE4, &amount, 4);
        p[0xE8] = static_cast<unsigned char>(loot);
    }
}

// ==================== 编辑 Popup ====================
void DrawEntityEditPopup()
{
    if (g_editTargetSlot < 0) return;

    ImGui::SetNextWindowSize(ImVec2(380.0f, 500.0f), ImGuiCond_Appearing);

    if (ImGui::BeginPopup("Entity Edit")) {
        const int slot = g_editTargetSlot;
        if (slot < 0 || slot >= static_cast<int>(kThingMaxSlots)
            || g_entities[slot].id == 0) {
            ImGui::TextUnformatted("(entity no longer exists)");
            if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        Dr2cEntityView &entity = g_entities[slot];
        const unsigned int uslot = static_cast<unsigned int>(slot);

        ImGui::Text("Slot %d   ID %u   Type %u / %u",
                    slot, entity.id, entity.type, entity.subtype);
        ImGui::Separator();

        if (ImGui::Button("Clone", ImVec2(80, 0))) CloneEntity(slot);
        ImGui::SameLine();
        if (ImGui::Button("Destroy", ImVec2(80, 0))) {
            DestroyEntity(slot);
            g_editTargetSlot = -1;
            ClearPendingEntityWrite();
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(80, 0))) ImGui::CloseCurrentPopup();

        ImGui::Separator();

        // ---------- 实体字段 ----------
        if (ImGui::CollapsingHeader("Entity", ImGuiTreeNodeFlags_None)) {
            ImGui::PushItemWidth(240.0f);
            int mapId = entity.mapId;
            int spriteId = entity.spriteId;
            bool noCollide = entity.noCollide != 0;
            bool noPick = entity.noPick != 0;
            bool unseen = entity.unseen != 0;
            bool invisible = entity.invisible != 0;
            bool noHit = entity.noHit != 0;
            bool glow = entity.glow != 0;

            if (ImGui::InputFloat3("Position", entity.position))
                QueueEntityWrite(uslot, entity, DR2C_ENTITY_WRITE_POSITION);
            if (ImGui::InputInt("Map", &mapId)) {
                entity.mapId = static_cast<unsigned char>(mapId);
                QueueEntityWrite(uslot, entity, DR2C_ENTITY_WRITE_MAP);
            }
            if (ImGui::InputFloat3("Velocity", entity.velocity))
                QueueEntityWrite(uslot, entity, DR2C_ENTITY_WRITE_VELOCITY);
            if (ImGui::InputFloat3("Physics", entity.physics))
                QueueEntityWrite(uslot, entity, DR2C_ENTITY_WRITE_PHYSICS);
            if (ImGui::InputInt("Hitpoints", &entity.hitpoints))
                QueueEntityWrite(uslot, entity, DR2C_ENTITY_WRITE_HITPOINTS);
            if (ImGui::InputInt("Sprite", &spriteId)) {
                entity.spriteId = static_cast<unsigned short>(spriteId);
                QueueEntityWrite(uslot, entity, DR2C_ENTITY_WRITE_SPRITE);
            }
            if (ImGui::InputScalar("AI state", ImGuiDataType_U32, &entity.aiState))
                QueueEntityWrite(uslot, entity, DR2C_ENTITY_WRITE_AI);

            bool flagsChanged = false;
            flagsChanged |= ImGui::Checkbox("No collision", &noCollide);
            ImGui::SameLine();
            flagsChanged |= ImGui::Checkbox("No pickup", &noPick);
            flagsChanged |= ImGui::Checkbox("Unseen", &unseen);
            ImGui::SameLine();
            flagsChanged |= ImGui::Checkbox("Invisible", &invisible);
            flagsChanged |= ImGui::Checkbox("No hit", &noHit);
            ImGui::SameLine();
            flagsChanged |= ImGui::Checkbox("Glow", &glow);

            entity.noCollide = static_cast<unsigned char>(noCollide);
            entity.noPick    = static_cast<unsigned char>(noPick);
            entity.unseen    = static_cast<unsigned char>(unseen);
            entity.invisible = static_cast<unsigned char>(invisible);
            entity.noHit     = static_cast<unsigned char>(noHit);
            entity.glow      = static_cast<unsigned char>(glow);

            if (flagsChanged)
                QueueEntityWrite(uslot, entity, DR2C_ENTITY_WRITE_FLAGS);
            ImGui::PopItemWidth();
        }

        // ---------- Character（type == 1)  ----------
        if (entity.type == 1) {
            if (ImGui::CollapsingHeader("Character", ImGuiTreeNodeFlags_None))
                DrawCharacterSection();
        }

        // ---------- Item（type == 3 且 subtype == 1） ----------
        if (entity.type == 3 && entity.subtype == 1) {
            if (ImGui::CollapsingHeader("Item", ImGuiTreeNodeFlags_None))
                DrawItemSection(slot);
        }

        // ---------- Vehicle（type == 3 且 subtype == 3） ----------
        if (entity.type == 3 && entity.subtype == 3) {
            if (ImGui::CollapsingHeader("Vehicle", ImGuiTreeNodeFlags_None))
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

void QueueEntityWrite(unsigned int slot, const Dr2cEntityView &entity, unsigned int mask)
{
    if (slot >= kThingMaxSlots) return;
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

    if (!ImGui_ImplWin32_Init(window) || !ImGui_ImplOpenGL3_Init("#version 130")) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }
    g_window = window;
    g_initialized = true;
    LogInit(module);
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