#ifndef DR2C_INTERNAL_ENTITY_DATA_H
#define DR2C_INTERNAL_ENTITY_DATA_H

struct Dr2cEntityView {
    unsigned short id = 0;
    unsigned char type = 0;
    unsigned char subtype = 0;
    unsigned char mapId = 0;
    unsigned char noCollide = 0;
    unsigned char noPick = 0;
    unsigned char unseen = 0;
    unsigned char invisible = 0;
    float position[3] = {};
    float velocity[3] = {};
    float physics[3] = {};
    unsigned char glow = 0;
    unsigned short spriteId = 0;
    int hitpoints = 0;
    unsigned char noHit = 0;
    unsigned int aiState = 0;
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
