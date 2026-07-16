#include "cave_generator.h"
#include "core/rng.h"
#include "core/world_coords.h"
#include "data/dungeon_db.h"
#include "data/feature_db.h"
#include "data/structure_db.h"
#include <algorithm>
#include <array>
#include <cstdlib>

namespace godot {

namespace {

static constexpr uint64_t CAVE_LAYOUT_SALT = 0x434156454C41594FULL; // "CAVELAYO"
static constexpr int CAVE_ATTEMPTS_PER_CHAMBER = 28;

struct CaveChamber {
    Vector2i center;
    int radius_x = 3;
    int radius_y = 3;
    int parent_index = -1;
    int child_count = 0;
    int path_distance = 0;
};

static uint64_t cave_cell_key(int p_x, int p_y) {
    return WorldCoords::pack_coords(p_x, p_y);
}

static int distance_sq(const Vector2i& p_a, const Vector2i& p_b) {
    const int dx = p_a.x - p_b.x;
    const int dy = p_a.y - p_b.y;
    return dx * dx + dy * dy;
}

static int max_int(int p_a, int p_b) {
    return p_a > p_b ? p_a : p_b;
}

static void add_floor(DungeonLayout& r_layout, const Vector2i& p_cell) {
    r_layout.corridor_cells.insert(cave_cell_key(p_cell.x, p_cell.y));
}

static bool has_floor(const DungeonLayout& p_layout, int p_x, int p_y) {
    return p_layout.corridor_cells.count(cave_cell_key(p_x, p_y)) > 0;
}

static void carve_disk(DungeonLayout& r_layout, const Vector2i& p_center, int p_radius) {
    const int radius_sq = p_radius * p_radius;
    for (int y = p_center.y - p_radius; y <= p_center.y + p_radius; y++) {
        for (int x = p_center.x - p_radius; x <= p_center.x + p_radius; x++) {
            const int dx = x - p_center.x;
            const int dy = y - p_center.y;
            if (dx * dx + dy * dy <= radius_sq) {
                add_floor(r_layout, Vector2i(x, y));
            }
        }
    }
}

static void carve_blob(DungeonLayout& r_layout, Rng::Seeded& r_rng, const Vector2i& p_center, int p_radius_x, int p_radius_y) {
    const int radius_x = max_int(2, p_radius_x);
    const int radius_y = max_int(2, p_radius_y);
    for (int y = p_center.y - radius_y - 1; y <= p_center.y + radius_y + 1; y++) {
        for (int x = p_center.x - radius_x - 1; x <= p_center.x + radius_x + 1; x++) {
            const float nx = static_cast<float>(x - p_center.x) / static_cast<float>(radius_x);
            const float ny = static_cast<float>(y - p_center.y) / static_cast<float>(radius_y);
            const float rough_edge = 1.0f + (r_rng.unit() * 0.34f - 0.17f);
            if (nx * nx + ny * ny <= rough_edge) {
                add_floor(r_layout, Vector2i(x, y));
            }
        }
    }
}

static int manhattan_distance(const Vector2i& p_a, const Vector2i& p_b) {
    return std::abs(p_a.x - p_b.x) + std::abs(p_a.y - p_b.y);
}

static void carve_corridor_brush(DungeonLayout& r_layout, const Vector2i& p_center, int p_width) {
    const int width = max_int(1, p_width);
    const int first_offset = -((width - 1) / 2);
    for (int dy = 0; dy < width; dy++) {
        for (int dx = 0; dx < width; dx++) {
            add_floor(r_layout, p_center + Vector2i(first_offset + dx, first_offset + dy));
        }
    }
}

static void carve_winding_tunnel(DungeonLayout& r_layout, Rng::Seeded& r_rng, const Vector2i& p_from, const Vector2i& p_to, int p_width) {
    Vector2i pos = p_from;
    Vector2i previous_step;
    carve_corridor_brush(r_layout, pos, p_width);

    int guard = 0;
    const int direct_distance = manhattan_distance(p_from, p_to);
    const int max_steps = direct_distance * 5 + 64;
    static const Vector2i steps[4] = {
        Vector2i(1, 0), Vector2i(-1, 0), Vector2i(0, 1), Vector2i(0, -1)
    };

    while (pos != p_to && guard < max_steps) {
        guard++;
        const int current_distance = manhattan_distance(pos, p_to);
        int best_score = -1000000;
        Vector2i best_step;

        for (const Vector2i& step : steps) {
            const Vector2i next = pos + step;
            const int next_distance = manhattan_distance(next, p_to);
            int score = (current_distance - next_distance) * 12 + r_rng.range(0, 12);
            if (step == previous_step) score += 5;
            if (previous_step != Vector2i() && step == Vector2i(-previous_step.x, -previous_step.y)) score -= 18;
            if (next_distance > current_distance + 1) score -= 12;
            if (score > best_score) {
                best_score = score;
                best_step = step;
            }
        }

        // Sustained perpendicular detours create bends without breaking
        // four-directional connectivity or allowing an unbounded walk.
        if (previous_step != Vector2i() && guard < direct_distance * 3 && r_rng.chance(0.22f)) {
            const Vector2i left(-previous_step.y, previous_step.x);
            const Vector2i right(previous_step.y, -previous_step.x);
            const Vector2i detour = r_rng.chance(0.5f) ? left : right;
            if (manhattan_distance(pos + detour, p_to) <= current_distance + 1) {
                best_step = detour;
            }
        }

        pos += best_step;
        previous_step = best_step;
        carve_corridor_brush(r_layout, pos, p_width);
    }

    // Guaranteed deterministic completion if the winding budget is exhausted.
    while (pos != p_to) {
        if (pos.x != p_to.x) {
            pos.x += p_to.x > pos.x ? 1 : -1;
        } else {
            pos.y += p_to.y > pos.y ? 1 : -1;
        }
        carve_corridor_brush(r_layout, pos, p_width);
    }
}

static Vector2i cardinal_direction(const Vector2i& p_delta) {
    if (std::abs(p_delta.x) >= std::abs(p_delta.y) && p_delta.x != 0) {
        return Vector2i(p_delta.x > 0 ? 1 : -1, 0);
    }
    if (p_delta.y != 0) {
        return Vector2i(0, p_delta.y > 0 ? 1 : -1);
    }
    return Vector2i(0, -1);
}

static uint8_t rotation_for_room_interior_direction(const Vector2i& p_direction) {
    if (p_direction.x > 0) return WorldCoords::ROT_WEST;
    if (p_direction.x < 0) return WorldCoords::ROT_EAST;
    if (p_direction.y > 0) return WorldCoords::ROT_NORTH;
    return WorldCoords::ROT_SOUTH;
}

static Vector2i rotated_size(const Vector2i& p_size, uint8_t p_rotation) {
    return (p_rotation == WorldCoords::ROT_WEST || p_rotation == WorldCoords::ROT_EAST)
        ? Vector2i(p_size.y, p_size.x)
        : p_size;
}

static Vector2i source_to_placed(const Vector2i& p_source, const Vector2i& p_size, uint8_t p_rotation) {
    switch (p_rotation) {
        case WorldCoords::ROT_WEST:
            return Vector2i(p_size.y - 1 - p_source.y, p_source.x);
        case WorldCoords::ROT_NORTH:
            return Vector2i(p_size.x - 1 - p_source.x, p_size.y - 1 - p_source.y);
        case WorldCoords::ROT_EAST:
            return Vector2i(p_source.y, p_size.x - 1 - p_source.x);
        case WorldCoords::ROT_SOUTH:
        default:
            return p_source;
    }
}

static bool room_within_cave_radius(const DungeonRect& p_bounds, const Vector2i& p_entrance_center, int p_max_offset) {
    return p_bounds.origin.x >= p_entrance_center.x - p_max_offset
        && p_bounds.origin.y >= p_entrance_center.y - p_max_offset
        && p_bounds.origin.x + p_bounds.size.x - 1 <= p_entrance_center.x + p_max_offset
        && p_bounds.origin.y + p_bounds.size.y - 1 <= p_entrance_center.y + p_max_offset;
}

static bool room_overlaps_carved_cave(const DungeonLayout& p_layout, const DungeonRect& p_bounds, const Vector2i& p_allowed_entrance) {
    for (int y = p_bounds.origin.y; y < p_bounds.origin.y + p_bounds.size.y; y++) {
        for (int x = p_bounds.origin.x; x < p_bounds.origin.x + p_bounds.size.x; x++) {
            if (Vector2i(x, y) == p_allowed_entrance) continue;
            if (has_floor(p_layout, x, y)) return true;
        }
    }
    return false;
}

static void carve_straight_connector(DungeonLayout& r_layout, const Vector2i& p_from, const Vector2i& p_to) {
    Vector2i pos = p_from;
    add_floor(r_layout, pos);
    while (pos != p_to) {
        if (pos.x != p_to.x) {
            pos.x += p_to.x > pos.x ? 1 : -1;
        } else {
            pos.y += p_to.y > pos.y ? 1 : -1;
        }
        add_floor(r_layout, pos);
    }
}

static void expand_bounds(DungeonRect& r_bounds, bool& r_initialized, const DungeonRect& p_rect) {
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

static void finalize_cave_layout(DungeonLayout& r_layout) {
    r_layout.corridor_wall_cells.clear();
    r_layout.door_cells.clear();

    bool bounds_initialized = false;
    expand_bounds(
        r_layout.bounds,
        bounds_initialized,
        DungeonRect{
            Vector2i(r_layout.entrance_chunk.x * WorldCoords::CHUNK_SIZE, r_layout.entrance_chunk.y * WorldCoords::CHUNK_SIZE),
            Vector2i(WorldCoords::CHUNK_SIZE, WorldCoords::CHUNK_SIZE)
        }
    );

    for (const PlacedDungeonRoom& room : r_layout.rooms) {
        expand_bounds(r_layout.bounds, bounds_initialized, room.bounds);
    }

    for (uint64_t key : r_layout.corridor_cells) {
        const Vector2i cell = WorldCoords::unpack_coords(key);
        expand_bounds(r_layout.bounds, bounds_initialized, DungeonRect{cell, Vector2i(1, 1)});
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                const int wx = cell.x + dx;
                const int wy = cell.y + dy;
                if (!has_floor(r_layout, wx, wy)) {
                    r_layout.corridor_wall_cells.insert(cave_cell_key(wx, wy));
                }
            }
        }
    }

