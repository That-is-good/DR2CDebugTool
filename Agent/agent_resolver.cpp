#include "agent_resolver.h"

#include "agent_memory.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace dr2c {
namespace {

struct Pattern {
    std::vector<unsigned char> bytes;
    std::vector<unsigned char> mask;
    bool valid = false;
};

Pattern ParsePattern(const char *text)
{
    Pattern result;
    const std::string s(text);
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && s[i] == ' ')
            ++i;
        if (i >= s.size())
            break;
        if (i + 1 < s.size() && s[i] == '?' && s[i + 1] == '?') {
            result.bytes.push_back(0);
            result.mask.push_back(0);
            i += 2;
            continue;
        }
        if (i + 1 >= s.size()) {
            result.bytes.clear();
            result.mask.clear();
            return result;
        }
        char pair[3] = {s[i], s[i + 1], 0};
        char *end = nullptr;
        const unsigned long value = std::strtoul(pair, &end, 16);
        if (*end != '\0') {
            result.bytes.clear();
            result.mask.clear();
            return result;
        }
        result.bytes.push_back(static_cast<unsigned char>(value));
        result.mask.push_back(0xff);
        i += 2;
    }
    result.valid = !result.bytes.empty() && result.bytes.size() == result.mask.size();
    return result;
}

bool FindUnique(const std::vector<unsigned char> &image,
                const Pattern &pattern, int *offset, int *matchCount = nullptr)
{
    if (matchCount)
        *matchCount = 0;
    if (!pattern.valid || pattern.bytes.size() > image.size())
        return false;

    int firstMatch = -1;
    int count = 0;
    const std::size_t last = image.size() - pattern.bytes.size();
    for (std::size_t i = 0; i <= last; ++i) {
        bool matches = true;
        for (std::size_t j = 0; j < pattern.bytes.size(); ++j) {
            if (pattern.mask[j] != 0 && image[i + j] != pattern.bytes[j]) {
                matches = false;
                break;
            }
        }
        if (matches) {
            if (firstMatch < 0)
                firstMatch = static_cast<int>(i);
            ++count;
        }
    }
    if (matchCount)
        *matchCount = count;
    if (offset)
        *offset = count == 1 ? firstMatch : -1;
    return count == 1;
}

int FindBytes(const std::vector<unsigned char> &image,
              const unsigned char *bytes, std::size_t size, int from, int end)
{
    if (from < 0)
        from = 0;
    if (end > static_cast<int>(image.size()))
        end = static_cast<int>(image.size());
    if (size == 0 || from > end || size > static_cast<std::size_t>(end - from))
        return -1;
    for (int i = from; i <= end - static_cast<int>(size); ++i) {
        if (std::memcmp(image.data() + i, bytes, size) == 0)
            return i;
    }
    return -1;
}

std::uint32_t ReadU32(const std::vector<unsigned char> &bytes, int offset)
{
    if (offset < 0 || offset + 4 > static_cast<int>(bytes.size()))
        return 0;
    return static_cast<std::uint32_t>(bytes[offset])
           | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8)
           | (static_cast<std::uint32_t>(bytes[offset + 2]) << 16)
           | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

std::uint32_t ToRva(std::uint32_t va, std::uint32_t imageBase, std::uint32_t imageSize)
{
    if (va < imageBase)
        return 0;
    const std::uint32_t rva = va - imageBase;
    return rva < imageSize ? rva : 0;
}

bool AbsoluteToRva(std::uint32_t absolute, std::uint32_t imageBase,
                   std::uint32_t imageSize, std::uint32_t *rva)
{
    const std::uint32_t result = ToRva(absolute, imageBase, imageSize);
    if (result == 0)
        return false;
    *rva = result;
    return true;
}

