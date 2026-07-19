#include "mine_generator.h"

#include "core/rng.h"
#include "core/world_coords.h"
#include "data/dungeon_db.h"
#include "data/feature_db.h"
#include "data/structure_db.h"

#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace godot {

namespace {

constexpr uint64_t MINE_LAYOUT_SALT = 0x4D494E454C41594FULL; // "MINELAYO"
constexpr int NODE_CLEARANCE = 7;
constexpr int ROOM_PADDING = 2;

struct MineNode {
    Vector2i pos;
    int degree = 0;
};

struct MineSegment {
    Vector2i a;
    Vector2i b;
    int width = 3;
};

int manhattan_distance(const Vector2i& p_a, const Vector2i& p_b) {
    return std::abs(p_a.x - p_b.x) + std::abs(p_a.y - p_b.y);
}

int distance_sq(const Vector2i& p_a, const Vector2i& p_b) {
    const int dx = p_a.x - p_b.x;
    const int dy = p_a.y - p_b.y;
    return dx * dx + dy * dy;
}

Vector2i rotated_size(const Vector2i& p_size, uint8_t p_rotation) {
    if (p_rotation == WorldCoords::ROT_WEST || p_rotation == WorldCoords::ROT_EAST) {
        return Vector2i(p_size.y, p_size.x);
    }
    return p_size;
}

void expand_bounds(DungeonRect& r_bounds, bool& r_initialized, const DungeonRect& p_rect) {
    if (!r_initialized) {
        r_bounds = p_rect;
        r_initialized = true;
        return;
    }

    const int min_x = std::min(r_bounds.origin.x, p_rect.origin.x);
    const int min_y = std::min(r_bounds.origin.y, p_rect.origin.y);
    const int max_x = std::max(r_bounds.origin.x + r_bounds.size.x, p_rect.origin.x + p_rect.size.x);
    const int max_y = std::max(r_bounds.origin.y + r_bounds.size.y, p_rect.origin.y + p_rect.size.y);
    r_bounds.origin = Vector2i(min_x, min_y);
    r_bounds.size = Vector2i(max_x - min_x, max_y - min_y);
}

bool rect_overlaps_features(const DungeonLayout& p_layout, const DungeonRect& p_rect, int p_padding) {
    for (const PlacedDungeonRoom& placed : p_layout.rooms) {
        if (DungeonGenerator::rects_overlap(placed.bounds, p_rect, p_padding)) return true;
    }
    return false;
}

void push_unique_cell(DungeonCorridor& r_corridor, const Vector2i& p_cell) {
    for (const Vector2i& existing : r_corridor.cells) {
        if (existing == p_cell) return;
    }
    r_corridor.cells.push_back(p_cell);
}

void carve_segment(DungeonLayout& r_layout, const MineSegment& p_segment) {
    DungeonCorridor corridor;
    const int low = -(p_segment.width / 2);
    const int high = low + p_segment.width - 1;

    if (p_segment.a.y == p_segment.b.y) {
        const int min_x = std::min(p_segment.a.x, p_segment.b.x);
        const int max_x = std::max(p_segment.a.x, p_segment.b.x);
        for (int x = min_x; x <= max_x; x++) {
            for (int offset = low; offset <= high; offset++) {
                push_unique_cell(corridor, Vector2i(x, p_segment.a.y + offset));
            }
        }
    } else {
        const int min_y = std::min(p_segment.a.y, p_segment.b.y);
        const int max_y = std::max(p_segment.a.y, p_segment.b.y);
        for (int y = min_y; y <= max_y; y++) {
            for (int offset = low; offset <= high; offset++) {
                push_unique_cell(corridor, Vector2i(p_segment.a.x + offset, y));
            }
        }
    }

    for (const Vector2i& cell : corridor.cells) {
        r_layout.corridor_cells.insert(WorldCoords::pack_coords(cell.x, cell.y));
    }
    r_layout.corridors.push_back(std::move(corridor));
}

bool add_feature_from_pool(
    DungeonLayout& r_layout,
    const String& p_pool_id,
    const Vector2i& p_center,
    Rng::Seeded& r_rng,
    int p_padding
) {
    if (p_pool_id.is_empty()) return false;

    FeatureDb* feature_db = FeatureDb::get_singleton();
    StructureDb* structure_db = StructureDb::get_singleton();
    const FeaturePoolInfo* pool = feature_db ? feature_db->get_feature_pool(p_pool_id) : nullptr;
    const FeatureEntryInfo* entry = pool ? feature_db->pick_weighted_entry(*pool, r_rng) : nullptr;
    if (!entry || entry->structure_id.is_empty() || !structure_db) return false;

    const StructureInfo* structure = structure_db->get_structure_info(entry->structure_id);
    if (!structure || structure->type != "feature" || structure->size.x <= 0 || structure->size.y <= 0) {
        UtilityFunctions::push_error("[MineGenerator] Invalid mine feature: ", entry->structure_id);
        return false;
    }

    const uint8_t rotation = structure->placement_rotations.empty()
        ? WorldCoords::ROT_SOUTH
        : structure->placement_rotations[r_rng.range(0, static_cast<int>(structure->placement_rotations.size()) - 1)];
    const Vector2i placed_size = rotated_size(structure->size, rotation);
    DungeonRect bounds{
        Vector2i(p_center.x - placed_size.x / 2, p_center.y - placed_size.y / 2),
        placed_size
    };
    const DungeonRect entrance_bounds{
        Vector2i(
            r_layout.entrance_chunk.x * WorldCoords::CHUNK_SIZE,
            r_layout.entrance_chunk.y * WorldCoords::CHUNK_SIZE
        ),
        Vector2i(WorldCoords::CHUNK_SIZE, WorldCoords::CHUNK_SIZE)
    };
    if (DungeonGenerator::rects_overlap(entrance_bounds, bounds, p_padding)) return false;
    if (rect_overlaps_features(r_layout, bounds, p_padding)) return false;

    PlacedDungeonRoom placed;
    placed.bounds = bounds;
    placed.structure_id = entry->structure_id;
    placed.rotation = rotation;
    r_layout.rooms.push_back(placed);
    return true;
}

}

