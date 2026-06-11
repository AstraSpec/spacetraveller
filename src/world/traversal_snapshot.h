#ifndef SPACETRAVELLER_TRAVERSAL_SNAPSHOT_H
#define SPACETRAVELLER_TRAVERSAL_SNAPSHOT_H

#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/string.hpp>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace godot {

class WorldBubble;
class EntityLedger;

class TraversalSnapshot {
    static constexpr uint32_t INVALID_ENTITY_ID = UINT32_MAX;

    WorldBubble* bubble = nullptr;
    const EntityLedger* ledger = nullptr;
    uint32_t entity_id = INVALID_ENTITY_ID;
    String traversal_profile;
    bool allow_openable_tiles = false;
    Vector2i start;
    Vector2i goal;
    std::unordered_set<uint64_t> blocking_cells;
    mutable std::unordered_map<uint64_t, bool> walkable_cache;

    bool compute_walkable(int x, int y) const;

public:
    TraversalSnapshot() = default;
    TraversalSnapshot(
        WorldBubble* p_bubble,
        const Vector2i& p_start,
        const Vector2i& p_goal,
        const std::vector<Vector2i>& p_blocking_positions,
        const EntityLedger* p_ledger = nullptr,
        uint32_t p_entity_id = INVALID_ENTITY_ID,
        const String& p_traversal_profile = "",
        bool p_allow_openable_tiles = false
    );

    bool is_walkable(int x, int y) const;
    int movement_cost(int x, int y) const;
};

}

#endif // SPACETRAVELLER_TRAVERSAL_SNAPSHOT_H
