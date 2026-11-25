#include "spriteatlas.h"

#include "../Memory/memorymanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>
#include <QRect>
#include <QSet>
#include <QStringList>

#include <algorithm>

namespace {

// 运行时精灵表：总数与表项基址（RVA）
constexpr quint32 kSpriteCountRva = 0x227FAC;
constexpr quint32 kSpriteTableRva = 0x227FC0;
constexpr int kSpriteEntrySize = 0x20;

// 表里一段连续 id，单元格尺寸相同，对应一张源 PNG 图集
struct SpriteRange {
    quint32 firstId = 0;
    quint32 lastId = 0;
    int cellW = 0;
    int cellH = 0;
};

struct PngFile {
    QString path;
    QString rel;
    int width = 0;
    int height = 0;
};

quint16 readU16(const QByteArray &buffer, int offset)
{
    const auto *p = reinterpret_cast<const quint8 *>(buffer.constData()) + offset;
    return static_cast<quint16>(p[0] | (p[1] << 8));
}

// 把 PNG 按单元格尺寸切分，算出每帧非透明像素的包围盒（相对单元格左上角）。
// 游戏注册帧时会统计同一包围盒并把“位置 + 尺寸”写进精灵表，因此这是最可靠的比对依据。
// 空帧返回空矩形。
QVector<QRect> computeInkRects(const QString &path, int cellW, int cellH)
{
    QVector<QRect> result;
    if (cellW <= 0 || cellH <= 0)
        return result;
    QImage image(path);
    if (image.isNull())
        return result;
    if (image.format() != QImage::Format_ARGB32)
        image = image.convertToFormat(QImage::Format_ARGB32);

    const int columns = image.width() / cellW;
    const int rows = image.height() / cellH;
    if (columns <= 0 || rows <= 0)
        return result;

    result.reserve(columns * rows);
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < columns; ++col) {
            const int x0 = col * cellW;
            const int y0 = row * cellH;
            int minX = cellW;
            int minY = cellH;
            int maxX = -1;
            int maxY = -1;
            for (int y = 0; y < cellH; ++y) {
                const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y0 + y)) + x0;
                for (int x = 0; x < cellW; ++x) {
                    if (qAlpha(line[x]) == 0)
                        continue;
                    if (x < minX) minX = x;
                    if (x > maxX) maxX = x;
                    if (y < minY) minY = y;
                    if (y > maxY) maxY = y;
                }
            }
            result.append(maxX < minX ? QRect()
                                      : QRect(minX, minY, maxX - minX + 1, maxY - minY + 1));
        }
    }
    return result;
}

// 比较 PNG 若干帧的包围盒与精灵表对应表项，返回一致率百分比。
// 表项 w/h 是裁剪后的尺寸；空帧在表里固定写成 2x2，两种情况都允许 1 像素误差。
// positionHits 额外返回“位置也对上”的帧数，用于同分时挑更可信的候选。
int scoreFrames(const QVector<QRect> &ink, const QByteArray &table, quint32 startId,
                quint32 lastId, int *positionHits = nullptr)
{
    const int total = static_cast<int>(ink.size());
    if (total <= 0 || table.isEmpty())
        return -1;
    const int step = qMax(1, total / 24);
    int checked = 0;
    int sizeHits = 0;
    int positionHit = 0;
    for (int frame = 0; frame < total; frame += step) {
        const quint32 id = startId + static_cast<quint32>(frame);
        if (id > lastId)
            break;
        const int entry = static_cast<int>(id) * kSpriteEntrySize;
        if (entry + kSpriteEntrySize > table.size())
            break;
        const int expectedW = readU16(table, entry + 0x04);
        const int expectedH = readU16(table, entry + 0x06);
        const int expectedX = readU16(table, entry + 0x08);
        const int expectedY = readU16(table, entry + 0x0A);
        const QRect &box = ink.at(frame);
        ++checked;
        if (expectedW <= 2 && expectedH <= 2) {
            // 空帧：表项固定 2x2，PNG 里也应是空格子
            if (box.isEmpty() || (box.width() <= 2 && box.height() <= 2)) {
                ++sizeHits;
                ++positionHit;
            }
            continue;
        }
        if (!box.isEmpty() && qAbs(box.width() - expectedW) <= 1
            && qAbs(box.height() - expectedH) <= 1) {
            ++sizeHits;
            if (qAbs(box.x() - expectedX) <= 1 && qAbs(box.y() - expectedY) <= 1)
                ++positionHit;
        }
    }
    if (positionHits)
        *positionHits = positionHit;
    return checked > 0 ? sizeHits * 100 / checked : -1;
}

