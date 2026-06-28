#include "cave_generator.h"
#include "core/rng.h"
#include "core/world_coords.h"
#include "data/dungeon_db.h"
#include "data/structure_db.h"
#include <algorithm>
#include <cstdlib>

namespace godot {

namespace {

static constexpr uint64_t CAVE_LAYOUT_SALT = 0x434156454C41594FULL; // "CAVELAYO"
static constexpr int CAVE_ATTEMPTS_PER_CHAMBER = 28;

struct CaveChamber {
    Vector2i center;
    int radius_x = 4;
    int radius_y = 4;
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

static void carve_tunnel(DungeonLayout& r_layout, Rng::Seeded& r_rng, const Vector2i& p_from, const Vector2i& p_to, int p_width) {
    Vector2i pos = p_from;
    const int tunnel_radius = max_int(1, p_width);
    carve_disk(r_layout, pos, tunnel_radius);

    int guard = 0;
    while (pos != p_to && guard < 512) {
        guard++;

        const int dx = p_to.x - pos.x;
        const int dy = p_to.y - pos.y;
        const bool step_x = dx != 0 && (dy == 0 || std::abs(dx) >= std::abs(dy) || r_rng.chance(0.45f));
        if (step_x) {
            pos.x += dx > 0 ? 1 : -1;
        } else if (dy != 0) {
            pos.y += dy > 0 ? 1 : -1;
        }

        if (r_rng.chance(0.12f)) {
            if (step_x) {
                pos.y += r_rng.range(-1, 1);
            } else {
                pos.x += r_rng.range(-1, 1);
            }
        }

        carve_disk(r_layout, pos, tunnel_radius);
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
    layout.entrance_chunk = p_entrance_chunk;

    Rng::Seeded rng = Rng::at(static_cast<uint32_t>(p_world_seed), p_entrance_chunk, Rng::BIOME, CAVE_LAYOUT_SALT);
    const Vector2i entrance_origin(
        p_entrance_chunk.x * WorldCoords::CHUNK_SIZE,
        p_entrance_chunk.y * WorldCoords::CHUNK_SIZE
    );
    const Vector2i entrance_center = entrance_origin + Vector2i(WorldCoords::CHUNK_SIZE / 2, WorldCoords::CHUNK_SIZE / 2);

    std::vector<CaveChamber> chambers;
    chambers.reserve(p_info.room_count_max + 1);

    auto add_chamber = [&](const Vector2i& p_center, int p_radius_x, int p_radius_y) {
        chambers.push_back(CaveChamber{p_center, p_radius_x, p_radius_y});
        layout.cave_chamber_centers.push_back(p_center);
        carve_blob(layout, rng, p_center, p_radius_x, p_radius_y);
    };

    add_chamber(entrance_center, 5, 4);

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
        const CaveChamber& parent = chambers[rng.range(0, static_cast<int>(chambers.size()) - 1)];
        const Vector2i dir = directions[rng.range(0, 7)];
        const Vector2i perp(-dir.y, dir.x);
        const int radius_x = rng.range(4, 8);
        const int radius_y = rng.range(3, 7);
        const int distance = rng.range(12, 22) + max_int(parent.radius_x, parent.radius_y);
        const int lateral = rng.range(-7, 7);
        const Vector2i center = parent.center + Vector2i(dir.x * distance, dir.y * distance) + Vector2i(perp.x * lateral, perp.y * lateral);

        if (std::abs(center.x - entrance_center.x) > max_offset || std::abs(center.y - entrance_center.y) > max_offset) {
            continue;
        }

        bool too_close = false;
        for (const CaveChamber& existing : chambers) {
            if (distance_sq(existing.center, center) < 64) {
                too_close = true;
                break;
            }
        }
        if (too_close) continue;

        carve_tunnel(layout, rng, parent.center, center, max_int(1, p_info.corridor_width));
        add_chamber(center, radius_x, radius_y);
    }

    const String end_structure_id = pick_structure_by_type(p_info.end_structure_type, rng);
    const Vector2i end_size = structure_size(end_structure_id);
    if (!end_structure_id.is_empty() && end_size.x > 0 && end_size.y > 0 && !chambers.empty()) {
        int farthest_index = 0;
        int farthest_distance = -1;
        for (int i = 0; i < static_cast<int>(chambers.size()); i++) {
            const int d = distance_sq(chambers[i].center, entrance_center);
            if (d > farthest_distance) {
                farthest_distance = d;
                farthest_index = i;
            }
        }

        const Vector2i end_center = chambers[farthest_index].center;
        carve_blob(layout, rng, end_center, end_size.x / 2 + 3, end_size.y / 2 + 3);
        carve_tunnel(layout, rng, chambers[farthest_index].center, end_center, max_int(1, p_info.corridor_width));

        PlacedDungeonRoom room;
        room.bounds = DungeonRect{
            Vector2i(end_center.x - end_size.x / 2, end_center.y - end_size.y / 2),
            end_size
        };
        room.structure_id = end_structure_id;
        layout.rooms.push_back(room);
    }

    finalize_cave_layout(layout);
    return layout;
}

}
