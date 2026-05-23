#ifndef SPACETRAVELLER_OCCLUSION_H
#define SPACETRAVELLER_OCCLUSION_H

#include "core/world_coords.h"
#include <godot_cpp/variant/vector2i.hpp>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

namespace godot {

class TileDb;

class Occlusion {
public:
    static inline uint64_t pack_coords(int x, int y) {
        return WorldCoords::pack_coords(x, y);
    }

    // Compute all visible tiles using Recursive Shadowcasting
    // Output: visible_keys contains pack_coords(x,y) for each visible tile
    static void compute_visible(
        const Vector2i& playerPos,
        int radius,
        const std::unordered_map<uint64_t, uint16_t>& tile_cache,
        std::unordered_set<uint64_t>& visible_keys
    );
};

}

#endif // SPACETRAVELLER_OCCLUSION_H