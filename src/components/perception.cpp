#include "perception.h"
#include "world/world_bubble.h"
#include "data/tile_db.h"

using namespace godot;

static bool is_tile_solid(int x, int y, const WorldBubble& bubble, const TileDb& tile_db) {
    uint16_t tile_id = const_cast<WorldBubble&>(bubble).query_tile_id(x, y);
    if (tile_id == 0) return false;
    const TileInfo* info = tile_db.get_tile_info(tile_id);
    return info && info->solid;
}

bool Perception::has_line_of_sight(int x1, int y1, int x2, int y2,
                                    const WorldBubble& bubble, const TileDb& tile_db) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;

    int cx = x1, cy = y1;
    while (cx != x2 || cy != y2) {
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; cx += sx; }
        if (e2 < dx) { err += dx; cy += sy; }

        if (cx == x2 && cy == y2) break;

        if (is_tile_solid(cx, cy, bubble, tile_db)) {
            return false;
        }
    }
    return true;
}
