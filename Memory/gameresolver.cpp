#include "gameresolver.h"

#include <QtEndian>
#include <QtGlobal>
#include <QList>
#include <QStringList>
#include <cstdint>
#include <cstring>

namespace {

struct Pattern {
    QByteArray bytes;
    QByteArray mask;
    bool valid = false;

    explicit Pattern(const char *value)
    {
        const QList<QByteArray> tokens = QByteArray(value).simplified().split(' ');
        for (const QByteArray &token : tokens) {
            if (token == "??") {
                bytes.append('\0');
                mask.append('\0');
                continue;
            }
            bool ok = false;
            const uint byte = token.toUInt(&ok, 16);
            if (!ok || token.size() != 2 || byte > 0xff) {
                bytes.clear();
                mask.clear();
                return;
            }
            bytes.append(static_cast<char>(byte));
            mask.append(static_cast<char>(0xff));
        }
        valid = !bytes.isEmpty() && bytes.size() == mask.size();
    }
};

quint32 readU32(const QByteArray &bytes, int offset)
{
    quint32 value = 0;
    if (offset >= 0 && offset <= bytes.size() - static_cast<int>(sizeof(value)))
        value = qFromLittleEndian<quint32>(
            reinterpret_cast<const uchar *>(bytes.constData() + offset));
    return value;
}

bool findInRange(const QByteArray &image, const QByteArray &pattern,
                 int from, int end, int *offset)
{
    if (pattern.isEmpty() || from < 0 || end > image.size() || from > end ||
        pattern.size() > end - from)
        return false;
    const int found = image.indexOf(pattern, from);
    if (found < 0 || found + pattern.size() > end)
        return false;
    if (offset)
        *offset = found;
    return true;
}

quint32 toRva(quint32 va, quint64 imageBase, quint32 imageSize)
{
    if (va < imageBase)
        return 0;
    const quint64 rva = static_cast<quint64>(va) - imageBase;
    return rva < imageSize ? static_cast<quint32>(rva) : 0;
}

bool requiredPattern(const QByteArray &section, quint32 sectionRva,
                     const Pattern &pattern, quint32 *rva)
{
    int offset = -1;
    int count = 0;
    if (!pattern.valid ||
        !GameResolver::findUnique(section, pattern.bytes, pattern.mask, &offset, &count) ||
        offset < 0 || static_cast<quint64>(sectionRva) + offset > UINT32_MAX)
        return false;
    *rva = sectionRva + static_cast<quint32>(offset);
    return true;
}

bool absoluteToRva(quint32 absolute, quint64 imageBase, quint32 imageSize, quint32 *rva)
{
    const quint32 result = toRva(absolute, imageBase, imageSize);
    if (!result)
        return false;
    *rva = result;
    return true;
}

} // namespace

bool GameResolver::findUnique(const QByteArray &image,
                              const QByteArray &bytes,
                              const QByteArray &mask,
                              int *matchOffset,
                              int *matchCount)
{
    if (bytes.isEmpty() || bytes.size() != mask.size() ||
        bytes.size() > image.size())
        return false;

    int count = 0;
    int firstMatch = -1;
    const char *source = image.constData();
    const char *pattern = bytes.constData();
    const char *patternMask = mask.constData();
    const int lastOffset = image.size() - bytes.size();
    for (int offset = 0; offset <= lastOffset; ++offset) {
        bool matches = true;
        for (int i = 0; i < bytes.size(); ++i) {
            if (patternMask[i] != 0 && source[offset + i] != pattern[i]) {
                matches = false;
                break;
            }
        }
        if (matches) {
            if (firstMatch < 0)
                firstMatch = offset;
            ++count;
        }
    }

    if (matchCount)
        *matchCount = count;
    if (matchOffset)
        *matchOffset = count == 1 ? firstMatch : -1;
    return count == 1;
}

