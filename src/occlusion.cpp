#include "occlusion.h"
#include "data/tile_db.h"
#include <cstdlib>

namespace godot {

bool Occlusion::is_occluded(
    const Vector2i& cellPos,
    const Vector2i& playerPos,
    const std::unordered_map<uint64_t, String>& tile_cache
) {
    TileDb* p_tile_db = TileDb::get_singleton();
    if (!p_tile_db) return false;

    // Player's own tile is never occluded
    if (cellPos == playerPos) {
        return false;
    }
    
    // Check if cell exists in cache
    uint64_t cellKey = pack_coords(cellPos.x, cellPos.y);
    if (tile_cache.find(cellKey) == tile_cache.end()) {
        return false;
    }
    
    // Bresenham's line algorithm
    int x0 = playerPos.x, y0 = playerPos.y;
    int x1 = cellPos.x, y1 = cellPos.y;
    
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    
    int x = x0, y = y0;
    
    while (true) {
        // Skip the starting position (player)
        if (x != x0 || y != y0) {
            // If we've reached the target, we're done (not occluded)
            if (x == x1 && y == y1) {
                break;
            }
            
            // Check if this intermediate cell is a wall
            auto it = tile_cache.find(pack_coords(x, y));
            if (it != tile_cache.end()) {
                const TileInfo* info = p_tile_db->get_tile_info(it->second);
                if (info && info->solid) {
                    return true;  // Wall blocks line of sight
                }
            }
        }
        
        if (x == x1 && y == y1) {
            break;
        }
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
    
    return false;
}

bool Occlusion::is_surrounded_by_walls(
    const Vector2i& playerPos,
    const std::unordered_map<uint64_t, String>& tile_cache
) {
    TileDb* p_tile_db = TileDb::get_singleton();
    if (!p_tile_db) return false;

    static const int neighbors[8][2] = {
        {-1, -1}, {0, -1}, {1, -1},
        {-1,  0},          {1,  0},
        {-1,  1}, {0,  1}, {1,  1}
    };
    
    for (int i = 0; i < 8; i++) {
        int nx = playerPos.x + neighbors[i][0];
        int ny = playerPos.y + neighbors[i][1];
        auto it = tile_cache.find(pack_coords(nx, ny));
        if (it != tile_cache.end()) {
            const TileInfo* info = p_tile_db->get_tile_info(it->second);
            if (!info || !info->solid) return false;
        } else {
            return false;
        }
    }
    
    return true;
}

}