// 表项与某一帧的包围盒是否一致（尺寸允许 1 像素误差；空帧在表里固定写 2x2）
bool inkMatchesEntry(const QRect &box, const QByteArray &table, quint32 id)
{
    const int entry = static_cast<int>(id) * kSpriteEntrySize;
    if (entry + kSpriteEntrySize > table.size())
        return false;
    const int expectedW = readU16(table, entry + 0x04);
    const int expectedH = readU16(table, entry + 0x06);
    if (expectedW <= 2 && expectedH <= 2)
        return box.isEmpty() || (box.width() <= 2 && box.height() <= 2);
    return !box.isEmpty() && qAbs(box.width() - expectedW) <= 1
           && qAbs(box.height() - expectedH) <= 1;
}

// 静态兜底表：文件路径与默认单帧尺寸。
// baseRva 是“该图集首帧全局编号”变量的默认 RVA；附加后会被运行时扫描结果覆盖。
struct SheetSpec {
    const char *title;
    const char *file;
    int cellW;
    int cellH;
    quint32 baseRva;
};

const SheetSpec kSheetSpecs[] = {
    {QT_TRANSLATE_NOOP("SpriteAtlas", "武器 8x32"),   "gfx/items/dr2c_weapons.png",             8,  32, 0x46C8A4u},
    {QT_TRANSLATE_NOOP("SpriteAtlas", "武器 16x32"),  "gfx/items/dr2c_weapons_16x32px.png",     16, 32, 0x46C8A0u},
    {QT_TRANSLATE_NOOP("SpriteAtlas", "拾取物 16"),   "gfx/items/dr2c_loot16.png",              16, 16, 0x46C89Cu},
    {QT_TRANSLATE_NOOP("SpriteAtlas", "室内物件 16"), "gfx/items/dr2c_cityfurniture_16px.png",  16, 16, 0x46C908u},
    {QT_TRANSLATE_NOOP("SpriteAtlas", "家具 32"),     "gfx/items/dr2c_housefurniture_32px.png", 32, 32, 0x46C8E8u},
    {QT_TRANSLATE_NOOP("SpriteAtlas", "特殊 16"),     "gfx/items/special16.png",                16, 16, 0x46C8E4u},
    {QT_TRANSLATE_NOOP("SpriteAtlas", "特殊 32"),     "gfx/items/special32.png",                32, 32, 0x46C8E0u},
    {QT_TRANSLATE_NOOP("SpriteAtlas", "特殊 48"),     "gfx/items/special48.png",                48, 48, 0x46C8DCu},
    {QT_TRANSLATE_NOOP("SpriteAtlas", "特殊 64"),     "gfx/items/special64.png",                64, 64, 0x46C8D8u},
    {QT_TRANSLATE_NOOP("SpriteAtlas", "特殊 96"),     "gfx/items/special96.png",                96, 96, 0x46C8D4u},
    {QT_TRANSLATE_NOOP("SpriteAtlas", "车辆"),        "gfx/cars/dr2c_cars110x96.png",           110, 96, 0x46C898u},
    {QT_TRANSLATE_NOOP("SpriteAtlas", "车辆残骸"),    "gfx/cars/car-wrecks124x96.png",          124, 96, 0x46C894u},
};

struct ScannedSheet {
    QString file;
    int cellW = 0;
    int cellH = 0;
    quint32 baseRva = 0;
};

