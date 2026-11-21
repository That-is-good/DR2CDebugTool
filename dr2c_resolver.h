#ifndef DR2C_RESOLVER_H
#define DR2C_RESOLVER_H

#include "dr2c_memory.h"
#include <cstddef>
#include <cstdint>

namespace dr2c {

struct GameLayout {
    bool valid = false;
    const char *error = nullptr;
    std::uint32_t imageSize = 0;

    enum Function {
        ScriptEvaluateStringSafe,
        FreeThing,
        AllocateEntity,
        AllocateThing,
        AllocateCharacterSlot,
        AssignCharacterToThing,
        SwitchActiveTileLayer,
        SetCurrentPlayerThing,
        GetCharacterData,
        Count
    };

    std::uint32_t functionRvas[Count] = {};
    std::uint32_t thingPoolRva = 0;
    std::uint32_t weaponPoolRva = 0;
    std::uint32_t characterPoolPointerRva = 0;
    std::uint32_t characterPoolRva = 0;
    std::uint32_t currentMapIdRva = 0;
    std::uint32_t thingStride = 0;
    std::uint32_t characterStride = 0;
    std::uint32_t weaponStride = 0;
};

bool ResolveGameLayout(Address moduleBase, GameLayout *layout);

} // namespace dr2c

#endif
