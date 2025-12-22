#ifndef SPACETRAVELLER_OCCLUSION_H
#define SPACETRAVELLER_OCCLUSION_H

#include <godot_cpp/variant/vector2i.hpp>
#include <unordered_map>
#include <cstdint>

namespace godot {

class Occlusion {
public:
    // Pack two int32s into a uint64 key
    static inline uint64_t pack_coords(int x, int y) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | 
               static_cast<uint64_t>(static_cast<uint32_t>(y));
    }
    
    // Check if a cell is occluded from the player using Bresenham's line algorithm
    static bool is_occluded(
        const Vector2i& cellPos,
        const Vector2i& playerPos,
        const std::unordered_map<uint64_t, int>& tile_cache,
        int wall_tile_y
    );
    
    // Check if player is surrounded by walls on all 8 sides
    static bool is_surrounded_by_walls(
        const Vector2i& playerPos,
        const std::unordered_map<uint64_t, int>& tile_cache,
        int wall_tile_y
    );
};

}

#endif // SPACETRAVELLER_OCCLUSION_H

