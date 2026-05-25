#include "traversal_snapshot.h"
#include "world_bubble.h"
#include "core/world_coords.h"
#include "data/tile_db.h"

using namespace godot;

TraversalSnapshot::TraversalSnapshot(
    WorldBubble* p_bubble,
    const Vector2i& p_start,
    const Vector2i& p_goal,
    const std::vector<Vector2i>& p_blocking_positions
) : bubble(p_bubble), start(p_start), goal(p_goal) {
    for (const Vector2i& pos : p_blocking_positions) {
        if (pos == start || pos == goal) continue;
        blocking_cells.insert(WorldCoords::pack_coords(pos.x, pos.y));
    }
}

bool TraversalSnapshot::compute_walkable(int x, int y) const {
    if (!bubble) return false;

    const uint64_t cell_key = WorldCoords::pack_coords(x, y);
    if (blocking_cells.count(cell_key) > 0) return false;

    const uint16_t tile_id = bubble->query_tile_id(x, y);
    if (tile_id == 0) return true;

    TileDb* tile_db = TileDb::get_singleton();
    if (!tile_db) return false;

    const TileInfo* info = tile_db->get_tile_info(tile_id);
    return info && !info->solid;
}

bool TraversalSnapshot::is_walkable(int x, int y) const {
    if (x == start.x && y == start.y) return true;
    if (x == goal.x && y == goal.y) return true;

    const uint64_t key = WorldCoords::pack_coords(x, y);
    auto it = walkable_cache.find(key);
    if (it != walkable_cache.end()) return it->second;

    const bool walkable = compute_walkable(x, y);
    walkable_cache[key] = walkable;
    return walkable;
}

int TraversalSnapshot::movement_cost(int x, int y) const {
    return is_walkable(x, y) ? 1 : 0;
}