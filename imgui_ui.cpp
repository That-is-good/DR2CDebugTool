#include "imgui_ui.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"
#include <windows.h>
#include <gdiplus.h>
#include <gl/GL.h>
#include <cstdio>
#include <cstring>

using namespace Gdiplus;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam);

namespace {

HWND g_window = nullptr;
bool g_initialized = false;
bool g_showWindow = true;
unsigned int g_frameCount = 0;
unsigned int g_entityCount = 0;
int g_selectedSlot = -1;


struct MapView {
    bool visible = false;
    int width = 1024;
    int height = 1024;
    ImVec2 origin = ImVec2(0.0f, 0.0f);
};

MapView g_maps[64] = {};
float g_mapZoom = 0.55f;

Dr2cEntityView g_entities[610] = {};
Dr2cEntityView g_pendingEntity = {};
unsigned int g_pendingSlot = 0;
volatile LONG g_pendingEntityWrite = 0;
bool g_keepEntityWrite = false;
unsigned int g_pendingWriteMask = 0;
ULONG_PTR g_gdiplusToken = 0;
GLuint g_entityTextures[8] = {};
bool g_entityTexturesLoaded = false;
int g_dragSlot = -1;
ImVec2 g_dragOffset = ImVec2(0.0f, 0.0f);

const unsigned char *EntityAddress(unsigned int slot)
{
    const uintptr_t moduleBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    return reinterpret_cast<const unsigned char *>(moduleBase + 0x5632E0u + slot * 0x304u);
}

void ReadEntity(unsigned int slot, Dr2cEntityView &entity)
{
    const unsigned char *thing = EntityAddress(slot);
    std::memcpy(&entity.id, thing + 0x00, sizeof(entity.id));
    entity.type = thing[0x02];
    entity.subtype = thing[0x03];
    entity.mapId = thing[0x04];
    entity.noCollide = thing[0x0D];
    entity.noPick = thing[0x11];
    entity.unseen = thing[0x12];
    entity.invisible = thing[0x13];
    std::memcpy(entity.position, thing + 0x2C, sizeof(entity.position));
    std::memcpy(entity.velocity, thing + 0x38, sizeof(entity.velocity));
    std::memcpy(entity.physics, thing + 0x58, sizeof(entity.physics));
    entity.glow = thing[0x70];
    std::memcpy(&entity.spriteId, thing + 0xD8, sizeof(entity.spriteId));
    std::memcpy(&entity.hitpoints, thing + 0x254, sizeof(entity.hitpoints));
    entity.noHit = thing[0x27A];
    std::memcpy(&entity.aiState, thing + 0x288, sizeof(entity.aiState));
}

void WriteEntity(unsigned int slot, const Dr2cEntityView &entity, unsigned int mask)
{
    unsigned char *thing = const_cast<unsigned char *>(EntityAddress(slot));
    if (mask & DR2C_ENTITY_WRITE_MAP)
        std::memcpy(thing + 0x04, &entity.mapId, sizeof(entity.mapId));
    if (mask & DR2C_ENTITY_WRITE_FLAGS) {
        std::memcpy(thing + 0x0D, &entity.noCollide, sizeof(entity.noCollide));
        std::memcpy(thing + 0x11, &entity.noPick, sizeof(entity.noPick));
        std::memcpy(thing + 0x12, &entity.unseen, sizeof(entity.unseen));
        std::memcpy(thing + 0x13, &entity.invisible, sizeof(entity.invisible));
        std::memcpy(thing + 0x70, &entity.glow, sizeof(entity.glow));
        std::memcpy(thing + 0x27A, &entity.noHit, sizeof(entity.noHit));
    }
    if (mask & DR2C_ENTITY_WRITE_POSITION)
        std::memcpy(thing + 0x2C, entity.position, sizeof(entity.position));
    if (mask & DR2C_ENTITY_WRITE_VELOCITY)
        std::memcpy(thing + 0x38, entity.velocity, sizeof(entity.velocity));
    if (mask & DR2C_ENTITY_WRITE_PHYSICS)
        std::memcpy(thing + 0x58, entity.physics, sizeof(entity.physics));
    if (mask & DR2C_ENTITY_WRITE_SPRITE)
        std::memcpy(thing + 0xD8, &entity.spriteId, sizeof(entity.spriteId));
    if (mask & DR2C_ENTITY_WRITE_HITPOINTS)
        std::memcpy(thing + 0x254, &entity.hitpoints, sizeof(entity.hitpoints));
    if (mask & DR2C_ENTITY_WRITE_AI)
        std::memcpy(thing + 0x288, &entity.aiState, sizeof(entity.aiState));
}

void UpdateEntityStats()
{
    unsigned int count = 0;
    for (unsigned int index = 0; index < 610u; ++index) {
        ReadEntity(index, g_entities[index]);
        if (g_entities[index].id != 0)
            ++count;
    }
    g_entityCount = count;
}

GLuint LoadPngTexture(const wchar_t *path)
{
    Bitmap bitmap(path, FALSE);
    if (bitmap.GetLastStatus() != Ok)
        return 0;

    const UINT width = bitmap.GetWidth();
    const UINT height = bitmap.GetHeight();
    Rect rect(0, 0, static_cast<INT>(width), static_cast<INT>(height));
    BitmapData data = {};
    if (bitmap.LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &data) != Ok)
        return 0;

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width),
                 static_cast<GLsizei>(height), 0, 0x80E1 /* GL_BGRA_EXT */,
                 GL_UNSIGNED_BYTE, data.Scan0);
    glBindTexture(GL_TEXTURE_2D, 0);
    bitmap.UnlockBits(&data);
    return texture;
}

