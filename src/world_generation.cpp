#include "world_generation.h"

using namespace godot;

void WorldGeneration::_bind_methods() {
	ClassDB::bind_method(D_METHOD("compute_occlusion_batch", "tileOffsets", "playerPos", "tileData"), &WorldGeneration::compute_occlusion_batch);
}

WorldGeneration::WorldGeneration() {
}

WorldGeneration::~WorldGeneration() {
    
}

Dictionary WorldGeneration::compute_occlusion_batch(const Array& tileOffsets, const Vector2i& playerPos, const Dictionary& tileData) {
    // Build internal tile map from dictionary once
    std::unordered_map<uint64_t, int> tileMap;
    tileMap.reserve(tileData.size());
    
    Array keys = tileData.keys();
    for (int i = 0; i < keys.size(); i++) {
        Vector2i cellPos = keys[i];
        Dictionary cellData = tileData[cellPos];
        int tileY = cellData["tile_y"];
        tileMap[pack_coords(cellPos.x, cellPos.y)] = tileY;
    }
    
    // Process all tiles and build results
    Dictionary results;
    for (int i = 0; i < tileOffsets.size(); i++) {
        Vector2i tileOffset = tileOffsets[i];
        Vector2i cellPos = tileOffset + playerPos;
        bool occluded = is_occluded_internal(cellPos, playerPos, tileMap);
        results[tileOffset] = occluded;
    }
    
    return results;
}

bool WorldGeneration::is_occluded_internal(const Vector2i& cellPos, const Vector2i& playerPos, 
                                           const std::unordered_map<uint64_t, int>& tileMap) {
    // If the cell is at the same position as player, it's not occluded
    if (cellPos == playerPos) {
        return false;
    }
    
    // Check if the cell exists in tileMap
    uint64_t cellKey = pack_coords(cellPos.x, cellPos.y);
    auto cellIt = tileMap.find(cellKey);
    if (cellIt == tileMap.end()) {
        return false;
    }
    
    int tileY = cellIt->second;
    
    // Only process wall (53) or ground (27) tiles for occlusion
    if (tileY != 27 && tileY != 53) {
        return false;
    }
    
    // Use Bresenham's line algorithm to check line of sight
    int x0 = playerPos.x, y0 = playerPos.y;
    int x1 = cellPos.x, y1 = cellPos.y;
    
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    
    int x = x0, y = y0;
    
    while (true) {
        // Skip the starting position (player position)
        if (x != x0 || y != y0) {
            // If we've reached the target cell, we're done
            if (x == x1 && y == y1) {
                break;
            }
            
            // Check if current position has a wall tile (fast lookup)
            auto it = tileMap.find(pack_coords(x, y));
            if (it != tileMap.end()) {
                // If we encounter a wall tile before reaching the target, the target is occluded
                if (it->second == 53) {
                    return true;
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
