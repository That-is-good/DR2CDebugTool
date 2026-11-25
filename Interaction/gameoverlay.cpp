#include "gameoverlay.h"

#include <QCursor>
#include <QPainter>
#include <QTimer>

#include <cmath>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

#ifdef Q_OS_WIN
HWND overlayTargetWindow(MemoryManager *memory)
{
    if (!memory)
        return nullptr;
    return reinterpret_cast<HWND>(static_cast<quintptr>(memory->gameWindowHandle()));
}
#endif

} // namespace

GameOverlay::GameOverlay(MemoryManager *memory, QWidget *parent)
    : QWidget(parent)
    , m_memory(memory)
    , m_timer(new QTimer(this))
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                   Qt::WindowDoesNotAcceptFocus | Qt::NoDropShadowWindowHint |
                   Qt::WindowTransparentForInput);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    // 覆盖层不拦截鼠标：悬停/拖拽通过轮询鼠标状态实现，游戏仍可正常操作。
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);

    connect(m_timer, &QTimer::timeout, this, &GameOverlay::tick);
}

void GameOverlay::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;

    if (active) {
        m_prevLeftDown = false;
        m_timer->start(60);
        tick();
    } else {
        m_timer->stop();
        cancelDrag();
        m_prevLeftDown = false;
        m_hover = HoverInfo{};
        hide();
    }
}

void GameOverlay::tick()
{
    if (!m_active || !m_memory)
        return;

    // 窗口不存在或游戏不在前台时不显示，避免覆盖其他程序
#ifdef Q_OS_WIN
    HWND window = overlayTargetWindow(m_memory);
    if (!window) {
        resetTransientState();
        hide();
        return;
    }
    if (IsIconic(window) || GetForegroundWindow() != window) {
        resetTransientState();
        hide();
        return;
    }
#else
    resetTransientState();
    hide();
    return;
#endif

    if (!syncGeometry()) {
        resetTransientState();
        hide();
        return;
    }
    if (!isVisible()) {
        show();
        raise();
    }

    // 逻辑坐标（Qt）→ 游戏客户区物理像素
    const qreal dpr = devicePixelRatioF();
    const QPoint cursor = mapFromGlobal(QCursor::pos());
    const bool inside = rect().contains(cursor);
    const int pixelX = static_cast<int>(std::lround(cursor.x() * dpr));
    const int pixelY = static_cast<int>(std::lround(cursor.y() * dpr));

    bool leftDown = false;
#ifdef Q_OS_WIN
    leftDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
#endif
    // 只在按下沿触发一次拖拽开始
    const bool pressed = leftDown && !m_prevLeftDown;
    m_prevLeftDown = leftDown;

    if (m_dragging) {
        if (leftDown) {
            m_memory->dragMove(m_dragAddress, pixelX, pixelY,
                               m_clientWidth, m_clientHeight);
        } else {
            m_memory->dragEnd(m_dragAddress);
            cancelDrag();
        }
        update();
        return;
    }

    HoverInfo hover;
    if (inside && m_memory->hoverThing(pixelX, pixelY,
                                       m_clientWidth, m_clientHeight, &hover)) {
        // 使用 Agent 返回的结果
    } else {
        hover = HoverInfo{};
    }
    m_hover = hover;

    if (pressed && inside && m_hover.found &&
        m_memory->dragBegin(m_hover.thingAddress)) {
        m_dragging = true;
        m_dragAddress = m_hover.thingAddress;
        m_timer->setInterval(16); // 拖动时提高刷新率
    }

    update();
}

bool GameOverlay::syncGeometry()
{
#ifdef Q_OS_WIN
    HWND window = overlayTargetWindow(m_memory);
    if (!window)
        return false;

    RECT client{};
    if (!GetClientRect(window, &client))
        return false;
    POINT origin{0, 0};
    if (!ClientToScreen(window, &origin))
        return false;

    m_clientWidth = static_cast<int>(client.right - client.left);
    m_clientHeight = static_cast<int>(client.bottom - client.top);

    // Win32 返回物理像素，Qt 顶层窗口几何使用逻辑像素，需要按 DPI 换算
    const qreal dpr = devicePixelRatioF();
    const QRect target(static_cast<int>(std::lround(origin.x / dpr)),
                       static_cast<int>(std::lround(origin.y / dpr)),
                       static_cast<int>(std::lround(m_clientWidth / dpr)),
                       static_cast<int>(std::lround(m_clientHeight / dpr)));
    if (target != geometry())
        setGeometry(target);
    return target.isValid();
#else
    return false;
#endif
}

void GameOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    // 先清空上一帧，避免高亮消失后残留
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect(), Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    if (!m_hover.found)
        return;

    // 高亮矩形来自 Agent，是游戏客户区的物理像素，绘制前换算成逻辑像素
    const qreal dpr = devicePixelRatioF();
    const QRectF target(m_hover.rect.x() / dpr, m_hover.rect.y() / dpr,
                        m_hover.rect.width() / dpr, m_hover.rect.height() / dpr);

    painter.setRenderHint(QPainter::Antialiasing, false);

    QColor color(255, 220, 80, 230);
    if (m_dragging)
        color = QColor(255, 80, 80, 240);

    painter.setPen(QPen(color, 2));
    QColor fill = color;
    fill.setAlpha(48);
    painter.setBrush(fill);
    painter.drawRect(target);
}

void GameOverlay::cancelDrag()
{
    m_dragging = false;
    m_dragAddress = 0;
    if (m_active)
        m_timer->setInterval(60);
    update();
}

void GameOverlay::resetTransientState()
{
    cancelDrag();
    m_hover = HoverInfo{};
    m_prevLeftDown = false;
}