quint32 readU32(const QByteArray &buffer, int offset)
{
    const auto *p = reinterpret_cast<const quint8 *>(buffer.constData()) + offset;
    return static_cast<quint32>(p[0])
           | (static_cast<quint32>(p[1]) << 8)
           | (static_cast<quint32>(p[2]) << 16)
           | (static_cast<quint32>(p[3]) << 24);
}

// 扫描游戏启动时的图集注册代码，还原“文件 → 单帧宽高 → base 全局 RVA”。
// 代码形态（GCC x86）：
//   call <GetSpriteCount>
//   mov  dword [esp], <png 路径>
//   mov  [<base 全局>], eax
//   call <LoadImage>
//   ...  mov dword [esp+0Ch], <高>; mov dword [esp+8], <宽> ...
//   call <RegisterSheet>
QVector<ScannedSheet> scanSheetTable(MemoryManager *mem, const QByteArray &code)
{
    QVector<ScannedSheet> result;
    const quint64 imageBase = mem->moduleBaseAddress();
    const auto *p = reinterpret_cast<const quint8 *>(code.constData());
    const int size = static_cast<int>(code.size());

    for (int i = 0; i + 20 <= size; ++i) {
        if (p[i] != 0xE8)
            continue;
        if (!(p[i + 5] == 0xC7 && p[i + 6] == 0x04 && p[i + 7] == 0x24))
            continue;

        const quint32 pathVa = readU32(code, i + 8);
        if (pathVa < imageBase || pathVa - imageBase >= mem->gameLayout().imageSize)
            continue;
        const QString path = mem->readString(pathVa, 128);
        if (!path.endsWith(QLatin1String(".png"), Qt::CaseInsensitive))
            continue;

        ScannedSheet sheet;
        sheet.file = path;

        // base 全局：紧跟在路径写入后的 A3 imm32
        for (int k = i + 12; k < qMin(i + 40, size - 5); ++k) {
            if (p[k] == 0xA3) {
                const quint32 baseVa = readU32(code, k + 1);
                if (baseVa >= imageBase)
                    sheet.baseRva = static_cast<quint32>(baseVa - imageBase);
                break;
            }
        }
        if (sheet.baseRva == 0 || sheet.baseRva >= mem->gameLayout().imageSize)
            continue;

        // 单帧尺寸：C7 44 24 0C <高> / C7 44 24 08 <宽>
        // 各图集的注册代码在 .text 中首尾相接（相邻仅几十字节），扫描必须限制到
        // “下一次注册”之前，否则会把下一张图集的单帧尺寸当成本图集的。
        int nextReg = qMin(i + 160, size);
        for (int j = i + 12; j + 20 <= size; ++j) {
            if (p[j] != 0xE8 || p[j + 5] != 0xC7 || p[j + 6] != 0x04 || p[j + 7] != 0x24)
                continue;
            const quint32 nextPathVa = readU32(code, j + 8);
            if (nextPathVa < imageBase || nextPathVa - imageBase >= mem->gameLayout().imageSize)
                continue;
            const QString nextPath = mem->readString(nextPathVa, 128);
            if (nextPath.endsWith(QLatin1String(".png"), Qt::CaseInsensitive)) {
                nextReg = j;
                break;
            }
        }
        for (int k = i + 12; k + 8 <= nextReg; ++k) {
            if (p[k] == 0xC7 && p[k + 1] == 0x44 && p[k + 2] == 0x24) {
                if (p[k + 3] == 0x0C && sheet.cellH == 0)
                    sheet.cellH = static_cast<int>(readU32(code, k + 4));
                else if (p[k + 3] == 0x08 && sheet.cellW == 0)
                    sheet.cellW = static_cast<int>(readU32(code, k + 4));
            }
        }
        if (sheet.cellW <= 0 || sheet.cellW > 512 || sheet.cellH <= 0 || sheet.cellH > 512)
            continue;

        result.append(sheet);
    }
    return result;
}

} // namespace

SpriteAtlas::SpriteAtlas(MemoryManager *memMgr)
    : m_memMgr(memMgr)
{
}

