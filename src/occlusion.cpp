#include "occlusion.h"
#include "data/tile_db.h"
#include <cmath>

namespace godot {

static inline bool is_opaque(
    int x, int y,
    const std::unordered_map<uint64_t, uint16_t>& tile_cache
) {
    auto it = tile_cache.find(Occlusion::pack_coords(x, y));
    if (it == tile_cache.end()) {
        return false;
    }
    TileDb* tile_db = TileDb::get_singleton();
    if (!tile_db) {
        return false;
    }
    const TileInfo* info = tile_db->get_tile_info(it->second);
    return info && info->solid;
}

// RogueBasin recursive shadowcasting (C++ reference implementation).
static void cast_light(
    int cx, int cy,
    int radius, int row,
    float start_slope, float end_slope,
    int xx, int xy, int yx, int yy,
    const std::unordered_map<uint64_t, uint16_t>& tile_cache,
    std::unordered_set<uint64_t>& visible_keys
) {
    if (start_slope < end_slope) {
        return;
    }

    const int radius2 = radius * radius;
    float next_start_slope = start_slope;

    for (int dist = row; dist <= radius; ++dist) {
        bool blocked = false;
        const int dy = -dist;

        for (int dx = -dist; dx <= 0; ++dx) {
            const float left_slope = (dx - 0.5f) / (dy + 0.5f);
            const float right_slope = (dx + 0.5f) / (dy - 0.5f);

            if (start_slope < right_slope) {
                continue;
            }
            if (end_slope > left_slope) {
                break;
            }

            const int map_x = cx + dx * xx + dy * xy;
            const int map_y = cy + dx * yx + dy * yy;

            if (dx * dx + dy * dy < radius2) {
                visible_keys.insert(Occlusion::pack_coords(map_x, map_y));
            }

            if (blocked) {
                if (is_opaque(map_x, map_y, tile_cache)) {
                    next_start_slope = right_slope;
                    continue;
                }
                blocked = false;
                start_slope = next_start_slope;
            } else if (is_opaque(map_x, map_y, tile_cache)) {
                blocked = true;
                next_start_slope = right_slope;
                cast_light(cx, cy, radius, dist + 1, start_slope, left_slope,
                           xx, xy, yx, yy, tile_cache, visible_keys);
            }
        }

        if (blocked) {
            break;
        }
    }
}

void Occlusion::compute_visible(
    const Vector2i& playerPos,
    int radius,
    const std::unordered_map<uint64_t, uint16_t>& tile_cache,
    std::unordered_set<uint64_t>& visible_keys
) {
    visible_keys.clear();
    if (radius <= 0) {
        visible_keys.insert(Occlusion::pack_coords(playerPos.x, playerPos.y));
        return;
    }

    visible_keys.reserve(static_cast<size_t>(radius) * static_cast<size_t>(radius) * 4);
    visible_keys.insert(Occlusion::pack_coords(playerPos.x, playerPos.y));

    static const int multipliers[4][8] = {
        {1, 0, 0, -1, -1, 0, 0, 1},
        {0, 1, -1, 0, 0, -1, 1, 0},
        {0, 1, 1, 0, 0, -1, -1, 0},
        {1, 0, 0, 1, -1, 0, 0, -1}
    };

    for (int oct = 0; oct < 8; ++oct) {
        cast_light(
            playerPos.x, playerPos.y,
            radius, 1,
            1.0f, 0.0f,
            multipliers[0][oct], multipliers[1][oct],
            multipliers[2][oct], multipliers[3][oct],
            tile_cache, visible_keys);
    }
}

}
