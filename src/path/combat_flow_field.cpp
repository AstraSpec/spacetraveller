#include "combat_flow_field.h"

#include "core/world_coords.h"
#include "world/traversal_rules.h"
#include "world/world_bubble.h"

#include <algorithm>
#include <queue>
#include <unordered_map>

using namespace godot;

namespace {

struct FlowNode {
    Vector2i position;
    int cost = 0;

    bool operator>(const FlowNode& other) const {
        return cost > other.cost;
    }
};

static const Vector2i FLOW_DIRECTIONS[] = {
    Vector2i(0, 1), Vector2i(0, -1), Vector2i(1, 0), Vector2i(-1, 0),
    Vector2i(1, 1), Vector2i(1, -1), Vector2i(-1, 1), Vector2i(-1, -1)
};

bool in_bounds(const Vector2i& position, const Vector2i& target, int radius) {
    return position.x >= target.x - radius && position.x < target.x + radius &&
        position.y >= target.y - radius && position.y < target.y + radius;
}

}

bool CombatFlowField::build(
    WorldBubble& bubble,
    const Vector2i& target,
    int z,
    int p_radius,
    const String& p_profile_id,
    bool p_allow_openable_tiles,
    uint64_t p_terrain_revision
) {
    valid = false;
    next_steps.clear();
    target_position = target;
    target_z = z;
    radius = p_radius;
    profile_id = p_profile_id;
    allow_openable_tiles = p_allow_openable_tiles;
    terrain_revision = p_terrain_revision;

    if (radius <= 0 || profile_id.is_empty()) {
        return false;
    }

    auto can_enter = [&](const Vector2i& position) {
        const uint16_t tile_id = bubble.query_tile_id_at_z(position.x, position.y, target_z);
        return allow_openable_tiles
            ? TraversalRules::can_profile_enter_or_open(profile_id, tile_id)
            : TraversalRules::can_profile_enter(profile_id, tile_id);
    };

    if (!can_enter(target_position)) {
        return false;
    }

    std::priority_queue<FlowNode, std::vector<FlowNode>, std::greater<FlowNode>> open;
    std::unordered_map<uint64_t, int> distances;
    distances.reserve(static_cast<size_t>(radius * radius * 4));

    const uint64_t target_key = WorldCoords::pack_coords(target_position.x, target_position.y);
    distances[target_key] = 0;
    open.push({target_position, 0});

    while (!open.empty()) {
        const FlowNode current = open.top();
        open.pop();

        const uint64_t current_key = WorldCoords::pack_coords(current.position.x, current.position.y);
        auto current_distance = distances.find(current_key);
        if (current_distance == distances.end() || current.cost != current_distance->second) {
            continue;
        }

        for (const Vector2i& direction : FLOW_DIRECTIONS) {
            const Vector2i next = current.position + direction;
            if (!in_bounds(next, target_position, radius) || !can_enter(next)) {
                continue;
            }

            const int next_cost = current.cost + 1;
            const uint64_t next_key = WorldCoords::pack_coords(next.x, next.y);
            auto existing = distances.find(next_key);
            if (existing != distances.end() && existing->second <= next_cost) {
                continue;
            }

            distances[next_key] = next_cost;
            next_steps[next_key] = current.position;
            open.push({next, next_cost});
        }
    }

    valid = true;
    return true;
}

bool CombatFlowField::matches(
    const Vector2i& target,
    int z,
    int p_radius,
    const String& p_profile_id,
    bool p_allow_openable_tiles,
    uint64_t p_terrain_revision
) const {
    return valid && target_position == target && target_z == z && radius == p_radius &&
        profile_id == p_profile_id && allow_openable_tiles == p_allow_openable_tiles &&
        terrain_revision == p_terrain_revision;
}

bool CombatFlowField::get_step(const Vector2i& position, Vector2i& out_step) const {
    const uint64_t key = WorldCoords::pack_coords(position.x, position.y);
    auto it = next_steps.find(key);
    if (it == next_steps.end()) {
        return false;
    }
    out_step = it->second;
    return true;
}
