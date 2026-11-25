#ifndef GAMEOVERLAY_H
#define GAMEOVERLAY_H

// ============================================================================
// GameOverlay
// 覆盖在游戏客户区上方的透明层：
//   - 悬停时高亮鼠标下方的实体（矩形由 Agent 计算）；
//   - 左键按下后可拖动实体，拖动过程实时写回坐标。
// 仅在“世界交互”开关打开且游戏窗口位于前台时显示。
// ============================================================================

#include <QWidget>

#include "../Memory/memorymanager.h"

class QTimer;

class GameOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit GameOverlay(MemoryManager *memory, QWidget *parent = nullptr);

    void setActive(bool active);
    bool isActive() const { return m_active; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void tick();
    bool syncGeometry();
    void cancelDrag();
    void resetTransientState();

    MemoryManager *m_memory = nullptr;
    QTimer *m_timer = nullptr;
    bool m_active = false;
    HoverInfo m_hover;
    bool m_dragging = false;
    bool m_prevLeftDown = false;
    quint64 m_dragAddress = 0;
    int m_clientWidth = 0;  // 游戏客户区宽度（物理像素）
    int m_clientHeight = 0; // 游戏客户区高度（物理像素）
};

#endif // GAMEOVERLAY_H
