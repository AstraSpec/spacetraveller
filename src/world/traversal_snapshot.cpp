#include "traversal_snapshot.h"
#include "world_bubble.h"
#include "entities/entity_ledger.h"
#include "world/traversal_rules.h"
#include "core/world_coords.h"

using namespace godot;

TraversalSnapshot::TraversalSnapshot(
    WorldBubble* p_bubble,
    const Vector2i& p_start,
    const EntityLedger* p_ledger,
    uint32_t p_entity_id,
    const String& p_traversal_profile,
    bool p_allow_openable_tiles
) : bubble(p_bubble),
    ledger(p_ledger),
    entity_id(p_entity_id),
    traversal_profile(p_traversal_profile),
    allow_openable_tiles(p_allow_openable_tiles),
    start(p_start) {}

bool TraversalSnapshot::compute_walkable(int x, int y) const {
    if (!bubble) return false;

    const uint16_t tile_id = bubble->query_tile_id(x, y);
    if (ledger && entity_id != INVALID_ENTITY_ID) {
        return allow_openable_tiles
            ? TraversalRules::can_enter_or_open(entity_id, tile_id, *ledger)
            : TraversalRules::can_enter(entity_id, tile_id, *ledger);
    }
    if (!traversal_profile.is_empty()) {
        return allow_openable_tiles
            ? TraversalRules::can_profile_enter_or_open(traversal_profile, tile_id)
            : TraversalRules::can_profile_enter(traversal_profile, tile_id);
    }
    return allow_openable_tiles
        ? TraversalRules::can_profile_enter_or_open("walker", tile_id)
        : TraversalRules::can_profile_enter("walker", tile_id);
}

bool TraversalSnapshot::is_walkable(int x, int y) const {
    if (x == start.x && y == start.y) return true;

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
