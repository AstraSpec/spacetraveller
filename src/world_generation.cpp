#include "world_generation.h"

using namespace godot;

void WorldGeneration::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_occluded", "cellPos", "playerPos", "tileData"), &WorldGeneration::is_occluded);
}

WorldGeneration::WorldGeneration() {
}

WorldGeneration::~WorldGeneration() {
    
}

bool WorldGeneration::is_occluded(const Vector2i& cellPos, const Vector2i& playerPos, const Dictionary& tileData) {
    // If the cell is at the same position as player, it's not occluded
    if (cellPos == playerPos) {
        return false;
    }
    
    // Check if the cell exists in tileData
    if (!tileData.has(cellPos)) {
        return false;
    }
    
    Dictionary cellData = tileData[cellPos];
    int tileY = cellData["tile_y"];
    
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
            
            // Check if current position has a wall tile
            Vector2i currentPos(x, y);
            if (tileData.has(currentPos)) {
                Dictionary currentData = tileData[currentPos];
                int currentTileY = currentData["tile_y"];
                
                // If we encounter a wall tile before reaching the target, the target is occluded
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