DungeonLayout MineGenerator::build_layout(
    const DungeonInfo& p_info,
    const Vector2i& p_entrance_chunk,
    int p_world_seed
) {
    DungeonLayout layout;
    layout.dungeon_type = p_info.id;
    layout.z = p_info.start_z;
    layout.floor_tile_id = p_info.floor_tile;
    layout.wall_tile_id = p_info.wall_tile;
    layout.natural_walls = true;
    layout.entrance_chunk = p_entrance_chunk;

    Rng::Seeded rng = Rng::at(
        static_cast<uint32_t>(p_world_seed), p_entrance_chunk, Rng::BIOME, MINE_LAYOUT_SALT
    );
    const Vector2i entrance_origin(
        p_entrance_chunk.x * WorldCoords::CHUNK_SIZE,
        p_entrance_chunk.y * WorldCoords::CHUNK_SIZE
    );
    const Vector2i entrance_center = entrance_origin + Vector2i(WorldCoords::CHUNK_SIZE / 2, WorldCoords::CHUNK_SIZE / 2);
    const int radius = p_info.radius_chunks * WorldCoords::CHUNK_SIZE;

    static const Vector2i directions[4] = {
        Vector2i(0, -1),
        Vector2i(1, 0),
        Vector2i(0, 1),
        Vector2i(-1, 0)
    };

    std::vector<MineNode> nodes;
    std::vector<MineSegment> segments;
    nodes.push_back(MineNode{entrance_center, 0});

    auto inside_radius = [&](const Vector2i& p_pos) {
        return std::abs(p_pos.x - entrance_center.x) <= radius &&
            std::abs(p_pos.y - entrance_center.y) <= radius;
    };
    auto endpoint_is_clear = [&](const Vector2i& p_pos) {
        for (const MineNode& node : nodes) {
            if (manhattan_distance(node.pos, p_pos) < NODE_CLEARANCE) return false;
        }
        return true;
    };
    auto try_add_segment = [&](int p_parent, int p_direction, int p_width) -> int {
        if (p_parent < 0 || p_parent >= static_cast<int>(nodes.size())) return -1;
        const int length = rng.range(p_info.segment_length_min, p_info.segment_length_max);
        const Vector2i end = nodes[p_parent].pos + directions[p_direction] * length;
        if (!inside_radius(end) || !endpoint_is_clear(end)) return -1;

        const int child = static_cast<int>(nodes.size());
        nodes.push_back(MineNode{end, 1});
        nodes[p_parent].degree++;
        segments.push_back(MineSegment{nodes[p_parent].pos, end, p_width});
        return child;
    };

    const int target_segments = rng.range(p_info.segment_count_min, p_info.segment_count_max);
    const int main_target = std::max(4, static_cast<int>(target_segments * (1.0f - p_info.branch_chance)));
    int current_node = 0;
    int current_direction = 2; // The entrance landing opens south.
    for (int i = 0; i < main_target; i++) {
        bool added = false;
        for (int attempt = 0; attempt < 8 && !added; attempt++) {
            int direction = current_direction;
            if (i > 0 && rng.range(0, 99) >= 58) {
                direction = (current_direction + (rng.chance(0.5f) ? 1 : 3)) % 4;
            }
            const int child = try_add_segment(current_node, direction, p_info.main_width);
            if (child >= 0) {
                current_node = child;
                current_direction = direction;
                added = true;
            }
        }
        if (!added) break;
    }

    int attempts = 0;
    while (static_cast<int>(segments.size()) < target_segments && attempts++ < target_segments * 24) {
        // Node zero is inside the authored landing. Only the main south tunnel
        // may leave it, otherwise side branches are sealed behind its walls.
        if (nodes.size() <= 1) break;
        const int parent = rng.range(1, static_cast<int>(nodes.size()) - 1);
        if (nodes[parent].degree >= 3) continue;
        const int direction = rng.range(0, 3);
        try_add_segment(parent, direction, p_info.branch_width);
    }

    // A few short orthogonal reconnections turn the branching tree into a mine
    // with navigable loops without losing its engineered grid character.
    const int loop_attempts = static_cast<int>(nodes.size() * p_info.loop_chance);
    for (int i = 0; i < loop_attempts; i++) {
        const int a_index = rng.range(1, static_cast<int>(nodes.size()) - 1);
        const int b_index = rng.range(1, static_cast<int>(nodes.size()) - 1);
        if (a_index == b_index) continue;
        const Vector2i a = nodes[a_index].pos;
        const Vector2i b = nodes[b_index].pos;
        const int distance = manhattan_distance(a, b);
        if (distance < p_info.segment_length_min || distance > p_info.segment_length_max * 2) continue;

        const Vector2i bend = rng.chance(0.5f) ? Vector2i(b.x, a.y) : Vector2i(a.x, b.y);
        if (!inside_radius(bend)) continue;
        if (a != bend) segments.push_back(MineSegment{a, bend, p_info.branch_width});
        if (bend != b) segments.push_back(MineSegment{bend, b, p_info.branch_width});
        nodes[a_index].degree++;
        nodes[b_index].degree++;
    }

    for (const MineSegment& segment : segments) {
        carve_segment(layout, segment);
    }

    // Place the final working at the most distant dead end.
    int end_index = -1;
    int end_distance_sq = -1;
    for (int i = 1; i < static_cast<int>(nodes.size()); i++) {
        if (nodes[i].degree > 1) continue;
        const int candidate_distance = distance_sq(nodes[i].pos, entrance_center);
        if (candidate_distance > end_distance_sq) {
            end_index = i;
            end_distance_sq = candidate_distance;
        }
    }
    if (end_index >= 0) {
        add_feature_from_pool(layout, p_info.end_feature_pool, nodes[end_index].pos, rng, ROOM_PADDING);
    }

    std::vector<int> room_candidates;
    for (int i = 1; i < static_cast<int>(nodes.size()); i++) {
        if (i == end_index) continue;
        if (nodes[i].degree == 1 || nodes[i].degree >= 3 || rng.chance(0.25f)) {
            room_candidates.push_back(i);
        }
    }
    const int room_target = rng.range(p_info.feature_room_count_min, p_info.feature_room_count_max);
    int rooms_placed = 0;
    while (rooms_placed < room_target && !room_candidates.empty()) {
        const int pick = rng.range(0, static_cast<int>(room_candidates.size()) - 1);
        const int node_index = room_candidates[pick];
        room_candidates.erase(room_candidates.begin() + pick);
        if (add_feature_from_pool(layout, p_info.room_feature_pool, nodes[node_index].pos, rng, ROOM_PADDING)) {
            rooms_placed++;
        }
    }

    // Support frames are ordinary feature structures placed across three-wide
    // haulage tunnels. Narrow exploratory branches remain rough and unsupported.
    FeatureDb* feature_db = FeatureDb::get_singleton();
    StructureDb* structure_db = StructureDb::get_singleton();
    const FeaturePoolInfo* support_pool = feature_db
        ? feature_db->get_feature_pool(p_info.support_feature_pool)
        : nullptr;
    for (const MineSegment& segment : segments) {
        if (segment.width != 3 || !support_pool || !structure_db) continue;
        const bool horizontal = segment.a.y == segment.b.y;
        const int length = manhattan_distance(segment.a, segment.b);
        int offset = rng.range(p_info.support_spacing_min, p_info.support_spacing_max);
        while (offset < length - 2) {
            const int step_x = segment.b.x == segment.a.x ? 0 : (segment.b.x > segment.a.x ? 1 : -1);
            const int step_y = segment.b.y == segment.a.y ? 0 : (segment.b.y > segment.a.y ? 1 : -1);
            const Vector2i center = segment.a + Vector2i(step_x * offset, step_y * offset);
            const FeatureEntryInfo* entry = feature_db->pick_weighted_entry(*support_pool, rng);
            const StructureInfo* structure = entry && !entry->structure_id.is_empty()
                ? structure_db->get_structure_info(entry->structure_id)
                : nullptr;
            if (structure && structure->type == "feature") {
                const uint8_t rotation = horizontal ? WorldCoords::ROT_WEST : WorldCoords::ROT_SOUTH;
                const Vector2i size = rotated_size(structure->size, rotation);
                DungeonRect bounds{Vector2i(center.x - size.x / 2, center.y - size.y / 2), size};
                if (!rect_overlaps_features(layout, bounds, 0)) {
                    PlacedDungeonRoom support;
                    support.bounds = bounds;
                    support.structure_id = entry->structure_id;
                    support.rotation = rotation;
                    layout.rooms.push_back(support);
                }
            }
            offset += rng.range(p_info.support_spacing_min, p_info.support_spacing_max);
        }
    }

    bool bounds_initialized = false;
    expand_bounds(
        layout.bounds,
        bounds_initialized,
        DungeonRect{entrance_origin, Vector2i(WorldCoords::CHUNK_SIZE, WorldCoords::CHUNK_SIZE)}
    );
    for (const DungeonCorridor& corridor : layout.corridors) {
        for (const Vector2i& cell : corridor.cells) {
            expand_bounds(layout.bounds, bounds_initialized, DungeonRect{cell, Vector2i(1, 1)});
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    const uint64_t key = WorldCoords::pack_coords(cell.x + dx, cell.y + dy);
                    if (layout.corridor_cells.count(key) == 0) layout.corridor_wall_cells.insert(key);
                }
            }
        }
    }
    for (const PlacedDungeonRoom& room : layout.rooms) {
        expand_bounds(layout.bounds, bounds_initialized, room.bounds);
    }

    return layout;
}

}
