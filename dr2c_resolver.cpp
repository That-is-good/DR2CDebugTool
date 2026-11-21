#include "dr2c_resolver.h"

#include <windows.h>
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
    std::string s(text);
    for (std::size_t i = 0; i < s.size();) {
        while (i < s.size() && s[i] == ' ') ++i;
        if (i >= s.size()) break;
        if (i + 1 < s.size() && s[i] == '?' && s[i + 1] == '?') {
            result.bytes.push_back(0);
            result.mask.push_back(0);
            i += 2;
        } else if (i + 1 < s.size()) {
            char pair[3] = {s[i], s[i + 1], 0};
            char *end = nullptr;
            unsigned long value = std::strtoul(pair, &end, 16);
            if (*end != '\0') {
                result.valid = false;
                return result;
            }
            result.bytes.push_back(static_cast<unsigned char>(value));
            result.mask.push_back(0xff);
            i += 2;
        } else {
            result.valid = false;
            return result;
        }
    }
    result.valid = !result.bytes.empty();
    return result;
}

bool FindUnique(const std::vector<unsigned char> &image,
                const Pattern &pattern, int *offset, int *matchCount = nullptr)
{
    if (matchCount)
        *matchCount = 0;
    if (!pattern.valid || pattern.bytes.size() > image.size())
        return false;
    int found = -1;
    int count = 0;
    for (std::size_t i = 0; i + pattern.bytes.size() <= image.size(); ++i) {
        bool match = true;
        for (std::size_t j = 0; j < pattern.bytes.size(); ++j) {
            if ((pattern.mask[j] & (image[i + j] ^ pattern.bytes[j])) != 0) {
                match = false;
                break;
            }
            if (matchCount)
                *matchCount = count;
        }
        if (match) {
            found = static_cast<int>(i);
            ++count;
        }
    }
    if (count != 1)
        return false;
    if (offset)
        *offset = found;
    return true;
}

int FindBytes(const std::vector<unsigned char> &image,
              const unsigned char *bytes, std::size_t size,
              int from, int end)
{
    if (from < 0) from = 0;
    if (end > static_cast<int>(image.size())) end = static_cast<int>(image.size());
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

bool AbsoluteToRva(std::uint32_t absolute, std::uint32_t imageBase,
                   std::uint32_t imageSize, std::uint32_t *rva)
{
    if (absolute < imageBase)
        return false;
    const std::uint64_t result = static_cast<std::uint64_t>(absolute) - imageBase;
    if (result == 0 || result >= imageSize)
        return false;
    *rva = static_cast<std::uint32_t>(result);
    return true;
}

bool Required(const std::vector<unsigned char> &section,
              std::uint32_t sectionRva, const Pattern &pattern,
              std::uint32_t *rva)
{
    int offset = -1;
    if (!FindUnique(section, pattern, &offset) || offset < 0)
        return false;
    if (static_cast<std::uint64_t>(sectionRva) + offset > UINT32_MAX)
        return false;
    *rva = sectionRva + static_cast<std::uint32_t>(offset);
    return true;
}

} // namespace