bool SpriteAtlas::buildRuntimeSheets()
{
    const quint64 base = m_memMgr ? m_memMgr->moduleBaseAddress() : 0;
    if (!base || m_gameDir.isEmpty())
        return false;

    quint32 spriteCount = 0;
    if (!m_memMgr->read<quint32>(base + kSpriteCountRva, spriteCount)
        || spriteCount < 2 || spriteCount > 200000)
        return false;

    // 整张精灵表分块读入（每项 32 字节，约 400KB）
    const int tableBytes = static_cast<int>(spriteCount) * kSpriteEntrySize;
    QByteArray table;
    table.reserve(tableBytes);
    constexpr int kChunk = 0x10000;
    while (table.size() < tableBytes) {
        const int take = qMin(kChunk, tableBytes - table.size());
        QByteArray part(take, Qt::Uninitialized);
        if (!m_memMgr->readMemory(base + kSpriteTableRva + table.size(), part.data(), take))
            break;
        table.append(part);
    }
    if (table.size() < kSpriteEntrySize * 2)
        return false;

    // 1. 按单元格尺寸把连续 id 切成段，一段对应一张源图集
    QVector<SpriteRange> ranges;
    for (int offset = 0; offset + kSpriteEntrySize <= table.size(); offset += kSpriteEntrySize) {
        const quint32 id = static_cast<quint32>(offset / kSpriteEntrySize);
        const int cellW = readU16(table, offset + 0x0C);
        const int cellH = readU16(table, offset + 0x0E);
        if (cellW <= 0 || cellH <= 0)
            continue;
        if (ranges.isEmpty() || ranges.last().cellW != cellW || ranges.last().cellH != cellH) {
            SpriteRange range;
            range.firstId = id;
            range.lastId = id;
            range.cellW = cellW;
            range.cellH = cellH;
            ranges.append(range);
        } else {
            ranges.last().lastId = id;
        }
    }
    if (ranges.isEmpty())
        return false;

    // 2. 只枚举游戏自己的 gfx 与 mods 下的 PNG（mod 可能新建子目录追加）。
    //    不整目录递归：否则会把 debug_version 这类嵌套的游戏副本也当成候选，
    //    同图同尺寸时按路径排序会先命中副本，看起来像“原版读到了 debug_version”。
    QVector<PngFile> files;
    const QDir gameDir(m_gameDir);
    for (const char *sub : {"gfx", "mods"}) {
        const QString root = gameDir.filePath(QString::fromLatin1(sub));
        if (!QFileInfo(root).isDir())
            continue;
        QDirIterator it(root, QStringList() << QStringLiteral("*.png"), QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext() && files.size() < 20000) {
            const QString path = it.next();
            const QSize size = QImageReader(path).size();
            if (size.isEmpty())
                continue;
            PngFile file;
            file.path = path;
            file.rel = gameDir.relativeFilePath(path);
            file.width = size.width();
            file.height = size.height();
            files.append(file);
        }
    }
    if (files.isEmpty())
        return false;

    // 3. 逐文件顺序对齐：精灵 id 按图集加载顺序连续分配，每张 PNG 占一段连续 id。
    //    从段首开始试所有“宽高整除且帧数不越界”的未使用 PNG，取校验分最高者，
    //    采纳后按帧数推进；找不到就把游标后移一位（该 id 可能来自单独注册的精灵）。
    std::sort(files.begin(), files.end(), [](const PngFile &a, const PngFile &b) {
        return a.rel < b.rel;
    });

    QHash<QString, QVector<QRect>> inkCache;
    QSet<QString> usedFiles;
    int matchedFiles = 0;
    int skippedIds = 0;
    int skippedRanges = 0;
    QString report = QStringLiteral("== 运行时精灵表还原 ==\n"
                                    "精灵总数: %1，id 段数: %2，gfx/mods 下 PNG: %3\n\n")
                         .arg(spriteCount)
                         .arg(ranges.size())
                         .arg(files.size());
    QStringList partialNotes;

    for (const SpriteRange &range : ranges) {
        quint32 cursor = range.firstId;
        int rangeMatched = 0;
        int rangeSkipped = 0;

        // 某个 PNG 在当前单元格尺寸下的逐帧包围盒（惰性计算，按“路径|单元格”缓存）
        auto inkRectsOf = [&](int fileIndex) -> const QVector<QRect> * {
            const PngFile &file = files.at(fileIndex);
            const QString key = file.path + QLatin1Char('|')
                                + QString::number(range.cellW) + QLatin1Char('x')
                                + QString::number(range.cellH);
            auto it = inkCache.find(key);
            if (it == inkCache.end())
                it = inkCache.insert(key, computeInkRects(file.path, range.cellW, range.cellH));
            return &it.value();
        };
        // 按当前单元格尺寸切出的帧数
        auto frameCountOf = [&](int fileIndex) -> qint64 {
            const PngFile &file = files.at(fileIndex);
            return static_cast<qint64>(file.width / range.cellW)
                   * (file.height / range.cellH);
        };
        // 该文件能否作为 startId 处的起点：没用过、宽高整除、帧数不越界且能按格切分
        auto isCandidate = [&](int fileIndex, quint32 startId, int ignoreIndex) -> bool {
            if (fileIndex == ignoreIndex)
                return false;
            const PngFile &file = files.at(fileIndex);
            if (usedFiles.contains(file.path))
                return false;
            if (file.width % range.cellW != 0 || file.height % range.cellH != 0)
                return false;
            const qint64 frames = frameCountOf(fileIndex);
            if (frames <= 0 || frames > static_cast<qint64>(range.lastId - startId) + 1)
                return false;
            return static_cast<qint64>(inkRectsOf(fileIndex)->size()) == frames;
        };
        // 采纳 selfIndex 后，下一张文件是否也能接上（区分同一美术的 base/mod 版本）
        auto canContinue = [&](int selfIndex, quint32 nextId) -> bool {
            if (nextId > range.lastId)
                return true; // 正好收在本段末尾也算接上
            for (int j = 0; j < files.size(); ++j) {
                if (!isCandidate(j, nextId, selfIndex))
                    continue;
                if (inkMatchesEntry(inkRectsOf(j)->first(), table, nextId))
                    return true;
            }
            return false;
        };

        while (cursor <= range.lastId) {
            // 收集当前 id 处所有可能的文件起点，比较整张 PNG 的校验分
            int bestScore = -1;
            QVector<int> tied;
            for (int i = 0; i < files.size(); ++i) {
                if (!isCandidate(i, cursor, -1))
                    continue;
                const int score = scoreFrames(*inkRectsOf(i), table, cursor, range.lastId);
                if (score > bestScore) {
                    bestScore = score;
                    tied.clear();
                    tied.append(i);
                } else if (score == bestScore) {
                    tied.append(i);
                }
            }

            int bestIndex = -1;
            int bestPosition = 0;
            if (bestScore >= 60 && !tied.isEmpty()) {
                if (tied.size() == 1) {
                    bestIndex = tied.first();
                    scoreFrames(*inkRectsOf(bestIndex), table, cursor, range.lastId, &bestPosition);
                } else {
                    // 同分：优先“后面能接上下一张文件”的，其次比较包围盒位置
                    int chosenPosition = -1;
                    bool chosenContinue = false;
                    for (int i : tied) {
                        const bool cont = canContinue(i, cursor + static_cast<quint32>(frameCountOf(i)));
                        int position = 0;
                        scoreFrames(*inkRectsOf(i), table, cursor, range.lastId, &position);
                        if ((cont && !chosenContinue)
                            || (cont == chosenContinue && position > chosenPosition)) {
                            bestIndex = i;
                            chosenPosition = position;
                            chosenContinue = cont;
                        }
                    }
                    bestPosition = chosenPosition;
                }
            }

            if (bestIndex >= 0) {
                const PngFile &file = files.at(bestIndex);
                SpriteSheet sheet;
                sheet.title = file.rel;
                sheet.file = file.rel;
                sheet.path = file.path;
                sheet.cellW = range.cellW;
                sheet.cellH = range.cellH;
                sheet.baseId = cursor;
                sheet.frames = static_cast<int>(frameCountOf(bestIndex));
                m_sheets.append(sheet);
                usedFiles.insert(file.path);
                report += QStringLiteral("id %1..%2 单元格 %3x%4（%5 帧）→ %6  校验 %7%（位置 %8）\n")
                              .arg(cursor)
                              .arg(cursor + static_cast<quint32>(sheet.frames) - 1)
                              .arg(range.cellW)
                              .arg(range.cellH)
                              .arg(sheet.frames)
                              .arg(file.rel)
                              .arg(bestScore)
                              .arg(bestPosition);
                cursor += static_cast<quint32>(sheet.frames);
                ++rangeMatched;
                ++matchedFiles;
            } else {
                ++cursor;
                ++rangeSkipped;
            }
        }
        if (rangeSkipped > 0) {
            skippedIds += rangeSkipped;
            ++skippedRanges;
            if (rangeMatched > 0) {
                partialNotes.append(QStringLiteral("id %1..%2 单元格 %3x%4：另跳过 %5 个 id（已匹配 %6 个文件）")
                                        .arg(range.firstId)
                                        .arg(range.lastId)
                                        .arg(range.cellW)
                                        .arg(range.cellH)
                                        .arg(rangeSkipped)
                                        .arg(rangeMatched));
            }
        }
    }

    if (!partialNotes.isEmpty())
        report += QStringLiteral("\n-- 段内未对齐 --\n%1\n").arg(partialNotes.join(QLatin1Char('\n')));
    report += QStringLiteral("\n匹配文件 %1 个；未对齐 %2 个 id 段内共 %3 个 id（多为字体与单独注册的精灵）\n")
                  .arg(matchedFiles)
                  .arg(skippedRanges)
                  .arg(skippedIds);

    // 诊断：静态锚点、每段首尾表项（未知字段）、PNG 尺寸清单。
    // 用于确定表项里是否带“图集序号”，以及 id 与文件的真实对应关系。
    QString diag = QStringLiteral("\n-- 静态图集锚点 --\n");
    for (const SheetSpec &spec : kSheetSpecs) {
        quint32 anchor = 0;
        if (m_memMgr->read<quint32>(base + spec.baseRva, anchor)) {
            diag += QStringLiteral("%1  baseId=%2  单元格=%3x%4\n")
                        .arg(QString::fromUtf8(spec.file))
                        .arg(anchor)
                        .arg(spec.cellW)
                        .arg(spec.cellH);
        }
    }
    diag += QStringLiteral("\n-- 每段首尾表项（未知字段按 u16 解码）--\n");
    for (const SpriteRange &range : ranges) {
        for (int which = 0; which < 2; ++which) {
            if (which == 1 && range.lastId == range.firstId)
                continue;
            const quint32 id = which == 0 ? range.firstId : range.lastId;
            const int offset = static_cast<int>(id) * kSpriteEntrySize;
            QString extra;
            for (int field = 0x10; field <= 0x1A; field += 2) {
                extra += QStringLiteral(" +%1=%2")
                             .arg(field, 2, 16, QLatin1Char('0'))
                             .arg(readU16(table, offset + field));
            }
            extra += QStringLiteral(" +1e=%1").arg(readU16(table, offset + 0x1E));
            diag += QStringLiteral("id %1: x=%2 y=%3 w=%4 h=%5 bbox=%6,%7 cell=%8x%9 |%10\n")
                        .arg(id)
                        .arg(readU16(table, offset + 0x00))
                        .arg(readU16(table, offset + 0x02))
                        .arg(readU16(table, offset + 0x04))
                        .arg(readU16(table, offset + 0x06))
                        .arg(readU16(table, offset + 0x08))
                        .arg(readU16(table, offset + 0x0A))
                        .arg(readU16(table, offset + 0x0C))
                        .arg(readU16(table, offset + 0x0E))
                        .arg(extra);
        }
    }
    diag += QStringLiteral("\n-- PNG 文件（相对路径 宽x高）--\n");
    for (const PngFile &file : files) {
        diag += QStringLiteral("%1  %2x%3\n")
                    .arg(file.rel)
                    .arg(file.width)
                    .arg(file.height);
    }

    report += diag;
    m_sheetReport = report;
    return matchedFiles > 0;
}

