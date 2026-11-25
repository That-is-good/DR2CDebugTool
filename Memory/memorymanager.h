#ifndef MEMORYMANAGER_H
#define MEMORYMANAGER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QVariantMap>
#include <QByteArray>
#include <QRect>
#include <QVector>

#include "base.h"
#include "gameresolver.h"

// ============================================================================
// MemoryManager（Qt 前端侧）
//
// 本类不再直接 ReadProcessMemory 读取游戏数据，而是：
//   1. 把 DR2CAgent.dll 注入游戏进程；
//   2. 通过命名管道 \\.\pipe\DR2CTool_<pid> 与 Agent 通信；
//   3. 对上层保持原有 API（readMemory/writeMemory/CallFunction 等）不变。
// ============================================================================

struct HoverInfo
{
    bool found = false;
    quint32 thingId = 0;
    quint32 slot = 0;
    quint32 mapId = 0;
    quint64 thingAddress = 0; // 实体在游戏进程中的地址
    QRect rect;              // 高亮矩形（游戏客户区坐标）
    float worldX = 0.0f;
    float worldY = 0.0f;
};

class MemoryManager : public QObject
{
    Q_OBJECT

public:
    explicit MemoryManager(QObject *parent = nullptr);
    ~MemoryManager() override;

    // 进程管理
    QList<QVariantMap> enumerateProcesses();
    bool attachProcess(const QString&);
    bool attachProcessById(quint32);
    void detachProcess();
    bool isAttached() const { return m_attached; }
    bool isProcessAlive() const;
    QString attachedProcessName() const { return m_processName; }
    quint32 attachedProcessId() const { return m_processId; }
    QString processImagePath() const;
    QString gameDirectory() const;

    // 主模块基址
    quint64 moduleBaseAddress() const { return m_moduleBase; }
    const GameResolver::Layout &gameLayout() const { return m_gameLayout; }
    bool hasFunction(GameResolver::Function function) const;

    // 主模块代码节（图集扫描用；数据由 Agent 回传）
    quint32 codeSectionRva() const { return m_codeSectionRva; }
    quint32 codeSectionSize() const { return m_codeSectionSize; }
    QByteArray readCodeSection() const;

    // 内存读写（经 Agent 转发）
    bool readMemory(quint64, void*, quint64) const;
    bool writeMemory(quint64, const void*, quint64) const;

    template<typename T>
    bool read(quint64 address, T &value) const
    {
        return readMemory(address, &value, sizeof(T));
    }

    template<typename T>
    bool write(quint64 address, const T &value) const
    {
        return writeMemory(address, &value, sizeof(T));
    }

    QString readString(quint64, quint64 maxLen = 40) const;
    bool writeString(quint64, const QString&, quint64 maxLen = 40) const;

    // 调用游戏函数（在 Agent 所在线程内执行，异常由 Agent 捕获）
    bool CallFunction(GameResolver::Function, const QVector<quint64>&,
                      quint64 *retValue = nullptr) const;

    bool ScriptEvaluateStringSafe(const QString&) const;
    bool FreeThing(quint64) const;
    quint64 AllocateEntity(qint8) const;
    quint64 AllocateThing(qint8) const;
    quint32 AllocateCharacterSlot() const;
    bool Assigncharactertothing(quint64, quint32) const;
    quint64 CreateWeaponEntity(qint32 weaponId) const;
    bool SwitchActiveTileLayer(quint8) const;
    bool SetCurrentPlayerThing(quint64) const;

    // 世界交互（悬停 / 拖拽）
    bool hoverThing(int mouseX, int mouseY, int clientWidth, int clientHeight,
                    HoverInfo *out) const;
    bool dragBegin(quint64 thingAddress) const;
    bool dragMove(quint64 thingAddress, int mouseX, int mouseY,
                  int clientWidth, int clientHeight) const;
    bool dragEnd(quint64 thingAddress) const;

    // 游戏窗口（用于把客户区坐标映射到屏幕坐标）
    quint64 gameWindowHandle() const;

signals:
    void processAttached();
    void processDetached();
    void attachError(const QString&);

private:
    bool injectAgent(QString *error);
    bool connectAgent(QString *error);
    bool fetchLayout(QString *error);

    // 同步收发一条请求；status 为响应帧头中的状态码
    bool sendRequest(quint16 command, const QByteArray &request,
                     QByteArray *response, quint32 *status) const;

    bool m_attached = false;
    bool m_agentInjected = false;
    quint32 m_processId = 0;
    QString m_processName;
    quint64 m_moduleBase = 0;
    quint32 m_codeSectionRva = 0;
    quint32 m_codeSectionSize = 0;
    GameResolver::Layout m_gameLayout;
    void *m_handle = nullptr; // 进程 HANDLE
    void *m_pipe = nullptr;   // 管道 HANDLE
    mutable quint32 m_sequence = 0;
};

#endif // MEMORYMANAGER_H
