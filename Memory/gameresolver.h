#ifndef GAMERESOLVER_H
#define GAMERESOLVER_H

#include <QtGlobal>
#include <QString>

#include <array>
#include <cstddef>

#include "../Common/protocol.h"

// ============================================================================
// GameResolver：前端侧的布局描述。
//
// 特征码扫描已经搬到注入的 Agent 中执行，这里只保留数据结构与函数编号，
// 函数编号直接复用 Common/protocol.h 中的 dr2c::FunctionId，保证两端一致。
// ============================================================================

class GameResolver
{
public:
    using Function = dr2c::FunctionId;

    static constexpr std::size_t FunctionCount =
        static_cast<std::size_t>(dr2c::FunctionId::Count);

    struct Layout {
        bool valid = false;
        QString error;
        QString warning;
        quint64 imageBase = 0;
        quint32 imageSize = 0;
        std::array<quint32, FunctionCount> functionRvas{};

        quint32 thingPoolRva = 0;
        quint32 weaponPoolRva = 0;
        quint32 missionStateRva = 0;
        quint32 characterPoolPointerRva = 0;
        quint32 characterPoolRva = 0;
        quint32 mapLayerMetaRva = 0;
        quint32 currentMapIdRva = 0;
        quint32 nextThingIdRva = 0;
        quint32 cameraMapLayerRva = 0;
        quint32 playerThingArrayRva = 0;
        quint32 thingStride = 0;
        quint32 characterStride = 0;
        quint32 weaponStride = 0;
        quint32 mapMetaStride = 0;
    };
};

#endif // GAMERESOLVER_H
