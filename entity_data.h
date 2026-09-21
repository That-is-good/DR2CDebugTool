#ifndef DR2C_INTERNAL_ENTITY_DATA_H
#define DR2C_INTERNAL_ENTITY_DATA_H

#include <cstdint>

struct Dr2cEntityView {
    std::uint16_t id = 0;
    std::uint8_t  type = 0;
    std::uint8_t  subtype = 0;
    std::uint8_t  mapId = 0;
    std::uint8_t  noCollide = 0;
    std::uint8_t  noPick = 0;
    std::uint8_t  unseen = 0;
    std::uint8_t  invisible = 0;
    float position[3] = {};
    float velocity[3] = {};
    float physics[3] = {};          // thing: mass / friction / bounce_friction
    std::uint8_t  glow = 0;
    std::uint16_t spriteId = 0;
    std::int32_t  hitpoints = 0;
    std::uint8_t  noHit = 0;
    std::uint32_t aiState = 0;
};

enum Dr2cEntityWriteMask {
    DR2C_ENTITY_WRITE_MAP = 1u << 0,
    DR2C_ENTITY_WRITE_POSITION = 1u << 1,
    DR2C_ENTITY_WRITE_VELOCITY = 1u << 2,
    DR2C_ENTITY_WRITE_PHYSICS = 1u << 3,
    DR2C_ENTITY_WRITE_FLAGS = 1u << 4,
    DR2C_ENTITY_WRITE_SPRITE = 1u << 5,
    DR2C_ENTITY_WRITE_HITPOINTS = 1u << 6,
    DR2C_ENTITY_WRITE_AI = 1u << 7
};

#endif