void SpriteAtlas::loadSheetImage(int sheetIndex) const
{
    if (sheetIndex < 0 || sheetIndex >= m_sheets.size())
        return;
    SpriteSheet &sheet = m_sheets[sheetIndex];
    if (!sheet.image.isNull() || sheet.path.isEmpty())
        return;
    sheet.image = QImage(sheet.path);
}

bool SpriteAtlas::refresh()
{
    m_cache.clear();
    m_sheets.clear();
    m_ready = false;
    m_error.clear();
    m_scannedSheets = 0;
    m_sheetReport.clear();

    if (!m_memMgr || !m_memMgr->isAttached()) {
        m_error = QCoreApplication::translate("SpriteAtlas", "游戏未附加");
        return false;
    }

    // 首选：运行时精灵表 → 连续 id 段 → gfx/mods 下 PNG（mod 追加的图集同样适用）
    if (buildRuntimeSheets() && !m_sheets.isEmpty()) {
        m_scannedSheets = static_cast<int>(m_sheets.size());
        m_ready = true;
        return true;
    }

    // 回退：静态表 + 启动代码扫描（运行时表不可用时仍能显示常见图集）
    m_sheets.clear();
    for (const SheetSpec &spec : kSheetSpecs) {
        SpriteSheet sheet;
        sheet.title = QCoreApplication::translate("SpriteAtlas", spec.title);
        sheet.file = QString::fromUtf8(spec.file);
        sheet.cellW = spec.cellW;
        sheet.cellH = spec.cellH;
        sheet.baseRva = spec.baseRva;
        m_sheets.append(sheet);
    }

    // 运行时扫描：按文件名覆盖静态表中的 RVA 与单帧尺寸，游戏版本更新后仍可用
    const QByteArray code = m_memMgr->readCodeSection();
    if (!code.isEmpty()) {
        const QVector<ScannedSheet> found = scanSheetTable(m_memMgr, code);
        for (SpriteSheet &sheet : m_sheets) {
            for (const ScannedSheet &scanned : found) {
                if (QString::compare(scanned.file, sheet.file, Qt::CaseInsensitive) == 0) {
                    sheet.cellW = scanned.cellW;
                    sheet.cellH = scanned.cellH;
                    sheet.baseRva = scanned.baseRva;
                    ++m_scannedSheets;
                    break;
                }
            }
        }
    }

    const quint64 moduleBase = m_memMgr->moduleBaseAddress();
    int loadedCount = 0;
    for (SpriteSheet &sheet : m_sheets) {
        quint32 baseId = 0;
        if (moduleBase && m_memMgr->read<quint32>(moduleBase + sheet.baseRva, baseId))
            sheet.baseId = baseId;

        const QString path = m_gameDir + QLatin1Char('/') + sheet.file;
        if (!QFileInfo::exists(path))
            continue;
        sheet.image = QImage(path);
        if (!sheet.image.isNull())
            ++loadedCount;
    }

    if (loadedCount == 0) {
        m_error = QCoreApplication::translate("SpriteAtlas", "未找到精灵图集（请确认已附加游戏且游戏目录正确）");
        return false;
    }
    m_ready = true;
    return true;
}