void LoadEntityTextures(HMODULE module)
{
    if (g_entityTexturesLoaded)
        return;
    GdiplusStartupInput input;
    if (GdiplusStartup(&g_gdiplusToken, &input, nullptr) != Ok)
        return;

    wchar_t modulePath[MAX_PATH] = {};
    GetModuleFileNameW(module, modulePath, MAX_PATH);
    wchar_t *slash = wcsrchr(modulePath, L'\\');
    if (slash)
        *(slash + 1) = L'\0';

    const wchar_t *files[8] = {
        L"Icons\\player\\anon.png", L"Icons\\zombie\\lks.png",
        L"Icons\\furniture\\sofa.png", L"Icons\\projectile\\tear.png",
        L"Icons\\item\\box.png", L"Icons\\weapon\\farmer.png",
        L"Icons\\vehicle\\car.png", L"Icons\\unknown\\9.png"
    };
    for (int index = 0; index < 8; ++index) {
        wchar_t path[MAX_PATH] = {};
        wcsncpy_s(path, MAX_PATH, modulePath, _TRUNCATE);
        wcsncat_s(path, MAX_PATH, files[index], _TRUNCATE);
        g_entityTextures[index] = LoadPngTexture(path);
    }
    g_entityTexturesLoaded = true;
}

void ReleaseEntityTextures()
{
    if (!g_entityTexturesLoaded)
        return;
    glDeleteTextures(8, g_entityTextures);
    GdiplusShutdown(g_gdiplusToken);
    g_gdiplusToken = 0;
    g_entityTexturesLoaded = false;
}

int EntityTextureIndex(const Dr2cEntityView &entity)
{
    if (entity.type == 1) return 0;
    if (entity.type == 2) return 1;
    if (entity.type == 4) return 3;
    if (entity.type != 3) return 7;
    switch (entity.subtype) {
    case 0: return 2;
    case 1: return 4;
    case 2: return 5;
    case 3: return 6;
    default: return 7;
    }
}

void DrawMapCanvas();

