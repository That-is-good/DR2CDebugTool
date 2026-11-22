#include "spriteatlas.h"

#include "../Memory/memorymanager.h"

#include <QFileInfo>

namespace {

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
    {"武器 8x32",   "gfx/items/dr2c_weapons.png",             8,  32, 0x46C8A4u},
    {"武器 16x32",  "gfx/items/dr2c_weapons_16x32px.png",     16, 32, 0x46C8A0u},
    {"拾取物 16",   "gfx/items/dr2c_loot16.png",              16, 16, 0x46C89Cu},
    {"室内物件 16", "gfx/items/dr2c_cityfurniture_16px.png",  16, 16, 0x46C908u},
    {"家具 32",     "gfx/items/dr2c_housefurniture_32px.png", 32, 32, 0x46C8E8u},
    {"特殊 16",     "gfx/items/special16.png",                16, 16, 0x46C8E4u},
    {"特殊 32",     "gfx/items/special32.png",                32, 32, 0x46C8E0u},
    {"特殊 48",     "gfx/items/special48.png",                48, 48, 0x46C8DCu},
    {"特殊 64",     "gfx/items/special64.png",                64, 64, 0x46C8D8u},
    {"特殊 96",     "gfx/items/special96.png",                96, 96, 0x46C8D4u},
    {"车辆",        "gfx/cars/dr2c_cars110x96.png",           110, 96, 0x46C898u},
    {"车辆残骸",    "gfx/cars/car-wrecks124x96.png",          124, 96, 0x46C894u},
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

bool SpriteAtlas::refresh()
{
    m_cache.clear();
    m_sheets.clear();
    for (const SheetSpec &spec : kSheetSpecs) {
        SpriteSheet sheet;
        sheet.title = QString::fromUtf8(spec.title);
        sheet.file = QString::fromUtf8(spec.file);
        sheet.cellW = spec.cellW;
        sheet.cellH = spec.cellH;
        sheet.baseRva = spec.baseRva;
        m_sheets.append(sheet);
    }

    m_ready = false;
    m_error.clear();
    m_scannedSheets = 0;

    if (!m_memMgr || !m_memMgr->isAttached()) {
        m_error = QStringLiteral("游戏未附加");
        return false;
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
        m_error = QStringLiteral("未找到精灵图集（请确认已附加游戏且游戏目录正确）");
        return false;
    }
    m_ready = true;
    return true;
}

bool SpriteAtlas::locate(quint32 spriteId, int *sheetIndex, int *frameIndex) const
{
    for (int i = 0; i < m_sheets.size(); ++i) {
        const SpriteSheet &sheet = m_sheets.at(i);
        if (!sheet.loaded() || sheet.baseId == 0 || spriteId < sheet.baseId)
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

    const SpriteSheet &sheet = m_sheets.at(sheetIndex);
    QImage frame = frameImage(sheet, frameIndex);
    QPixmap result;
    if (!frame.isNull()) {
        const qreal scale = static_cast<qreal>(boxSize)
                            / qMax(frame.width(), frame.height());
        const int w = qMax(1, static_cast<int>(frame.width() * scale));
        const int h = qMax(1, static_cast<int>(frame.height() * scale));
        result = QPixmap::fromImage(frame.scaled(w, h, Qt::KeepAspectRatio,
                                                 Qt::FastTransformation));
    }
    m_cache.insert(key, result);
    return result;
}

QString SpriteAtlas::describe(quint32 spriteId) const
{
    int sheetIndex = -1;
    int frameIndex = -1;
    if (!locate(spriteId, &sheetIndex, &frameIndex))
        return QStringLiteral("ID %1（无法确定图集）").arg(spriteId);
    return QStringLiteral("ID %1 · %2 第 %3 帧")
        .arg(spriteId)
        .arg(m_sheets.at(sheetIndex).title)
        .arg(frameIndex);
}
