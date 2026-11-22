#ifndef SPRITEATLAS_H
#define SPRITEATLAS_H

#include <QString>
#include <QVector>
#include <QImage>
#include <QHash>
#include <QPixmap>

class MemoryManager;

// 一张游戏图集（精灵表）。cellW/cellH 为单帧尺寸，baseRva 是游戏内
// “该图集首帧全局编号”变量的 RVA（死亡之路用全局 spriteid 编号）。
struct SpriteSheet {
    QString title;      // 界面显示名
    QString file;       // 相对游戏目录的图片路径
    int cellW = 16;
    int cellH = 16;
    quint32 baseRva = 0;
    quint32 baseId = 0; // 运行时读取
    QImage image;       // 运行时加载

    int columns() const { return cellW > 0 ? image.width() / cellW : 0; }
    int rows() const { return cellH > 0 ? image.height() / cellH : 0; }
    int frameCount() const { return columns() * rows(); }
    bool loaded() const { return !image.isNull(); }
};

// 精灵图集访问：把 spriteid 换算为 (图集, 帧序号) 并裁剪出图像。
// 图集布局来自 prog.exe 启动时注册 gfx 精灵表的代码（逐帧等宽网格）。
class SpriteAtlas
{
public:
    explicit SpriteAtlas(MemoryManager *memMgr);

    void setGameDirectory(const QString &dir) { m_gameDir = dir; }
    QString gameDirectory() const { return m_gameDir; }

    // 读取各图集首帧编号并加载图片；游戏未附加或图片缺失时返回 false
    bool refresh();
    bool isReady() const { return m_ready; }
    QString lastError() const { return m_error; }
    // 本次 refresh() 中由运行时扫描确认的图集数量（0 表示回退到静态表）
    int scannedSheets() const { return m_scannedSheets; }

    const QVector<SpriteSheet> &sheets() const { return m_sheets; }

    bool locate(quint32 spriteId, int *sheetIndex, int *frameIndex) const;
    QPixmap pixmap(quint32 spriteId, int boxSize) const;
    QString describe(quint32 spriteId) const;

    static QImage frameImage(const SpriteSheet &sheet, int frameIndex);

private:
    MemoryManager *m_memMgr;
    QString m_gameDir;
    QString m_error;
    bool m_ready = false;
    int m_scannedSheets = 0;
    QVector<SpriteSheet> m_sheets;
    mutable QHash<quint64, QPixmap> m_cache;
};

#endif // SPRITEATLAS_H