void DrawEntityPanel()
{
    ImGui::SetNextWindowSize(ImVec2(940.0f, 560.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("DR2C Entities", &g_showWindow,
             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::BeginChild("entity-map-panel", ImVec2(-360.0f, 0.0f), ImGuiChildFlags_Borders);
    DrawMapCanvas();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("entity-details", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);

    static int s_lastSelected = -1;
    if (s_lastSelected != g_selectedSlot) {
        ClearPendingEntityWrite();
        s_lastSelected = g_selectedSlot;
    }

    if (g_selectedSlot >= 0 && g_selectedSlot < 610
        && g_entities[g_selectedSlot].id != 0) {
        // 直接引用实时数据，UI 自动反映最新值
        Dr2cEntityView &entity = g_entities[g_selectedSlot];
        const unsigned int slot = static_cast<unsigned int>(g_selectedSlot);

        // 去掉 Address 显示
        ImGui::Text("Slot %d   ID %u   Type %u / %u",
                    g_selectedSlot, entity.id, entity.type, entity.subtype);
        ImGui::Separator();

        int mapId = entity.mapId;
        int spriteId = entity.spriteId;
        bool noCollide = entity.noCollide != 0;
        bool noPick = entity.noPick != 0;
        bool unseen = entity.unseen != 0;
        bool invisible = entity.invisible != 0;
        bool noHit = entity.noHit != 0;
        bool glow = entity.glow != 0;

        if (ImGui::InputInt("Map", &mapId)) {
            entity.mapId = static_cast<unsigned char>(mapId);
            QueueEntityWrite(slot, entity, DR2C_ENTITY_WRITE_MAP);
        }
        if (ImGui::InputFloat3("Velocity", entity.velocity))
            QueueEntityWrite(slot, entity, DR2C_ENTITY_WRITE_VELOCITY);
        if (ImGui::InputFloat3("Physics", entity.physics))
            QueueEntityWrite(slot, entity, DR2C_ENTITY_WRITE_PHYSICS);
        if (ImGui::InputInt("Hitpoints", &entity.hitpoints))
            QueueEntityWrite(slot, entity, DR2C_ENTITY_WRITE_HITPOINTS);
        if (ImGui::InputInt("Sprite", &spriteId)) {
            entity.spriteId = static_cast<unsigned short>(spriteId);
            QueueEntityWrite(slot, entity, DR2C_ENTITY_WRITE_SPRITE);
        }
        if (ImGui::InputScalar("AI state", ImGuiDataType_U32, &entity.aiState))
            QueueEntityWrite(slot, entity, DR2C_ENTITY_WRITE_AI);

        bool flagsChanged = false;
        flagsChanged |= ImGui::Checkbox("No collision", &noCollide);
        flagsChanged |= ImGui::Checkbox("No pickup", &noPick);
        flagsChanged |= ImGui::Checkbox("Unseen", &unseen);
        flagsChanged |= ImGui::Checkbox("Invisible", &invisible);
        flagsChanged |= ImGui::Checkbox("No hit", &noHit);
        flagsChanged |= ImGui::Checkbox("Glow", &glow);

        entity.noCollide = static_cast<unsigned char>(noCollide);
        entity.noPick = static_cast<unsigned char>(noPick);
        entity.unseen = static_cast<unsigned char>(unseen);
        entity.invisible = static_cast<unsigned char>(invisible);
        entity.noHit = static_cast<unsigned char>(noHit);
        entity.glow = static_cast<unsigned char>(glow);

        if (flagsChanged)
            QueueEntityWrite(slot, entity, DR2C_ENTITY_WRITE_FLAGS);
    } else {
        ImGui::TextUnformatted("Select an active entity.");
    }

    ImGui::EndChild();
    ImGui::End();
}

} // namespace

void QueueEntityWrite(unsigned int slot, const Dr2cEntityView &entity, unsigned int mask)
{
    if (slot >= 610u)
        return;
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
    if (g_initialized || !window)
        return g_initialized;

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

    LoadEntityTextures(module);
    g_window = window;
    g_initialized = true;
    return true;
}

void ShutdownInternalUi()
{
    if (!g_initialized)
        return;
    ReleaseEntityTextures();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    g_window = nullptr;
    g_initialized = false;
}

void RenderInternalUi()
{
    if (!g_initialized)
        return;

    ++g_frameCount;
    UpdateEntityStats();
    ApplyPendingEntityWrite();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (g_showWindow)
        DrawEntityPanel();

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

namespace {

void UpdateMapLayout()
{
    for (MapView &map : g_maps)
        map = MapView();

    int maxMap = 0;
    for (const Dr2cEntityView &entity : g_entities) {
        if (entity.id != 0 && entity.mapId < 64)
            maxMap = (entity.mapId > maxMap) ? entity.mapId : maxMap;
    }

    for (int mapId = 0; mapId <= maxMap; ++mapId) {
        const uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        const unsigned char *meta = reinterpret_cast<const unsigned char *>(
            base + 0x46DBC0u + static_cast<uintptr_t>(mapId) * 0x34u);
        int width = 0;
        int height = 0;
        int tileWidth = 0;
        int tileHeight = 0;
        std::memcpy(&width, meta + 0x08, sizeof(width));
        std::memcpy(&height, meta + 0x0C, sizeof(height));
        std::memcpy(&tileWidth, meta + 0x10, sizeof(tileWidth));
        std::memcpy(&tileHeight, meta + 0x14, sizeof(tileHeight));
        int pixelWidth = 0;
        int pixelHeight = 0;
        std::memcpy(&pixelWidth, meta + 0x20, sizeof(pixelWidth));
        std::memcpy(&pixelHeight, meta + 0x24, sizeof(pixelHeight));
        if (pixelWidth <= 0)
            pixelWidth = width * tileWidth;
        if (pixelHeight <= 0)
            pixelHeight = height * tileHeight;
        if (pixelWidth <= 0)
            pixelWidth = 1024;
        if (pixelHeight <= 0)
            pixelHeight = 1024;
        g_maps[mapId].visible = true;
        g_maps[mapId].width = pixelWidth;
        g_maps[mapId].height = pixelHeight;
    }

    const float gap = 36.0f;
    const int columns = 3;
    float cursorX = gap;
    float cursorY = gap;
    float rowHeight = 0.0f;
    int column = 0;
    for (int mapId = 0; mapId <= maxMap; ++mapId) {
        if (!g_maps[mapId].visible)
            continue;
        g_maps[mapId].origin = ImVec2(cursorX, cursorY);
        const float mapHeight = g_maps[mapId].height * g_mapZoom;
        const float mapWidth = g_maps[mapId].width * g_mapZoom;
        rowHeight = (mapHeight > rowHeight) ? mapHeight : rowHeight;
        ++column;
        if (column == columns) {
            column = 0;
            cursorX = gap;
            cursorY += rowHeight + gap;
            rowHeight = 0.0f;
        } else {
            cursorX += mapWidth + gap;
        }
    }
}

ImU32 EntityColor(const Dr2cEntityView &entity)
{
    switch (entity.type) {
    case 1: return IM_COL32(80, 210, 120, 255);
    case 2: return IM_COL32(230, 90, 80, 255);
    case 3: return IM_COL32(240, 190, 70, 255);
    case 4: return IM_COL32(120, 170, 245, 255);
    default: return IM_COL32(210, 210, 210, 255);
    }
}

void DrawMapCanvas()
{
    UpdateMapLayout();
    ImGui::Text("Active entities: %u / 610", g_entityCount);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat("Zoom", &g_mapZoom, 0.15f, 1.25f, "%.2fx");

    ImGui::BeginChild("entity-map-canvas", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders,
                  ImGuiWindowFlags_HorizontalScrollbar |
                  ImGuiWindowFlags_NoMove |
                  ImGuiWindowFlags_NoScrollWithMouse);
    const ImVec2 canvasOrigin = ImGui::GetCursorScreenPos();
    ImVec2 contentSize(2000.0f, 1400.0f);
    for (const MapView &map : g_maps) {
        if (!map.visible)
            continue;
        contentSize.x = (map.origin.x + map.width * g_mapZoom + 50.0f > contentSize.x)
            ? map.origin.x + map.width * g_mapZoom + 50.0f : contentSize.x;
        contentSize.y = (map.origin.y + map.height * g_mapZoom + 50.0f > contentSize.y)
            ? map.origin.y + map.height * g_mapZoom + 50.0f : contentSize.y;
    }
    ImGui::InvisibleButton("##entity-canvas", contentSize,
                           ImGuiButtonFlags_MouseButtonLeft);
    const bool canvasHovered = ImGui::IsItemHovered();
    const bool canvasActive  = ImGui::IsItemActive();

    ImDrawList *draw = ImGui::GetWindowDrawList();
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    const bool dragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left);

    for (int mapId = 0; mapId < 64; ++mapId) {
        const MapView &map = g_maps[mapId];
        if (!map.visible)
            continue;
        const ImVec2 topLeft(canvasOrigin.x + map.origin.x, canvasOrigin.y + map.origin.y);
        const ImVec2 bottomRight(topLeft.x + map.width * g_mapZoom,
                                 topLeft.y + map.height * g_mapZoom);
        draw->AddRectFilled(topLeft, bottomRight, IM_COL32(38, 45, 52, 230));
        draw->AddRect(topLeft, bottomRight, IM_COL32(130, 150, 165, 255), 0.0f, 0, 2.0f);
        char title[32] = {};
        std::snprintf(title, sizeof(title), "Map %d", mapId);
        draw->AddText(ImVec2(topLeft.x + 8.0f, topLeft.y + 6.0f),
                      IM_COL32(230, 230, 230, 255), title);
    }

    for (unsigned int slot = 0; slot < 610u; ++slot) {
        const Dr2cEntityView &entity = g_entities[slot];
        if (entity.id == 0 || entity.mapId >= 64 || !g_maps[entity.mapId].visible)
            continue;

        const MapView &map = g_maps[entity.mapId];
        const ImVec2 mapOrigin(canvasOrigin.x + map.origin.x,
                               canvasOrigin.y + map.origin.y);
        const ImVec2 center(mapOrigin.x + entity.position[0] * g_mapZoom,
                            mapOrigin.y + entity.position[1] * g_mapZoom);

        // 修复图标缩放
        const float iconSize = 30.0f * g_mapZoom;
        const float iconSizeClamped = (iconSize < 8.0f) ? 8.0f : (iconSize > 48.0f ? 48.0f : iconSize);

        const ImVec2 iconMin(center.x - iconSizeClamped * 0.5f, center.y - iconSizeClamped * 0.5f);
        const ImVec2 iconMax(center.x + iconSizeClamped * 0.5f, center.y + iconSizeClamped * 0.5f);

        const GLuint texture = g_entityTextures[EntityTextureIndex(entity)];
        if (texture != 0)
            draw->AddImage(ImTextureRef(static_cast<ImTextureID>(texture)),
                           iconMin, iconMax);
        else
            draw->AddCircleFilled(center, 6.0f, EntityColor(entity));

        if (g_selectedSlot == static_cast<int>(slot))
            draw->AddRect(iconMin, iconMax, IM_COL32(255, 255, 255, 255), 0.0f, 0, 2.0f);

        const float dx = mouse.x - center.x;
        const float dy = mouse.y - center.y;
        const float hitRadius = iconSize * 0.5f + 5.0f;

        if (canvasHovered && clicked && dx * dx + dy * dy < hitRadius * hitRadius) {
            g_selectedSlot = static_cast<int>(slot);
            g_dragSlot = static_cast<int>(slot);
            g_dragOffset = ImVec2(mouse.x - center.x, mouse.y - center.y);
            ImGui::SetNextFrameWantCaptureMouse(true);   // 新增
        }

        if (canvasActive && dragging && g_dragSlot == static_cast<int>(slot)) {
            Dr2cEntityView moved = entity;
            moved.position[0] = (mouse.x - g_dragOffset.x - mapOrigin.x) / g_mapZoom;
            moved.position[1] = (mouse.y - g_dragOffset.y - mapOrigin.y) / g_mapZoom;
            QueueEntityWrite(slot, moved, DR2C_ENTITY_WRITE_POSITION);
            ImGui::SetNextFrameWantCaptureMouse(true);   // 新增
        }
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        g_dragSlot = -1;
    ImGui::EndChild();
}

} // namespace