    for (uint64_t key : r_layout.corridor_wall_cells) {
        const Vector2i cell = WorldCoords::unpack_coords(key);
        expand_bounds(r_layout.bounds, bounds_initialized, DungeonRect{cell, Vector2i(1, 1)});
    }
}

static String pick_structure_by_type(const String& p_type, Rng::Seeded& r_rng) {
    if (p_type.is_empty()) return "";

    StructureDb* structure_db = StructureDb::get_singleton();
    const std::vector<String>* structures = structure_db ? structure_db->get_structure_ids_by_type(p_type) : nullptr;
    if (!structures || structures->empty()) return "";

    return (*structures)[r_rng.range(0, static_cast<int>(structures->size()) - 1)];
}

static String pick_structure_from_feature_pool(const String& p_pool_id, Rng::Seeded& r_rng) {
    if (p_pool_id.is_empty()) return "";

    FeatureDb* feature_db = FeatureDb::get_singleton();
    const FeaturePoolInfo* pool = feature_db ? feature_db->get_feature_pool(p_pool_id) : nullptr;
    const FeatureEntryInfo* entry = pool ? feature_db->pick_weighted_entry(*pool, r_rng) : nullptr;
    return entry ? entry->structure_id : String();
}

static Vector2i structure_size(const String& p_structure_id) {
    StructureDb* structure_db = StructureDb::get_singleton();
    return structure_db ? structure_db->get_structure_size(p_structure_id) : Vector2i();
}

}