bool ResolveGameLayout(Address moduleBase, GameLayout *layout)
{
    if (!layout)
        return false;
    *layout = GameLayout{};
    if (!moduleBase)
        return false;

    IMAGE_DOS_HEADER dos{};
    std::memcpy(&dos, Ptr(moduleBase), sizeof(dos));
    if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0)
        return false;

    Address ntAddress = moduleBase + static_cast<std::uint32_t>(dos.e_lfanew);
    std::uint32_t signature = Load<std::uint32_t>(ntAddress);
    if (signature != IMAGE_NT_SIGNATURE)
        return false;

    IMAGE_FILE_HEADER fileHeader{};
    IMAGE_OPTIONAL_HEADER32 optionalHeader{};
    std::memcpy(&fileHeader, Ptr(ntAddress + sizeof(signature)), sizeof(fileHeader));
    std::memcpy(&optionalHeader,
                Ptr(ntAddress + sizeof(signature) + sizeof(fileHeader)),
                sizeof(optionalHeader));
    if (fileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        optionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        optionalHeader.SizeOfImage == 0)
        return false;

    layout->imageSize = optionalHeader.SizeOfImage;
    const std::uint32_t imageBase = optionalHeader.ImageBase;
    Address sectionTable = ntAddress + sizeof(signature) +
        sizeof(fileHeader) + fileHeader.SizeOfOptionalHeader;

    IMAGE_SECTION_HEADER textSection{};
    bool foundText = false;
    for (std::uint16_t i = 0; i < fileHeader.NumberOfSections && i < 96; ++i) {
        IMAGE_SECTION_HEADER section{};
        std::memcpy(&section, Ptr(sectionTable + i * sizeof(section)), sizeof(section));
        if (std::memcmp(section.Name, ".text", 5) == 0 &&
            (section.Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0) {
            textSection = section;
            foundText = true;
            break;
        }
    }
    if (!foundText)
        return false;

    const std::uint32_t sectionSize =
        textSection.Misc.VirtualSize > textSection.SizeOfRawData
            ? textSection.Misc.VirtualSize : textSection.SizeOfRawData;
    if (sectionSize == 0 || sectionSize > 32u * 1024u * 1024u ||
        static_cast<std::uint64_t>(textSection.VirtualAddress) + sectionSize >
            optionalHeader.SizeOfImage)
        return false;

    std::vector<unsigned char> section(sectionSize);
    std::memcpy(section.data(), Ptr(moduleBase + textSection.VirtualAddress), sectionSize);
    const std::uint32_t sectionRva = textSection.VirtualAddress;

    Pattern allocateEntity = ParsePattern(
        "55 57 56 53 BE ?? ?? ?? ?? 83 EC 3C 8B 2D ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? C7 05 ?? ?? ?? ?? 00 00 00 00");
    Pattern allocateThing = ParsePattern(
        "53 83 EC 18 C7 04 24 03 00 00 00 E8 ?? ?? ?? ?? 85 C0 89 C3 74 ?? 8B 44 24 20 88 43 03 E8 ?? ?? ?? ?? 88 43 04 83 C4 18 89 D8 5B C3");
    Pattern allocateCharacter = ParsePattern(
        "83 EC 1C C7 04 24 12 00 00 00 E8 ?? ?? ?? ?? 83 C4 1C C3");
    Pattern assignCharacter = ParsePattern(
        "56 53 83 EC 14 8B 5C 24 24 8B 74 24 20 89 1C 24 E8 ?? ?? ?? ?? 85 C0 74 ?? 0F B7 16 89 9E 48 01 00 00 66 89 50 04 83 C4 14 B8 01 00 00 00 5B 5E C3");
    Pattern setPlayer = ParsePattern(
        "8B 54 24 04 B8 ?? ?? ?? ?? 39 90 68 BE 00 00 74 ?? 83 C0 04 3D ?? ?? ?? ?? 75 ?? A1 ?? ?? ?? ?? 89 54 24 04 83 F8 04 19 C9 21 C8 89 14 85 ?? ?? ?? ?? E9 ?? ?? ?? ??");
    Pattern switchLayer = ParsePattern(
        "57 56 53 83 EC 10 A1 ?? ?? ?? ?? 8B 5C 24 20 39 D8 0F 84 ?? ?? ?? ?? 89 04 24 E8 ?? ?? ?? ??");
    Pattern setMapLayer = ParsePattern(
        "8B 54 24 04 31 C0 81 FA FE 00 00 00 77 ?? A1 ?? ?? ?? ?? 89 15 ?? ?? ?? ?? 6B D2 ?? 81 C2 ?? ?? ?? ?? 89 15 ?? ?? ?? ?? F3 C3");
    Pattern weaponAccessor = ParsePattern(
        "8B 44 24 04 C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 3D ?? ?? ?? ?? 76 ?? B8 ?? ?? ?? ?? 69 C0 ?? ?? ?? ?? 05 ?? ?? ?? ?? C3");

    Pattern *patterns[] = {&allocateEntity, &allocateThing, &allocateCharacter,
                           &assignCharacter, &setPlayer, &switchLayer};
    GameLayout::Function ids[] = {GameLayout::AllocateEntity, GameLayout::AllocateThing,
                                  GameLayout::AllocateCharacterSlot, GameLayout::AssignCharacterToThing,
                                  GameLayout::SetCurrentPlayerThing, GameLayout::SwitchActiveTileLayer};
    for (std::size_t i = 0; i < 6; ++i) {
        if (!Required(section, sectionRva, *patterns[i],
                      &layout->functionRvas[ids[i]])) {
            layout->error = "无法唯一解析核心函数；为避免错误读写已拒绝附加。";
            return false;
        }
    }

    int allocOffset = static_cast<int>(layout->functionRvas[GameLayout::AllocateEntity] - sectionRva);
    int assignOffset = static_cast<int>(layout->functionRvas[GameLayout::AssignCharacterToThing] - sectionRva);
    std::uint32_t mapSetterRva = 0;
    if (!Required(section, sectionRva, setMapLayer, &mapSetterRva)) {
        layout->error = "未能定位地图层函数特征码；已拒绝附加。";
        return false;
    }
    int mapOffset = static_cast<int>(mapSetterRva - sectionRva);

    int poolOffset = -1;
    int poolCount = 0;
    int allocEnd = allocOffset + 0x200;
    if (allocEnd > static_cast<int>(section.size())) allocEnd = static_cast<int>(section.size());
    for (int i = allocOffset; i + 12 <= allocEnd; ++i) {
        if (section[i] == 0x69 && section[i + 1] == 0xc0 &&
            section[i + 6] == 0x8d && section[i + 7] == 0x98 &&
            ReadU32(section, i + 2) != 0) {
            ++poolCount;
            poolOffset = i;
        }
    }
    if (poolCount != 1 || poolOffset < 0 || poolOffset + 12 > static_cast<int>(section.size())) {
        layout->error = "无法解析实体池地址。";
        return false;
    }
    if (!AbsoluteToRva(ReadU32(section, poolOffset + 8), imageBase,
                       layout->imageSize, &layout->thingPoolRva)) {
        layout->error = "实体池地址不在模块范围内。";
        return false;
    }
    layout->thingStride = ReadU32(section, poolOffset + 2);
    if (layout->thingStride == 0 || layout->thingStride > 0x10000) {
        layout->error = "实体结构步长无效。";
        return false;
    }

    int getterCall = FindBytes(section,
        reinterpret_cast<const unsigned char*>("\xE8"), 1,
        assignOffset, assignOffset + 0x40);
    if (getterCall < 0 || getterCall + 5 > static_cast<int>(section.size())) {
        layout->error = "无法解析角色数据查询函数。";
        return false;
    }
    std::int32_t displacement;
    std::memcpy(&displacement, section.data() + getterCall + 1, 4);
    std::int64_t getterRva = static_cast<std::int64_t>(sectionRva) +
        getterCall + 5 + displacement;
    if (getterRva < sectionRva ||
        getterRva >= static_cast<std::int64_t>(sectionRva) + section.size())
        getterRva = -1;
    if (getterRva < 0) {
        layout->error = "角色数据查询函数地址无效。";
        return false;
    }
    int getterOffset = static_cast<int>(getterRva - sectionRva);
    layout->functionRvas[GameLayout::GetCharacterData] =
        sectionRva + static_cast<std::uint32_t>(getterOffset);
    int getterEnd = getterOffset + 0x40;
    if (getterEnd > static_cast<int>(section.size())) getterEnd = static_cast<int>(section.size());

    int characterPoolOffset = -1;
    for (int i = getterOffset; i + 6 <= getterEnd; ++i) {
        if (section[i] == 0x03 && section[i + 1] == 0x05) {
            characterPoolOffset = i;
            break;
        }
    }
    if (characterPoolOffset < 0 ||
        !AbsoluteToRva(ReadU32(section, characterPoolOffset + 2), imageBase,
                       layout->imageSize, &layout->characterPoolPointerRva)) {
        layout->error = "无法解析角色池指针。";
        return false;
    }
    int characterStrideOffset = -1;
    int characterStrideCount = 0;
    for (int i = getterOffset; i + 6 <= getterEnd; ++i) {
        if (section[i] == 0x69 && (section[i + 1] & 0xf8) == 0xc0) {
            characterStrideOffset = i;
            ++characterStrideCount;
        }
    }
    if (characterStrideCount != 1) {
        layout->error = "无法解析角色结构步长。";
        return false;
    }
    layout->characterStride = ReadU32(section, characterStrideOffset + 2);

    std::uint32_t weaponRva = 0;
    if (!Required(section, sectionRva, weaponAccessor, &weaponRva)) {
        layout->error = "无法定位武器数据查询函数。";
        return false;
    }
    int weaponOffset = static_cast<int>(weaponRva - sectionRva);
    if (weaponOffset + 38 > static_cast<int>(section.size()) ||
        !AbsoluteToRva(ReadU32(section, weaponOffset + 33), imageBase,
                       layout->imageSize, &layout->weaponPoolRva)) {
        layout->error = "无法解析武器池地址。";
        return false;
    }
    layout->weaponStride = ReadU32(section, weaponOffset + 28);
    if (layout->weaponStride == 0 || layout->weaponStride > 0x10000) {
        layout->error = "武器结构步长无效。";
        return false;
    }

    Pattern freeStable = ParsePattern(
        "53 83 EC 28 8B 5C 24 30 85 DB 0F 84 ?? ?? ?? ?? C7 44 24 10 01 00 00 00");
    Pattern freeBeta = ParsePattern(
        "53 83 EC 38 A1 ?? ?? ?? ?? 8B 5C 24 40 85 C0 74 ?? 8B 0D ?? ?? ?? ?? "
        "BA 89 88 88 88 89 C8 F7 E2 C1 EA 05 6B D2 3C 39 D1 0F 84 ?? ?? ?? ??");
    int stableOffset = -1, betaOffset = -1;
    int stableCount = 0, betaCount = 0;
    FindUnique(section, freeStable, &stableOffset, &stableCount);
    FindUnique(section, freeBeta, &betaOffset, &betaCount);
    if (stableCount + betaCount == 1) {
        layout->functionRvas[GameLayout::FreeThing] =
            sectionRva + (stableCount == 1
                ? static_cast<std::uint32_t>(stableOffset)
                : static_cast<std::uint32_t>(betaOffset));
    }

    int mapEnd = mapOffset + 0x40;
    if (mapEnd > static_cast<int>(section.size())) mapEnd = static_cast<int>(section.size());
    int currentMapOffset = -1;
    for (int i = mapOffset; i + 5 <= mapEnd; ++i) {
        if (section[i] == 0xa1) {
            currentMapOffset = i;
            break;
        }
    }
    if (currentMapOffset < 0 ||
        !AbsoluteToRva(ReadU32(section, currentMapOffset + 1), imageBase,
                       layout->imageSize, &layout->currentMapIdRva)) {
        layout->error = "无法解析当前地图 ID 地址。";
        return false;
    }

    std::uint32_t characterPoolAddress = Load<std::uint32_t>(
        moduleBase + layout->characterPoolPointerRva);
    if (characterPoolAddress < moduleBase ||
        static_cast<std::uint64_t>(characterPoolAddress) - moduleBase >= layout->imageSize) {
        layout->error = "角色池运行时指针无效。";
        return false;
    }
    layout->characterPoolRva =
        static_cast<std::uint32_t>(characterPoolAddress - moduleBase);

    layout->valid = true;
    layout->error = nullptr;
    return true;
}

} // namespace dr2c
