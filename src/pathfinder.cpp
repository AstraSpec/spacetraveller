#include "pathfinder.h"
#include <godot_cpp/core/class_db.hpp>
#include <queue>
#include <unordered_map>
#include <cmath>
#include "data/tile_db.h"
#include "occlusion.h"

using namespace godot;

struct AStarNode {
    Vector2i pos;
    int g; // cost from start
    int h; // heuristic cost to end
    int f() const { return g + h; }

    bool operator>(const AStarNode& other) const {
        return f() > other.f();
    }
};

void Pathfinder::_bind_methods() {
    ClassDB::bind_static_method("Pathfinder", D_METHOD("find_path", "tilemap", "start", "end"), &Pathfinder::find_path);
    ClassDB::bind_static_method("Pathfinder", D_METHOD("is_walkable", "tilemap", "pos"), &Pathfinder::is_walkable);
}

Pathfinder::Pathfinder() {}
Pathfinder::~Pathfinder() {}

bool Pathfinder::is_walkable(FastTileMap* p_tilemap, const Vector2i& p_pos) {
    if (!p_tilemap) return false;
    String tid = p_tilemap->get_tile_at(p_pos.x, p_pos.y);
    if (tid == "void") return true; // Assuming void is walkable

    TileDb* tile_db = TileDb::get_singleton();
    if (!tile_db) return false;
    
    return !tile_db->is_solid(tid);
}

Array Pathfinder::find_path(FastTileMap* p_tilemap, const Vector2i& p_start, const Vector2i& p_end) {
    Array res;
    if (!p_tilemap || p_start == p_end) return res;
    if (!is_walkable(p_tilemap, p_end)) return res;

    std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> open;
    std::unordered_map<uint64_t, int> g_scores;
    std::unordered_map<uint64_t, Vector2i> parents;

    auto heuristic = [](const Vector2i& a, const Vector2i& b) {
        return std::abs(a.x - b.x) + std::abs(a.y - b.y);
    };

    uint64_t start_key = Occlusion::pack_coords(p_start.x, p_start.y);
    open.push({p_start, 0, heuristic(p_start, p_end)});
    g_scores[start_key] = 0;

    Vector2i dirs[] = {Vector2i(0, 1), Vector2i(0, -1), Vector2i(1, 0), Vector2i(-1, 0)};

    int max_iterations = 1000; // safety break
    int iterations = 0;

    while (!open.empty() && iterations++ < max_iterations) {
        AStarNode current = open.top();
        open.pop();

        if (current.pos == p_end) {
            // Reconstruct path
            Vector2i curr = p_end;
            while (curr != p_start) {
                res.push_front(curr);
                uint64_t key = Occlusion::pack_coords(curr.x, curr.y);
                if (parents.find(key) == parents.end()) break;
                curr = parents[key];
            }
            return res;
        }

        uint64_t current_key = Occlusion::pack_coords(current.pos.x, current.pos.y);
        if (current.g > g_scores[current_key]) continue;

        for (const Vector2i& d : dirs) {
            Vector2i next = current.pos + d;
            if (!is_walkable(p_tilemap, next)) continue;

            uint64_t next_key = Occlusion::pack_coords(next.x, next.y);
            int new_g = current.g + 1;

            if (g_scores.find(next_key) == g_scores.end() || new_g < g_scores[next_key]) {
                g_scores[next_key] = new_g;
                parents[next_key] = current.pos;
                open.push({next, new_g, heuristic(next, p_end)});
            }
        }
    }

    return res;
}
