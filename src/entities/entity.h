#ifndef SPACETRAVELLER_ENTITY_H
#define SPACETRAVELLER_ENTITY_H

#include <cstdint>

namespace godot {

static constexpr uint32_t INVALID_ENTITY_ID = UINT32_MAX;
static constexpr uint32_t PLAYER_ENTITY_ID = 0;

struct Entity {
    uint32_t id;
    int x, y, z;
    uint32_t component_mask;
    float next_turn_time;
    float condition_time;
    float condition_recovery_multiplier;
    uint16_t atlas_x;
    uint16_t atlas_y;
};

}

#endif // SPACETRAVELLER_ENTITY_H
