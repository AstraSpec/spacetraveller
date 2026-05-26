#ifndef SPACETRAVELLER_ENTITY_H
#define SPACETRAVELLER_ENTITY_H

#include <cstdint>

namespace godot {

struct Entity {
    uint32_t id;
    int x, y;
    uint32_t component_mask;
    float next_turn_time;
    uint16_t atlas_x;
    uint16_t atlas_y;
};

}

#endif // SPACETRAVELLER_ENTITY_H