#include "perception.h"
#include "entities/entity.h"
#include "world/world_bubble.h"
#include "data/tile_db.h"
#include "core/world_coords.h"
#include "occlusion.h"

using namespace godot;

static bool is_tile_solid(int x, int y, const WorldBubble& bubble, const TileDb& tile_db) {
    uint16_t tile_id = const_cast<WorldBubble&>(bubble).query_tile_id(x, y);
    if (tile_id == 0) return false;
    const TileInfo* info = tile_db.get_tile_info(tile_id);
    return info && info->solid;
}

void Perception::tick_full(PerceptionMemory& mem, const Entity& self,
                            const WorldBubble& bubble, const Vector2i& player_pos) {
    mem.known_tiles.insert(WorldCoords::pack_coords(self.x, self.y));

    mem.player_seen = false;

    int radius = bubble.get_world_bubble_radius();
    std::unordered_set<uint64_t> visible_set;
    Occlusion::compute_visible(
        Vector2i(self.x, self.y),
        radius,
        bubble.get_tile_cache(WorldBubble::LAYER_TILE),
        visible_set
    );

    for (uint64_t key : visible_set) {
        mem.known_tiles.insert(key);
    }

    uint64_t player_key = WorldCoords::pack_coords(player_pos.x, player_pos.y);
    if (visible_set.count(player_key)) {
        mem.player_seen = true;
        mem.last_known_player_pos = player_pos;
    }
}

void Perception::tick_raycast(PerceptionMemory& mem, const Entity& self,
                               const Vector2i& target_pos,
                               const WorldBubble& bubble, const TileDb& tile_db) {
    mem.known_tiles.insert(WorldCoords::pack_coords(self.x, self.y));

    mem.player_seen = false;

    int x1 = self.x, y1 = self.y;
    int x2 = target_pos.x, y2 = target_pos.y;
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

        if (cx == x2 && cy == y2) {
            mem.player_seen = true;
            mem.last_known_player_pos = target_pos;
            break;
        }

        uint64_t key = WorldCoords::pack_coords(cx, cy);
        mem.known_tiles.insert(key);

        if (is_tile_solid(cx, cy, bubble, tile_db)) {
            break;
        }
    }
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
