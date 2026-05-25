#ifndef SPACETRAVELLER_TRAVERSAL_SNAPSHOT_H
#define SPACETRAVELLER_TRAVERSAL_SNAPSHOT_H

#include <godot_cpp/variant/vector2i.hpp>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace godot {

class WorldBubble;

class TraversalSnapshot {
    WorldBubble* bubble = nullptr;
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
        const std::vector<Vector2i>& p_blocking_positions
    );

    bool is_walkable(int x, int y) const;
    int movement_cost(int x, int y) const;
};

}

#endif // SPACETRAVELLER_TRAVERSAL_SNAPSHOT_H