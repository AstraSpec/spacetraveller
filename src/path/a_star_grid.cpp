#include "a_star_grid.h"
#include "world/traversal_snapshot.h"
#include "core/world_coords.h"
#include <queue>
#include <unordered_map>
#include <algorithm>

using namespace godot;

namespace {

struct AStarNode {
    Vector2i pos;
    int g = 0;
    int h = 0;
    int f() const { return g + h; }

    bool operator>(const AStarNode& other) const {
        if (f() != other.f()) return f() > other.f();
        if (h != other.h) return h > other.h;
        return g < other.g;
    }
};

} // namespace

PathResult AStarGridPathfinder::find_path(const PathRequest& request, const TraversalSnapshot& traversal) const {
    PathResult result;
    const Vector2i& start = request.start;
    const Vector2i& end = request.goal;

    const int goal_radius = std::max(0, request.goal_radius);
    auto goal_reached = [goal_radius, &end](const Vector2i& pos) {
        return std::max(std::abs(pos.x - end.x), std::abs(pos.y - end.y)) <= goal_radius;
    };

    if (goal_reached(start)) {
        result.found = true;
        return result;
    }

    if (goal_radius == 0 && !traversal.is_walkable(end.x, end.y)) {
        return result;
    }

    const bool allow_diagonal = request.allow_diagonal();

    auto heuristic = [allow_diagonal, goal_radius](const Vector2i& a, const Vector2i& b) {
        const int dx = std::max(0, std::abs(a.x - b.x) - goal_radius);
        const int dy = std::max(0, std::abs(a.y - b.y) - goal_radius);
        if (!allow_diagonal) return 10 * (dx + dy);
        const int diagonal = std::min(dx, dy);
        const int straight = std::max(dx, dy) - diagonal;
        return 14 * diagonal + 10 * straight;
    };

    std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> open;
    std::unordered_map<uint64_t, int> g_scores;
    std::unordered_map<uint64_t, Vector2i> parents;

    const uint64_t start_key = WorldCoords::pack_coords(start.x, start.y);
    open.push({start, 0, heuristic(start, end)});
    g_scores[start_key] = 0;

    static const Vector2i dirs_4[] = {
        Vector2i(0, 1), Vector2i(0, -1), Vector2i(1, 0), Vector2i(-1, 0)
    };
    static const Vector2i dirs_8[] = {
        Vector2i(0, 1), Vector2i(0, -1), Vector2i(1, 0), Vector2i(-1, 0),
        Vector2i(1, 1), Vector2i(1, -1), Vector2i(-1, 1), Vector2i(-1, -1)
    };
    const Vector2i* dirs = allow_diagonal ? dirs_8 : dirs_4;
    const int dir_count = allow_diagonal ? 8 : 4;

    constexpr int MAX_ITERATIONS = 4096;
    int iterations = 0;

    while (!open.empty() && iterations++ < MAX_ITERATIONS) {
        AStarNode current = open.top();
        open.pop();

        if (goal_reached(current.pos)) {
            Vector2i curr = current.pos;
            while (curr != start) {
                result.waypoints.push_back(curr);
                const uint64_t key = WorldCoords::pack_coords(curr.x, curr.y);
                auto parent_it = parents.find(key);
                if (parent_it == parents.end()) {
                    break;
                }
                curr = parent_it->second;
            }
            std::reverse(result.waypoints.begin(), result.waypoints.end());
            result.found = true;
            return result;
        }

        const uint64_t current_key = WorldCoords::pack_coords(current.pos.x, current.pos.y);
        if (current.g > g_scores[current_key]) {
            continue;
        }

        for (int i = 0; i < dir_count; ++i) {
            const Vector2i& d = dirs[i];
            const Vector2i next = current.pos + d;
            if (!traversal.is_walkable(next.x, next.y)) {
                continue;
            }

            const bool diagonal = d.x != 0 && d.y != 0;
            if (diagonal && (!traversal.is_walkable(current.pos.x + d.x, current.pos.y) ||
                             !traversal.is_walkable(current.pos.x, current.pos.y + d.y))) {
                continue;
            }

            const int terrain_cost = traversal.movement_cost(next.x, next.y);
            if (terrain_cost <= 0) {
                continue;
            }
            const int step_cost = terrain_cost * (diagonal ? 14 : 10);

            const uint64_t next_key = WorldCoords::pack_coords(next.x, next.y);
            const int new_g = current.g + step_cost;

            auto score_it = g_scores.find(next_key);
            if (score_it != g_scores.end() && new_g >= score_it->second) {
                continue;
            }

            g_scores[next_key] = new_g;
            parents[next_key] = current.pos;
            open.push({next, new_g, heuristic(next, end)});
        }
    }

    return result;
}
