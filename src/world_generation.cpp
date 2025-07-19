#include "world_generation.h"

using namespace godot;

void WorldGeneration::_bind_methods() {
	ClassDB::bind_method(D_METHOD("update_occlusion", "playerPos", "tileData", "worldBubbleSize"), &WorldGeneration::update_occlusion);
}

WorldGeneration::WorldGeneration() {
}

WorldGeneration::~WorldGeneration() {
    
}

Dictionary WorldGeneration::update_occlusion(const Vector2i& playerPos, const Dictionary& tileData, const int& worldBubbleSize) {
    Dictionary visibilityData;
    int radius = worldBubbleSize / 2;
    
    // Iterate through a circle of cells with diameter worldBubbleSize
    for (int y = -radius; y < radius; y++) {
        for (int x = -radius; x < radius; x++) {
            // Check if this position is within the circle
            if (x * x + y * y <= radius * radius) {
                Vector2i cellPos = playerPos + Vector2i(x, y);
                Vector2i localPos = Vector2i(x + radius, y + radius); // Convert to local coordinates
                
                // Check if this cell exists in tileData
                if (tileData.has(cellPos)) {
                    Dictionary cellData = tileData[cellPos];
                    int tileY = cellData["tile_y"];
                    
                    // Only check wood tiles (tileY == 27) for occlusion
                    if (tileY == 27) {
                        // Check if this wood tile is occluded from player's perspective
                        if (is_cell_occluded(cellPos, playerPos, tileData)) {
                            visibilityData[localPos] = 0.25; // Occluded
                        } else {
                            visibilityData[localPos] = 1.0; // Visible
                        }
                    } else {
                        visibilityData[localPos] = 1.0; // Stone tiles are always visible
                    }
                } else {
                    visibilityData[localPos] = 0.0; // Never seen
                }
            }
        }
    }
    
    Dictionary result;
    result["visibility_data"] = visibilityData;
    
    return result;
}

bool WorldGeneration::is_cell_occluded(const Vector2i& cellPos, const Vector2i& playerPos, const Dictionary& tileData) {
    // If the cell is at the same position as player, it's not occluded
    if (cellPos == playerPos) {
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
            
            // Check if current position has a stone tile
            Vector2i currentPos(x, y);
            if (tileData.has(currentPos)) {
                Dictionary currentData = tileData[currentPos];
                int currentTileY = currentData["tile_y"];
                
                // If we encounter a stone tile before reaching the target, the target is occluded
                if (currentTileY == 53) {
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