bool SpriteAtlas::locate(quint32 spriteId, int *sheetIndex, int *frameIndex) const
{
    for (int i = 0; i < m_sheets.size(); ++i) {
        const SpriteSheet &sheet = m_sheets.at(i);
        if (!sheet.loaded() || spriteId < sheet.baseId)
            continue;
        // 静态表在 baseId 未读到时不能参与定位；运行时段的 baseId 可以是 0
        if (sheet.baseId == 0 && sheet.path.isEmpty())
            continue;
        const int frame = static_cast<int>(spriteId - sheet.baseId);
        if (frame < sheet.frameCount()) {
            if (sheetIndex) *sheetIndex = i;
            if (frameIndex) *frameIndex = frame;
            return true;
        }
    }
    return false;
}

QImage SpriteAtlas::frameImage(const SpriteSheet &sheet, int frameIndex)
{
    if (!sheet.loaded() || frameIndex < 0)
        return QImage();
    const int cols = sheet.columns();
    if (cols <= 0)
        return QImage();
    const int col = frameIndex % cols;
    const int row = frameIndex / cols;
    const QRect rect(col * sheet.cellW, row * sheet.cellH, sheet.cellW, sheet.cellH);
    QImage cell = sheet.image.copy(rect);
    if (cell.isNull())
        return cell;

    // 游戏注册帧时（0x401D90）会统计非透明像素包围盒，只把该子矩形写入精灵表，
    // 这里保持一致：否则预览会带上整格透明边距，看起来“没对上”。
    const QImage argb = cell.convertToFormat(QImage::Format_ARGB32);
    int minX = argb.width();
    int minY = argb.height();
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < argb.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(argb.constScanLine(y));
        for (int x = 0; x < argb.width(); ++x) {
            if (qAlpha(line[x]) == 0)
                continue;
            minX = qMin(minX, x);
            maxX = qMax(maxX, x);
            minY = qMin(minY, y);
            maxY = qMax(maxY, y);
        }
    }
    if (maxX < minX || maxY < minY)
        return QImage(); // 空格子：游戏里也只是一帧全透明

    return cell.copy(QRect(minX, minY, maxX - minX + 1, maxY - minY + 1));
}