bool Required(const std::vector<unsigned char> &section, std::uint32_t sectionRva,
              const Pattern &pattern, std::uint32_t *rva)
{
    int offset = -1;
    if (!FindUnique(section, pattern, &offset) || offset < 0)
        return false;
    if (static_cast<std::uint64_t>(sectionRva) + static_cast<std::uint64_t>(offset) > 0xFFFFFFFFull)
        return false;
    *rva = sectionRva + static_cast<std::uint32_t>(offset);
    return true;
}

void SetError(LayoutInfo *layout, const char *message)
{
    layout->valid = 0;
    std::snprintf(layout->error, sizeof(layout->error), "%s", message);
}

void AppendWarning(LayoutInfo *layout, const char *message)
{
    const std::size_t used = std::strlen(layout->warning);
    if (used < sizeof(layout->warning) - 1)
        std::snprintf(layout->warning + used, sizeof(layout->warning) - used, "%s", message);
}

} // namespace

bool ResolveLayout(std::uint32_t moduleBase, LayoutInfo *layout)
{
    if (!layout)
        return false;
    std::memset(layout, 0, sizeof(*layout));
    if (!moduleBase) {
        SetError(layout, "游戏主模块基址为空。");
        return false;
    }

    IMAGE_DOS_HEADER dos{};
    if (!SafeRead(moduleBase, &dos, sizeof(dos)) ||
        dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0) {
        SetError(layout, "目标进程不是有效的 PE 模块。");
        return false;
    }

    Address ntAddress = static_cast<Address>(moduleBase) +
                        static_cast<std::uint32_t>(dos.e_lfanew);
    std::uint32_t signature = 0;
    IMAGE_FILE_HEADER fileHeader{};
    IMAGE_OPTIONAL_HEADER32 optionalHeader{};
    if (!SafeRead(ntAddress, &signature, sizeof(signature)) ||
        signature != IMAGE_NT_SIGNATURE ||
        !SafeRead(ntAddress + sizeof(signature), &fileHeader, sizeof(fileHeader)) ||
        !SafeRead(ntAddress + sizeof(signature) + sizeof(fileHeader),
                  &optionalHeader, sizeof(optionalHeader)) ||
        fileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        optionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        optionalHeader.SizeOfImage == 0 ||
        fileHeader.NumberOfSections == 0 ||
        fileHeader.NumberOfSections > 96) {
        SetError(layout, "目标模块格式不受支持（需要 32 位 x86 PE）。");
        return false;
    }

    layout->imageBase = optionalHeader.ImageBase;
    layout->imageSize = optionalHeader.SizeOfImage;

    const Address sectionTable = ntAddress + sizeof(signature) + sizeof(fileHeader) +
                                 fileHeader.SizeOfOptionalHeader;
    IMAGE_SECTION_HEADER textSection{};
    bool foundText = false;
    for (std::uint16_t i = 0; i < fileHeader.NumberOfSections; ++i) {
        IMAGE_SECTION_HEADER section{};
        if (!SafeRead(sectionTable + static_cast<Address>(i) * sizeof(section),
                      &section, sizeof(section)))
            continue;
        if (std::memcmp(section.Name, ".text", 5) == 0 &&
            (section.Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0) {
            textSection = section;
            foundText = true;
            break;
        }
    }

    if (!foundText) {
        SetError(layout, "目标模块缺少有效的可执行代码节。");
        return false;
    }

    const std::uint32_t sectionSize =
        std::max(textSection.Misc.VirtualSize, textSection.SizeOfRawData);
    if (sectionSize == 0 || sectionSize > 32u * 1024u * 1024u ||
        static_cast<std::uint64_t>(textSection.VirtualAddress) + sectionSize >
            optionalHeader.SizeOfImage) {
        SetError(layout, "目标模块代码节尺寸异常。");
        return false;
    }

    std::vector<unsigned char> section(sectionSize);
    if (!SafeRead(static_cast<Address>(moduleBase) + textSection.VirtualAddress,
                  section.data(), sectionSize)) {
        SetError(layout, "无法读取目标模块代码节。");
        return false;
    }
    const std::uint32_t sectionRva = textSection.VirtualAddress;
    layout->codeSectionRva = sectionRva;
    layout->codeSectionSize = sectionSize;

    const std::uint32_t imageBase = layout->imageBase;
    const std::uint32_t imageSize = layout->imageSize;

    const Pattern allocateEntity = ParsePattern(
        "55 57 56 53 BE ?? ?? ?? ?? 83 EC 3C 8B 2D ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? "
        "C7 05 ?? ?? ?? ?? 00 00 00 00");
    const Pattern allocateThing = ParsePattern(
        "53 83 EC 18 C7 04 24 03 00 00 00 E8 ?? ?? ?? ?? 85 C0 89 C3 74 ?? "
        "8B 44 24 20 88 43 03 E8 ?? ?? ?? ?? 88 43 04 83 C4 18 89 D8 5B C3");
    const Pattern allocateCharacter = ParsePattern(
        "83 EC 1C C7 04 24 12 00 00 00 E8 ?? ?? ?? ?? 83 C4 1C C3");
    const Pattern assignCharacter = ParsePattern(
        "56 53 83 EC 14 8B 5C 24 24 8B 74 24 20 89 1C 24 E8 ?? ?? ?? ?? "
        "85 C0 74 ?? 0F B7 16 89 9E 48 01 00 00 66 89 50 04 83 C4 14 "
        "B8 01 00 00 00 5B 5E C3");
    const Pattern setPlayer = ParsePattern(
        "8B 54 24 04 B8 ?? ?? ?? ?? 39 90 68 BE 00 00 74 ?? 83 C0 04 "
        "3D ?? ?? ?? ?? 75 ?? A1 ?? ?? ?? ?? 89 54 24 04 83 F8 04 19 C9 "
        "21 C8 89 14 85 ?? ?? ?? ?? E9 ?? ?? ?? ??");
    const Pattern switchLayer = ParsePattern(
        "57 56 53 83 EC 10 A1 ?? ?? ?? ?? 8B 5C 24 20 39 D8 0F 84 ?? ?? ?? ?? "
        "89 04 24 E8 ?? ?? ?? ??");
    const Pattern setMapLayer = ParsePattern(
        "8B 54 24 04 31 C0 81 FA FE 00 00 00 77 ?? A1 ?? ?? ?? ?? "
        "89 15 ?? ?? ?? ?? 6B D2 ?? 81 C2 ?? ?? ?? ?? 89 15 ?? ?? ?? ?? F3 C3");
    const Pattern weaponAccessor = ParsePattern(
        "8B 44 24 04 C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 3D ?? ?? ?? ?? 76 ?? "
        "B8 ?? ?? ?? ?? 69 C0 ?? ?? ?? ?? 05 ?? ?? ?? ?? C3");
    const Pattern createWeaponEntity = ParsePattern(
        "56 53 83 EC 14 8B 5C 24 20 C7 04 24 03 00 00 00 E8 ?? ?? ?? ?? "
        "89 04 24 89 C6 89 5C 24 04 E8 ?? ?? ?? ?? 81 FB 01 04 00 00");

    struct RequiredFunction {
        FunctionId id;
        const Pattern *pattern;
        const char *name;
    };
    const RequiredFunction requiredFunctions[] = {
        {FunctionId::AllocateEntity, &allocateEntity, "AllocateEntity"},
        {FunctionId::AllocateThing, &allocateThing, "AllocateThing"},
        {FunctionId::AllocateCharacterSlot, &allocateCharacter, "AllocateCharacterSlot"},
        {FunctionId::AssignCharacterToThing, &assignCharacter, "AssignCharacterToThing"},
        {FunctionId::SetCurrentPlayerThing, &setPlayer, "SetCurrentPlayerThing"},
        {FunctionId::SwitchActiveTileLayer, &switchLayer, "SwitchActiveTileLayer"},
    };

    std::string unresolved;
    for (const RequiredFunction &item : requiredFunctions) {
        std::uint32_t rva = 0;
        if (!Required(section, sectionRva, *item.pattern, &rva)) {
            if (!unresolved.empty())
                unresolved += ", ";
            unresolved += item.name;
            continue;
        }
        layout->functionRvas[static_cast<std::uint32_t>(item.id)] = rva;
    }
    if (!unresolved.empty()) {
        char message[512];
        std::snprintf(message, sizeof(message),
                      "无法唯一解析核心函数: %s；为避免错误读写，已拒绝附加。",
                      unresolved.c_str());
        SetError(layout, message);
        return false;
    }

    // 可选：武器实体创建函数
    {
        std::uint32_t createWeaponRva = 0;
        if (Required(section, sectionRva, createWeaponEntity, &createWeaponRva))
            layout->functionRvas[static_cast<std::uint32_t>(FunctionId::CreateWeaponEntity)] =
                createWeaponRva;
        else
            AppendWarning(layout, "未能定位武器生成函数，生成武器功能将不可用。");
    }

    std::uint32_t mapSetterRva = 0;
    if (!Required(section, sectionRva, setMapLayer, &mapSetterRva)) {
        SetError(layout, "未能唯一定位地图层函数特征码；为避免错误读写，已拒绝附加。");
        return false;
    }

    const int allocOffset = static_cast<int>(
        layout->functionRvas[static_cast<std::uint32_t>(FunctionId::AllocateEntity)] - sectionRva);
    const int assignOffset = static_cast<int>(
        layout->functionRvas[static_cast<std::uint32_t>(FunctionId::AssignCharacterToThing)] -
        sectionRva);
    const int mapOffset = static_cast<int>(mapSetterRva - sectionRva);
    const int playerOffset = static_cast<int>(
        layout->functionRvas[static_cast<std::uint32_t>(FunctionId::SetCurrentPlayerThing)] -
        sectionRva);

    // ---- 实体池地址与步长：69 C0 <stride> ... 8D 98 <pool> ----
    const unsigned char poolReference[8] = {0x69, 0xc0, 0, 0, 0, 0, 0x8d, 0x98};
    int poolReferenceOffset = -1;
    int poolReferenceCount = 0;
    const int allocateEnd = std::min(static_cast<int>(section.size()), allocOffset + 0x200);
    for (int offset = allocOffset; offset <= allocateEnd - 8; ++offset) {
        bool matches = true;
        for (int i = 0; i < 8; ++i) {
            if ((i < 2 || i >= 6) && section[offset + i] != poolReference[i]) {
                matches = false;
                break;
            }
        }
        if (matches) {
            poolReferenceOffset = offset;
            ++poolReferenceCount;
        }
    }
    if (poolReferenceCount != 1 || poolReferenceOffset < 0 ||
        poolReferenceOffset + 12 > static_cast<int>(section.size()) ||
        ReadU32(section, poolReferenceOffset + 8) == 0) {
        SetError(layout, "无法从实体分配函数解析实体池地址。");
        return false;
    }
    if (!AbsoluteToRva(ReadU32(section, poolReferenceOffset + 8), imageBase, imageSize,
                       &layout->thingPoolRva)) {
        SetError(layout, "实体池地址不在游戏模块范围内。");
        return false;
    }
    layout->thingStride = ReadU32(section, poolReferenceOffset + 2);
    if (layout->thingStride == 0 || layout->thingStride > 0x10000u) {
        SetError(layout, "实体结构步长无效。");
        return false;
    }

    // ---- 任务状态：8B 2D <mission+4> ----
    int missionReferenceOffset = -1;
    {
        const unsigned char missionReference[2] = {0x8b, 0x2d};
        missionReferenceOffset = FindBytes(section, missionReference, 2,
                                           allocOffset, allocateEnd);
    }
    if (missionReferenceOffset < 0 || missionReferenceOffset + 6 > static_cast<int>(section.size())) {
        SetError(layout, "无法从实体分配函数解析任务状态地址。");
        return false;
    }
    {
        const std::uint32_t missionCurrentPlayerVa = ReadU32(section, missionReferenceOffset + 2);
        if (missionCurrentPlayerVa < imageBase + 4 ||
            !AbsoluteToRva(missionCurrentPlayerVa - 4, imageBase, imageSize,
                           &layout->missionStateRva)) {
            SetError(layout, "任务状态地址无效。");
            return false;
        }
    }

    // ---- 角色查询函数：角色绑定函数中的第一个 CALL ----
    const int assignEnd = std::min(static_cast<int>(section.size()), assignOffset + 0x40);
    int getterCallOffset = -1;
    {
        const unsigned char callOpcode = 0xE8;
        getterCallOffset = FindBytes(section, &callOpcode, 1, assignOffset, assignEnd);
    }
    if (getterCallOffset < 0 || getterCallOffset + 5 > static_cast<int>(section.size())) {
        SetError(layout, "无法从角色绑定函数解析角色数据查询函数。");
        return false;
    }
    int getterOffset = -1;
    {
        std::int32_t displacement = 0;
        std::memcpy(&displacement, section.data() + getterCallOffset + 1, sizeof(displacement));
        const std::int64_t getterRva = static_cast<std::int64_t>(sectionRva) +
                                       getterCallOffset + 5 + displacement;
        if (getterRva < sectionRva ||
            getterRva >= static_cast<std::int64_t>(sectionRva) + static_cast<std::int64_t>(section.size())) {
            SetError(layout, "角色数据查询函数地址无效。");
            return false;
        }
        getterOffset = static_cast<int>(getterRva - sectionRva);
    }
    const int getterEnd = std::min(static_cast<int>(section.size()), getterOffset + 0x40);

    // ---- 角色池指针：03 05 <pool> ----
    int characterPoolReferenceOffset = -1;
    {
        const unsigned char characterPoolReference[2] = {0x03, 0x05};
        characterPoolReferenceOffset = FindBytes(section, characterPoolReference, 2,
                                                 getterOffset, getterEnd);
    }
    if (characterPoolReferenceOffset < 0 ||
        characterPoolReferenceOffset + 6 > static_cast<int>(section.size()) ||
        !AbsoluteToRva(ReadU32(section, characterPoolReferenceOffset + 2), imageBase, imageSize,
                       &layout->characterPoolPointerRva)) {
        SetError(layout, "无法从角色数据查询函数解析角色池指针。");
        return false;
    }

    // ---- 角色步长：69 C0..C7 <stride>，必须唯一 ----
    int characterStrideOffset = -1;
    int characterStrideCount = 0;
    for (int offset = getterOffset; offset + 6 <= getterEnd; ++offset) {
        if (section[offset] == 0x69 && (section[offset + 1] & 0xf8) == 0xc0) {
            characterStrideOffset = offset;
            ++characterStrideCount;
        }
    }
    if (characterStrideCount != 1 || characterStrideOffset < 0) {
        SetError(layout, "无法从角色数据查询函数解析角色结构步长。");
        return false;
    }
    layout->characterStride = ReadU32(section, characterStrideOffset + 2);
    if (layout->characterStride == 0 || layout->characterStride > 0x10000u) {
        SetError(layout, "角色结构步长无效。");
        return false;
    }

    // ---- 实体分配游标：8B 0D <next-id> ----
    int nextThingOffset = -1;
    {
        const unsigned char nextThingReference[2] = {0x8b, 0x0d};
        nextThingOffset = FindBytes(section, nextThingReference, 2, allocOffset, allocateEnd);
    }
    if (nextThingOffset < 0 || nextThingOffset + 6 > static_cast<int>(section.size()) ||
        !AbsoluteToRva(ReadU32(section, nextThingOffset + 2), imageBase, imageSize,
                       &layout->nextThingIdRva)) {
        SetError(layout, "无法从实体分配函数解析实体分配游标。");
        return false;
    }

    // ---- 当前地图 ID：A1 <current-map> ----
    const int mapEnd = std::min(static_cast<int>(section.size()), mapOffset + 0x40);
    int currentMapOffset = -1;
    {
        const unsigned char movEax = 0xa1;
        currentMapOffset = FindBytes(section, &movEax, 1, mapOffset, mapEnd);
    }
    if (currentMapOffset < 0 || currentMapOffset + 5 > static_cast<int>(section.size()) ||
        !AbsoluteToRva(ReadU32(section, currentMapOffset + 1), imageBase, imageSize,
                       &layout->currentMapIdRva)) {
        SetError(layout, "无法从地图层函数解析当前地图 ID。");
        return false;
    }

    // ---- 地图元数据数组：81 C2 <meta-array> ----
    int mapMetaOffset = -1;
    {
        const unsigned char mapMetaReference[2] = {0x81, 0xc2};
        mapMetaOffset = FindBytes(section, mapMetaReference, 2, mapOffset, mapEnd);
    }
    if (mapMetaOffset < 0 || mapMetaOffset + 6 > static_cast<int>(section.size()) ||
        !AbsoluteToRva(ReadU32(section, mapMetaOffset + 2), imageBase, imageSize,
                       &layout->mapLayerMetaRva)) {
        SetError(layout, "无法从地图层函数解析地图元数据数组。");
        return false;
    }
    {
        const int mapStrideInstruction = mapOffset + 0x1B;
        if (mapStrideInstruction < 0 || mapStrideInstruction >= static_cast<int>(section.size())) {
            SetError(layout, "无法读取地图元数据结构步长。");
            return false;
        }
        layout->mapMetaStride = section[mapStrideInstruction];
        if (layout->mapMetaStride == 0 || layout->mapMetaStride > 0x100u) {
            SetError(layout, "地图元数据结构步长无效。");
            return false;
        }
    }

    // ---- 武器池：武器查询函数固定形态 ----
    std::uint32_t weaponAccessorRva = 0;
    if (!Required(section, sectionRva, weaponAccessor, &weaponAccessorRva)) {
        SetError(layout, "无法唯一定位武器数据查询函数。");
        return false;
    }
    {
        const int weaponAccessorOffset = static_cast<int>(weaponAccessorRva - sectionRva);
        if (weaponAccessorOffset + 38 > static_cast<int>(section.size()) ||
            !AbsoluteToRva(ReadU32(section, weaponAccessorOffset + 33), imageBase, imageSize,
                           &layout->weaponPoolRva)) {
            SetError(layout, "无法从武器数据查询函数解析武器池地址。");
            return false;
        }
        layout->weaponStride = ReadU32(section, weaponAccessorOffset + 28);
        if (layout->weaponStride == 0 || layout->weaponStride > 0x10000u) {
            SetError(layout, "武器结构步长无效。");
            return false;
        }
    }

    // ---- 玩家实体指针数组：B8 <base>，数组位于 +0xBE68 ----
    const int playerEnd = std::min(static_cast<int>(section.size()), playerOffset + 0x20);
    int playerBaseOffset = -1;
    {
        const unsigned char movEax = 0xb8;
        playerBaseOffset = FindBytes(section, &movEax, 1, playerOffset, playerEnd);
    }
    if (playerBaseOffset < 0 || playerBaseOffset + 5 > static_cast<int>(section.size())) {
        SetError(layout, "无法从玩家切换函数解析玩家实体指针数组。");
        return false;
    }
    {
        const std::uint32_t playerBaseVa = ReadU32(section, playerBaseOffset + 1);
        if (playerBaseVa > 0xffffffffu - 0xBE68u ||
            !AbsoluteToRva(playerBaseVa + 0xBE68u, imageBase, imageSize,
                           &layout->playerThingArrayRva)) {
            SetError(layout, "玩家实体指针数组地址无效。");
            return false;
        }
    }

    // ---- 当前渲染层：A1 <camera-map-layer> ----
    const int switchOffset = static_cast<int>(
        layout->functionRvas[static_cast<std::uint32_t>(FunctionId::SwitchActiveTileLayer)] -
        sectionRva);
    const int switchEnd = std::min(static_cast<int>(section.size()), switchOffset + 0x40);
    int cameraMapOffset = -1;
    {
        const unsigned char movEax = 0xa1;
        cameraMapOffset = FindBytes(section, &movEax, 1, switchOffset, switchEnd);
    }
    if (cameraMapOffset < 0 || cameraMapOffset + 5 > static_cast<int>(section.size()) ||
        !AbsoluteToRva(ReadU32(section, cameraMapOffset + 1), imageBase, imageSize,
                       &layout->cameraMapLayerRva)) {
        SetError(layout, "无法从地图切换函数解析当前渲染层变量。");
        return false;
    }

    // ---- 可选：销毁实体 / 脚本执行 ----
    {
        const Pattern freeThing = ParsePattern(
            "53 83 EC 28 8B 5C 24 30 85 DB 0F 84 ?? ?? ?? ?? C7 44 24 10 01 00 00 00");
        const Pattern freeThingBeta = ParsePattern(
            "53 83 EC 38 A1 ?? ?? ?? ?? 8B 5C 24 40 85 C0 74 ?? 8B 0D ?? ?? ?? ?? "
            "BA 89 88 88 88 89 C8 F7 E2 C1 EA 05 6B D2 3C 39 D1 0F 84 ?? ?? ?? ??");
        const Pattern script = ParsePattern(
            "83 EC 1C C7 04 24 ?? ?? ?? ?? E8 ?? ?? ?? ?? 85 C0 75 ?? 8B 44 24 20 "
            "C7 05 ?? ?? ?? ?? 00 00 00 00 89 04 24 E8 ?? ?? ?? ??");

        int stableOffset = -1;
        int betaOffset = -1;
        int stableCount = 0;
        int betaCount = 0;
        FindUnique(section, freeThing, &stableOffset, &stableCount);
        FindUnique(section, freeThingBeta, &betaOffset, &betaCount);
        if (stableCount + betaCount == 1) {
            const int offset = stableCount == 1 ? stableOffset : betaOffset;
            layout->functionRvas[static_cast<std::uint32_t>(FunctionId::FreeThing)] =
                sectionRva + static_cast<std::uint32_t>(offset);
        }

        int scriptOffset = -1;
        if (FindUnique(section, script, &scriptOffset))
            layout->functionRvas[static_cast<std::uint32_t>(FunctionId::ScriptEvaluateStringSafe)] =
                sectionRva + static_cast<std::uint32_t>(scriptOffset);

        if (layout->functionRvas[static_cast<std::uint32_t>(FunctionId::FreeThing)] == 0 ||
            layout->functionRvas[static_cast<std::uint32_t>(FunctionId::ScriptEvaluateStringSafe)] == 0) {
            AppendWarning(layout,
                          "当前构建未识别到全部可选函数（销毁实体或脚本控制台）；相关操作将不可用。");
        }
    }

    // ---- 角色池运行时指针（必须指向模块内部，否则拒绝） ----
    {
        std::uint32_t characterPoolAddress = 0;
        if (!SafeRead(static_cast<Address>(moduleBase) + layout->characterPoolPointerRva,
                      &characterPoolAddress, sizeof(characterPoolAddress)) ||
            characterPoolAddress < moduleBase ||
            static_cast<std::uint64_t>(characterPoolAddress) - moduleBase >= imageSize) {
            SetError(layout, "角色池运行时指针无效；为避免错误读写，已拒绝附加。");
            return false;
        }
        layout->characterPoolRva = characterPoolAddress - moduleBase;
    }

    layout->valid = 1;
    return true;
}

} // namespace dr2c
