#ifndef GAMERESOLVER_H
#define GAMERESOLVER_H

#include <QByteArray>
#include <QString>
#include <array>

class GameResolver
{
public:
    enum class Function {
        ScriptEvaluateStringSafe,
        FreeThing,
        AllocateEntity,
        AllocateThing,
        AllocateCharacterSlot,
        AssignCharacterToThing,
        SwitchActiveTileLayer,
        SetCurrentPlayerThing,
        Count
    };

    struct Layout {
        bool valid = false;
        QString error;
        QString warning;
        quint64 imageBase = 0;
        quint32 imageSize = 0;
        std::array<quint32, static_cast<size_t>(Function::Count)> functionRvas{};

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

    static bool resolve(const QByteArray &executableSection,
                        quint32 sectionRva,
                        quint64 imageBase,
                        quint32 imageSize,
                        Layout *layout);
    static bool findUnique(const QByteArray &image,
                           const QByteArray &bytes,
                           const QByteArray &mask,
                           int *matchOffset,
                           int *matchCount = nullptr);
};

#endif