QPixmap SpriteAtlas::pixmap(quint32 spriteId, int boxSize) const
{
    int sheetIndex = -1;
    int frameIndex = -1;
    if (!locate(spriteId, &sheetIndex, &frameIndex))
        return QPixmap();

    const quint64 key = (static_cast<quint64>(sheetIndex) << 40)
                        | (static_cast<quint64>(spriteId) << 8)
                        | static_cast<quint64>(boxSize & 0xFF);
    auto it = m_cache.constFind(key);
    if (it != m_cache.constEnd())
        return it.value();

    loadSheetImage(sheetIndex);
    const SpriteSheet &sheet = m_sheets.at(sheetIndex);
    QImage frame = frameImage(sheet, frameIndex);
    QPixmap result;
    if (!frame.isNull()) {
        const qreal scale = static_cast<qreal>(boxSize)
                            / qMax(frame.width(), frame.height());
        const int w = qMax(1, static_cast<int>(frame.width() * scale));
        const int h = qMax(1, static_cast<int>(frame.height() * scale));
        const QImage scaled = frame.scaled(w, h, Qt::KeepAspectRatio,
                                           Qt::FastTransformation);

        // 精灵带 alpha，浅色/白色精灵直接贴控件背景会糊成一片，看不出轮廓。
        // 先铺一层 8px 交替灰棋盘格做底，再把精灵叠上去，透明区域一眼可辨。
        const int tile = 8;
        QImage canvas(w, h, QImage::Format_ARGB32);
        for (int y = 0; y < h; ++y) {
            QRgb *line = reinterpret_cast<QRgb *>(canvas.scanLine(y));
            const bool rowOdd = (y / tile) & 1;
            for (int x = 0; x < w; ++x) {
                const bool colOdd = (x / tile) & 1;
                line[x] = (rowOdd != colOdd) ? 0xFFB0B0B0u : 0xFFD8D8D8u;
            }
        }
        QPainter painter(&canvas);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.drawImage(0, 0, scaled);
        painter.end();

        result = QPixmap::fromImage(canvas);
    }
    m_cache.insert(key, result);
    return result;
}

QString SpriteAtlas::describe(quint32 spriteId) const
{
    int sheetIndex = -1;
    int frameIndex = -1;
    if (!locate(spriteId, &sheetIndex, &frameIndex))
        return QCoreApplication::translate("SpriteAtlas", "ID %1（无法确定图集）").arg(spriteId);
    return QCoreApplication::translate("SpriteAtlas", "ID %1 · %2 第 %3 帧")
        .arg(spriteId)
        .arg(m_sheets.at(sheetIndex).title)
        .arg(frameIndex);
}
