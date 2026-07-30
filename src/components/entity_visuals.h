#ifndef SPACETRAVELLER_ENTITY_VISUALS_H
#define SPACETRAVELLER_ENTITY_VISUALS_H

#include <cstdint>
#include <limits>

namespace godot {

struct EntityVisualAtlas {
    uint16_t x = 0;
    uint16_t y = 0;
};

namespace EntityVisuals {

inline EntityVisualAtlas resolve_atlas(
    uint16_t standing_x,
    uint16_t standing_y,
    int race_origin_x,
    int downed_atlas_offset,
    bool downed
) {
    EntityVisualAtlas result{standing_x, standing_y};
    if (!downed || downed_atlas_offset < 0) {
        return result;
    }

    const int downed_x = race_origin_x + downed_atlas_offset;
    if (downed_x < 0 ||
        downed_x > static_cast<int>(std::numeric_limits<uint16_t>::max())) {
        return result;
    }

    result.x = static_cast<uint16_t>(downed_x);
    return result;
}

}

}

#endif // SPACETRAVELLER_ENTITY_VISUALS_H