DungeonLayout CaveGenerator::build_layout(
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

    Rng::Seeded rng = Rng::at(static_cast<uint32_t>(p_world_seed), p_entrance_chunk, Rng::BIOME, CAVE_LAYOUT_SALT);
    const Vector2i entrance_origin(
        p_entrance_chunk.x * WorldCoords::CHUNK_SIZE,
        p_entrance_chunk.y * WorldCoords::CHUNK_SIZE
    );
    const Vector2i entrance_center = entrance_origin + Vector2i(WorldCoords::CHUNK_SIZE / 2, WorldCoords::CHUNK_SIZE / 2);

    std::vector<CaveChamber> chambers;
    chambers.reserve(p_info.room_count_max + 1);

    auto add_chamber = [&](const Vector2i& p_center, int p_radius_x, int p_radius_y, int p_parent_index, int p_path_distance) {
        chambers.push_back(CaveChamber{p_center, p_radius_x, p_radius_y, p_parent_index, 0, p_path_distance});
        layout.cave_chamber_centers.push_back(p_center);
        carve_blob(layout, rng, p_center, p_radius_x, p_radius_y);
        return static_cast<int>(chambers.size()) - 1;
    };

    add_chamber(entrance_center, 3, 3, -1, 0);

    static const Vector2i directions[8] = {
        Vector2i(1, 0),
        Vector2i(-1, 0),
        Vector2i(0, 1),
        Vector2i(0, -1),
        Vector2i(1, 1),
        Vector2i(-1, 1),
        Vector2i(1, -1),
        Vector2i(-1, -1)
    };

    const int target_chambers = rng.range(p_info.room_count_min, p_info.room_count_max);
    const int max_offset = max_int(1, p_info.radius_chunks) * WorldCoords::CHUNK_SIZE;
    int attempts = 0;
    while (static_cast<int>(chambers.size()) < target_chambers && attempts < target_chambers * CAVE_ATTEMPTS_PER_CHAMBER) {
        attempts++;
        const int chamber_count = static_cast<int>(chambers.size());
        const int recent_start = std::max(0, chamber_count - 4);
        const int parent_index = rng.chance(0.75f)
            ? rng.range(recent_start, chamber_count - 1)
            : rng.range(0, chamber_count - 1);
        const CaveChamber parent = chambers[parent_index];
        const Vector2i dir = directions[rng.range(0, 7)];
        const Vector2i perp(-dir.y, dir.x);
        const int radius_x = rng.range(2, 4);
        const int radius_y = rng.range(2, 4);
        const int distance = rng.range(9, 16) + max_int(parent.radius_x, parent.radius_y);
        const int lateral = rng.range(-5, 5);
        const Vector2i center = parent.center + Vector2i(dir.x * distance, dir.y * distance) + Vector2i(perp.x * lateral, perp.y * lateral);

        if (std::abs(center.x - entrance_center.x) > max_offset || std::abs(center.y - entrance_center.y) > max_offset) {
            continue;
        }

        bool too_close = false;
        for (const CaveChamber& existing : chambers) {
            const int separation = max_int(existing.radius_x, existing.radius_y) + max_int(radius_x, radius_y) + 4;
            if (distance_sq(existing.center, center) < separation * separation) {
                too_close = true;
                break;
            }
        }
        if (too_close) continue;

        carve_winding_tunnel(layout, rng, parent.center, center, p_info.corridor_width);
        const int path_distance = parent.path_distance + manhattan_distance(parent.center, center);
        add_chamber(center, radius_x, radius_y, parent_index, path_distance);
        chambers[parent_index].child_count++;
    }

    String end_structure_id = pick_structure_from_feature_pool(p_info.end_feature_pool, rng);
    if (end_structure_id.is_empty()) {
        end_structure_id = pick_structure_by_type(p_info.end_structure_type, rng);
    }
    const Vector2i end_size = structure_size(end_structure_id);
    if (!end_structure_id.is_empty() && end_size.x > 0 && end_size.y > 0 && !chambers.empty()) {
        std::vector<int> candidate_indices;
        candidate_indices.reserve(chambers.size());
        for (int i = 1; i < static_cast<int>(chambers.size()); i++) {
            if (chambers[i].child_count == 0) candidate_indices.push_back(i);
        }
        for (int i = 1; i < static_cast<int>(chambers.size()); i++) {
            if (chambers[i].child_count != 0) candidate_indices.push_back(i);
        }
        if (candidate_indices.empty()) candidate_indices.push_back(0);

        std::stable_sort(candidate_indices.begin(), candidate_indices.end(), [&](int p_a, int p_b) {
            const bool a_is_terminal = chambers[p_a].child_count == 0;
            const bool b_is_terminal = chambers[p_b].child_count == 0;
            if (a_is_terminal != b_is_terminal) return a_is_terminal;
            return chambers[p_a].path_distance > chambers[p_b].path_distance;
        });

        bool placed_end = false;
        for (int chamber_index : candidate_indices) {
            const CaveChamber& chamber = chambers[chamber_index];
            const Vector2i parent_center = chamber.parent_index >= 0
                ? chambers[chamber.parent_index].center
                : entrance_center + Vector2i(0, 1);
            const Vector2i preferred = cardinal_direction(chamber.center - parent_center);
            const std::array<Vector2i, 4> placement_directions = {
                preferred,
                Vector2i(-preferred.y, preferred.x),
                Vector2i(preferred.y, -preferred.x),
                Vector2i(-preferred.x, -preferred.y)
            };

            for (const Vector2i& outward : placement_directions) {
                const uint8_t rotation = rotation_for_room_interior_direction(outward);
                const Vector2i placed_size = rotated_size(end_size, rotation);
                const Vector2i source_entrance(end_size.x / 2, end_size.y - 1);
                const Vector2i placed_entrance = source_to_placed(source_entrance, end_size, rotation);
                const int chamber_extent = outward.x != 0 ? chamber.radius_x : chamber.radius_y;

                for (int extra_distance = 3; extra_distance <= std::max(end_size.x, end_size.y) + 8; extra_distance++) {
                    const Vector2i world_entrance = chamber.center + outward * (chamber_extent + extra_distance);
                    const DungeonRect bounds{world_entrance - placed_entrance, placed_size};
                    if (!room_within_cave_radius(bounds, entrance_center, max_offset)) continue;
                    if (room_overlaps_carved_cave(layout, bounds, world_entrance)) continue;

                    PlacedDungeonRoom room;
                    room.bounds = bounds;
                    room.structure_id = end_structure_id;
                    room.rotation = rotation;
                    layout.rooms.push_back(room);
                    carve_straight_connector(layout, chamber.center, world_entrance);
                    placed_end = true;
                    break;
                }
                if (placed_end) break;
            }
            if (placed_end) break;
        }
    }

    finalize_cave_layout(layout);
    return layout;
}

}