bool GameResolver::resolve(const QByteArray &section,
                           quint32 sectionRva,
                           quint64 imageBase,
                           quint32 imageSize,
                           Layout *layout)
{
    if (!layout)
        return false;
    *layout = Layout{};
    layout->imageBase = imageBase;
    layout->imageSize = imageSize;

    const Pattern allocateEntity(
        "55 57 56 53 BE ?? ?? ?? ?? 83 EC 3C 8B 2D ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? "
        "C7 05 ?? ?? ?? ?? 00 00 00 00");
    const Pattern allocateThing(
        "53 83 EC 18 C7 04 24 03 00 00 00 E8 ?? ?? ?? ?? 85 C0 89 C3 74 ?? "
        "8B 44 24 20 88 43 03 E8 ?? ?? ?? ?? 88 43 04 83 C4 18 89 D8 5B C3");
    const Pattern allocateCharacter(
        "83 EC 1C C7 04 24 12 00 00 00 E8 ?? ?? ?? ?? 83 C4 1C C3");
    const Pattern assignCharacter(
        "56 53 83 EC 14 8B 5C 24 24 8B 74 24 20 89 1C 24 E8 ?? ?? ?? ?? "
        "85 C0 74 ?? 0F B7 16 89 9E 48 01 00 00 66 89 50 04 83 C4 14 "
        "B8 01 00 00 00 5B 5E C3");
    const Pattern setPlayer(
        "8B 54 24 04 B8 ?? ?? ?? ?? 39 90 68 BE 00 00 74 ?? 83 C0 04 "
        "3D ?? ?? ?? ?? 75 ?? A1 ?? ?? ?? ?? 89 54 24 04 83 F8 04 19 C9 "
        "21 C8 89 14 85 ?? ?? ?? ?? E9 ?? ?? ?? ??");
    const Pattern switchLayer(
        "57 56 53 83 EC 10 A1 ?? ?? ?? ?? 8B 5C 24 20 39 D8 0F 84 ?? ?? ?? ?? "
        "89 04 24 E8 ?? ?? ?? ??");
    const Pattern setMapLayer(
        "8B 54 24 04 31 C0 81 FA FE 00 00 00 77 ?? A1 ?? ?? ?? ?? "
        "89 15 ?? ?? ?? ?? 6B D2 ?? 81 C2 ?? ?? ?? ?? 89 15 ?? ?? ?? ?? F3 C3");
    const Pattern weaponAccessor(
        "8B 44 24 04 C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 3D ?? ?? ?? ?? 76 ?? "
        "B8 ?? ?? ?? ?? 69 C0 ?? ?? ?? ?? 05 ?? ?? ?? ?? C3");
    QStringList unresolvedFunctions;
    auto function = [&](Function id, const Pattern &pattern, const QString &name) {
        quint32 rva = 0;
        if (!requiredPattern(section, sectionRva, pattern, &rva)) {
            unresolvedFunctions.append(name);
            return false;
        }
        layout->functionRvas[static_cast<size_t>(id)] = rva;
        return true;
    };

    function(Function::AllocateEntity, allocateEntity, QStringLiteral("AllocateEntity"));
    function(Function::AllocateThing, allocateThing, QStringLiteral("AllocateThing"));
    function(Function::AllocateCharacterSlot, allocateCharacter,
             QStringLiteral("AllocateCharacterSlot"));
    function(Function::AssignCharacterToThing, assignCharacter,
             QStringLiteral("AssignCharacterToThing"));
    function(Function::SetCurrentPlayerThing, setPlayer,
             QStringLiteral("SetCurrentPlayerThing"));
    function(Function::SwitchActiveTileLayer, switchLayer,
             QStringLiteral("SwitchActiveTileLayer"));
    if (!unresolvedFunctions.isEmpty()) {
        layout->error = QStringLiteral("无法唯一解析核心函数: %1；为避免错误读写，已拒绝附加。")
                            .arg(unresolvedFunctions.join(QStringLiteral(", ")));
        return false;
    }

    quint32 mapSetterRva = 0;
    if (!requiredPattern(section, sectionRva, setMapLayer, &mapSetterRva)) {
        layout->error = QStringLiteral("未能唯一定位地图层函数特征码；为避免错误读写，已拒绝附加。");
        return false;
    }

    int allocOffset = static_cast<int>(layout->functionRvas[static_cast<size_t>(Function::AllocateEntity)] - sectionRva);
    const int assignOffset = static_cast<int>(
        layout->functionRvas[static_cast<size_t>(Function::AssignCharacterToThing)] - sectionRva);
    int mapOffset = static_cast<int>(mapSetterRva - sectionRva);
    int playerOffset = static_cast<int>(layout->functionRvas[static_cast<size_t>(Function::SetCurrentPlayerThing)] - sectionRva);

    const QByteArray poolReference = QByteArray::fromHex("69c0000000008d98");
    QByteArray poolReferenceMask(poolReference.size(), '\0');
    poolReferenceMask.replace(0, 2, QByteArray(2, static_cast<char>(0xff)));
    poolReferenceMask.replace(6, 2, QByteArray(2, static_cast<char>(0xff)));
    int poolReferenceOffset = -1;
    const int allocateEnd = qMin(section.size(), allocOffset + 0x200);
    int poolReferenceCount = 0;
    for (int offset = allocOffset; offset <= allocateEnd - poolReference.size(); ++offset) {
        bool matches = true;
        for (int i = 0; i < poolReference.size(); ++i) {
            if (poolReferenceMask[i] != 0 &&
                section[offset + i] != poolReference[i]) {
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
        poolReferenceOffset + 12 > section.size() ||
        readU32(section, poolReferenceOffset + 8) == 0) {
        layout->error = QStringLiteral("无法从实体分配函数解析实体池地址。");
        return false;
    }
    const quint32 thingPoolVa = readU32(section, poolReferenceOffset + 8);
    if (!absoluteToRva(thingPoolVa, imageBase, imageSize, &layout->thingPoolRva)) {
        layout->error = QStringLiteral("实体池地址不在游戏模块范围内。");
        return false;
    }
    layout->thingStride = readU32(section, poolReferenceOffset + 2);
    if (layout->thingStride == 0 || layout->thingStride > 0x10000) {
        layout->error = QStringLiteral("实体结构步长无效。");
        return false;
    }

    const QByteArray missionReference = QByteArray::fromHex("8b2d");
    int missionReferenceOffset = -1;
    if (!findInRange(section, missionReference, allocOffset, allocateEnd, &missionReferenceOffset) ||
        missionReferenceOffset + 6 > section.size()) {
        layout->error = QStringLiteral("无法从实体分配函数解析任务状态地址。");
        return false;
    }
    quint32 missionCurrentPlayerVa = readU32(section, missionReferenceOffset + 2);
    if (missionCurrentPlayerVa < imageBase + 4 ||
        !absoluteToRva(missionCurrentPlayerVa - 4, imageBase, imageSize, &layout->missionStateRva)) {
        layout->error = QStringLiteral("任务状态地址无效。");
        return false;
    }
    const int assignEnd = qMin(section.size(), assignOffset + 0x40);
    const QByteArray characterGetterCall = QByteArray::fromHex("e8");
    int getterCallOffset = -1;
    if (!findInRange(section, characterGetterCall, assignOffset, assignEnd, &getterCallOffset) ||
        getterCallOffset + 5 > section.size()) {
        layout->error = QStringLiteral("无法从角色绑定函数解析角色数据查询函数。");
        return false;
    }
    const qint32 getterDisplacement = qFromLittleEndian<qint32>(
        reinterpret_cast<const uchar *>(section.constData() + getterCallOffset + 1));
    const qint64 getterRva64 = static_cast<qint64>(sectionRva) + getterCallOffset + 5 +
        getterDisplacement;
    if (getterRva64 < sectionRva ||
        getterRva64 >= static_cast<qint64>(sectionRva) + section.size()) {
        layout->error = QStringLiteral("角色数据查询函数地址无效。");
        return false;
    }
    const int getterOffset = static_cast<int>(getterRva64 - sectionRva);
    const int getterEnd = qMin(section.size(), getterOffset + 0x40);
    const QByteArray characterPoolReference = QByteArray::fromHex("0305");
    int characterPoolReferenceOffset = -1;
    if (!findInRange(section, characterPoolReference, getterOffset, getterEnd,
                     &characterPoolReferenceOffset) ||
        characterPoolReferenceOffset + 6 > section.size() ||
        !absoluteToRva(readU32(section, characterPoolReferenceOffset + 2),
                       imageBase, imageSize, &layout->characterPoolPointerRva)) {
        layout->error = QStringLiteral("无法从角色数据查询函数解析角色池指针。");
        return false;
    }
    const QByteArray characterStrideReference = QByteArray::fromHex("69c000000000");
    QByteArray characterStrideMask(characterStrideReference.size(), '\0');
    characterStrideMask[0] = static_cast<char>(0xff);
    characterStrideMask[1] = static_cast<char>(0xf8);
    int characterStrideOffset = -1;
    int characterStrideCount = 0;
    for (int offset = getterOffset;
         offset <= getterEnd - characterStrideReference.size(); ++offset) {
        bool matches = true;
        for (int i = 0; i < characterStrideReference.size(); ++i) {
            if ((static_cast<quint8>(characterStrideMask[i]) &
                 static_cast<quint8>(section[offset + i] ^ characterStrideReference[i])) != 0) {
                matches = false;
                break;
            }
        }
        if (matches) {
            characterStrideOffset = offset;
            ++characterStrideCount;
        }
    }
    if (characterStrideCount != 1 || characterStrideOffset < 0 ||
        characterStrideOffset + 6 > section.size()) {
        layout->error = QStringLiteral("无法从角色数据查询函数解析角色结构步长。");
        return false;
    }
    layout->characterStride = readU32(section, characterStrideOffset + 2);
    if (layout->characterStride == 0 || layout->characterStride > 0x10000) {
        layout->error = QStringLiteral("角色结构步长无效。");
        return false;
    }

    const QByteArray nextThingReference = QByteArray::fromHex("8b0d");
    int nextThingOffset = -1;
    if (!findInRange(section, nextThingReference, allocOffset, allocateEnd, &nextThingOffset) ||
        nextThingOffset + 6 > section.size() ||
        !absoluteToRva(readU32(section, nextThingOffset + 2), imageBase, imageSize,
                       &layout->nextThingIdRva)) {
        layout->error = QStringLiteral("无法从实体分配函数解析实体分配游标。");
        return false;
    }

    const QByteArray currentMapReference = QByteArray::fromHex("a1");
    int currentMapOffset = -1;
    const int mapEnd = qMin(section.size(), mapOffset + 0x40);
    if (!findInRange(section, currentMapReference, mapOffset, mapEnd, &currentMapOffset) ||
        currentMapOffset + 5 > section.size() ||
        !absoluteToRva(readU32(section, currentMapOffset + 1), imageBase, imageSize,
                       &layout->currentMapIdRva)) {
        layout->error = QStringLiteral("无法从地图层函数解析当前地图 ID。");
        return false;
    }

    const QByteArray mapMetaReference = QByteArray::fromHex("81c2");
    int mapMetaOffset = -1;
    if (!findInRange(section, mapMetaReference, mapOffset, mapEnd, &mapMetaOffset) ||
        mapMetaOffset + 6 > section.size() ||
        !absoluteToRva(readU32(section, mapMetaOffset + 2), imageBase, imageSize,
                       &layout->mapLayerMetaRva)) {
        layout->error = QStringLiteral("无法从地图层函数解析地图元数据数组。");
        return false;
    }
    const int mapStrideInstruction = mapSetterRva - sectionRva + 0x1B;
    if (mapStrideInstruction < 0 || mapStrideInstruction >= section.size()) {
        layout->error = QStringLiteral("无法读取地图元数据结构步长。");
        return false;
    }
    layout->mapMetaStride = static_cast<quint8>(section[mapStrideInstruction]);
    if (layout->mapMetaStride == 0 || layout->mapMetaStride > 0x100) {
        layout->error = QStringLiteral("地图元数据结构步长无效。");
        return false;
    }

    quint32 weaponAccessorRva = 0;
    if (!requiredPattern(section, sectionRva, weaponAccessor, &weaponAccessorRva)) {
        layout->error = QStringLiteral("无法唯一定位武器数据查询函数。");
        return false;
    }
    const int weaponAccessorOffset = static_cast<int>(weaponAccessorRva - sectionRva);
    if (weaponAccessorOffset + 38 > section.size() ||
        !absoluteToRva(readU32(section, weaponAccessorOffset + 33),
                       imageBase, imageSize, &layout->weaponPoolRva)) {
        layout->error = QStringLiteral("无法从武器数据查询函数解析武器池地址。");
        return false;
    }
    layout->weaponStride = readU32(section, weaponAccessorOffset + 28);
    if (layout->weaponStride == 0 || layout->weaponStride > 0x10000) {
        layout->error = QStringLiteral("武器结构步长无效。");
        return false;
    }

    const QByteArray playerArrayBase = QByteArray::fromHex("b8");
    int playerBaseOffset = -1;
    const int playerEnd = qMin(section.size(), playerOffset + 0x20);
    if (!findInRange(section, playerArrayBase, playerOffset, playerEnd, &playerBaseOffset) ||
        playerBaseOffset + 5 > section.size()) {
        layout->error = QStringLiteral("无法从玩家切换函数解析玩家实体指针数组。");
        return false;
    }
    const quint32 playerBaseVa = readU32(section, playerBaseOffset + 1);
    if (playerBaseVa > 0xffffffffu - 0xBE68 ||
        !absoluteToRva(playerBaseVa + 0xBE68, imageBase, imageSize,
                       &layout->playerThingArrayRva)) {
        layout->error = QStringLiteral("玩家实体指针数组地址无效。");
        return false;
    }

    const QByteArray cameraMapReference = QByteArray::fromHex("a1");
    int cameraMapOffset = -1;
    const int switchOffset = static_cast<int>(
        layout->functionRvas[static_cast<size_t>(Function::SwitchActiveTileLayer)] - sectionRva);
    const int switchEnd = qMin(section.size(), switchOffset + 0x40);
    if (!findInRange(section, cameraMapReference, switchOffset, switchEnd, &cameraMapOffset) ||
        cameraMapOffset + 5 > section.size() ||
        !absoluteToRva(readU32(section, cameraMapOffset + 1), imageBase, imageSize,
                       &layout->cameraMapLayerRva)) {
        layout->error = QStringLiteral("无法从地图切换函数解析当前渲染层变量。");
        return false;
    }

    Pattern freeThing(
        "53 83 EC 28 8B 5C 24 30 85 DB 0F 84 ?? ?? ?? ?? C7 44 24 10 01 00 00 00");
    Pattern freeThingBeta(
        "53 83 EC 38 A1 ?? ?? ?? ?? 8B 5C 24 40 85 C0 74 ?? 8B 0D ?? ?? ?? ?? "
        "BA 89 88 88 88 89 C8 F7 E2 C1 EA 05 6B D2 3C 39 D1 0F 84 ?? ?? ?? ??");
    Pattern script(
        "83 EC 1C C7 04 24 ?? ?? ?? ?? E8 ?? ?? ?? ?? 85 C0 75 ?? 8B 44 24 20 "
        "C7 05 ?? ?? ?? ?? 00 00 00 00 89 04 24 E8 ?? ?? ?? ??");
    int optionalOffset = -1;
    int stableFreeThingCount = 0;
    int betaFreeThingCount = 0;
    int stableFreeThingOffset = -1;
    int betaFreeThingOffset = -1;
    findUnique(section, freeThing.bytes, freeThing.mask,
               &stableFreeThingOffset, &stableFreeThingCount);
    findUnique(section, freeThingBeta.bytes, freeThingBeta.mask,
               &betaFreeThingOffset, &betaFreeThingCount);
    if (stableFreeThingCount + betaFreeThingCount == 1) {
        optionalOffset = stableFreeThingCount == 1
            ? stableFreeThingOffset : betaFreeThingOffset;
        layout->functionRvas[static_cast<size_t>(Function::FreeThing)] = sectionRva + optionalOffset;
    }
    optionalOffset = -1;
    if (findUnique(section, script.bytes, script.mask, &optionalOffset))
        layout->functionRvas[static_cast<size_t>(Function::ScriptEvaluateStringSafe)] =
            sectionRva + optionalOffset;

    if (!layout->functionRvas[static_cast<size_t>(Function::FreeThing)] ||
        !layout->functionRvas[static_cast<size_t>(Function::ScriptEvaluateStringSafe)]) {
        layout->warning = QStringLiteral("当前构建未识别到全部可选函数（销毁实体或脚本控制台）；相关操作将不可用。");
    }
    layout->valid = true;
    return true;
}
