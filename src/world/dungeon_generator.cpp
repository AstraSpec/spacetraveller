#include "dungeon_generator.h"
#include "core/rng.h"
#include "core/world_coords.h"
#include "data/dungeon_db.h"
#include <algorithm>

using namespace godot;

static uint64_t cell_key(int p_x, int p_y) {
    return WorldCoords::pack_coords(p_x, p_y);
}

bool DungeonLayout::might_contain(int p_x, int p_y) const {
    DungeonRect padded = bounds;
    padded.origin.x -= 2;
    padded.origin.y -= 2;
    padded.size.x += 4;
    padded.size.y += 4;
    return DungeonGenerator::rect_has_point(padded, p_x, p_y);
}

bool DungeonLayout::has_corridor(int p_x, int p_y) const {
    return corridor_cells.count(cell_key(p_x, p_y)) > 0;
}

bool DungeonLayout::has_corridor_wall(int p_x, int p_y) const {
    return corridor_wall_cells.count(cell_key(p_x, p_y)) > 0;
}

bool DungeonLayout::has_door(int p_x, int p_y) const {
    return door_cells.count(cell_key(p_x, p_y)) > 0;
}

bool DungeonGenerator::rect_has_point(const DungeonRect& p_rect, int p_x, int p_y) {
    return p_x >= p_rect.origin.x && p_y >= p_rect.origin.y &&
        p_x < p_rect.origin.x + p_rect.size.x &&
        p_y < p_rect.origin.y + p_rect.size.y;
}

bool DungeonGenerator::rects_overlap(const DungeonRect& p_a, const DungeonRect& p_b, int p_padding) {
    const int a_min_x = p_a.origin.x - p_padding;
    const int a_min_y = p_a.origin.y - p_padding;
    const int a_max_x = p_a.origin.x + p_a.size.x + p_padding;
    const int a_max_y = p_a.origin.y + p_a.size.y + p_padding;
    const int b_min_x = p_b.origin.x;
    const int b_min_y = p_b.origin.y;
    const int b_max_x = p_b.origin.x + p_b.size.x;
    const int b_max_y = p_b.origin.y + p_b.size.y;

    return a_min_x < b_max_x && a_max_x > b_min_x && a_min_y < b_max_y && a_max_y > b_min_y;
}

bool DungeonGenerator::room_boundary_has_point(const DungeonRect& p_rect, int p_x, int p_y) {
    if (!rect_has_point(p_rect, p_x, p_y)) return false;
    return p_x == p_rect.origin.x ||
        p_y == p_rect.origin.y ||
        p_x == p_rect.origin.x + p_rect.size.x - 1 ||
        p_y == p_rect.origin.y + p_rect.size.y - 1;
}

namespace {

static constexpr uint64_t DUNGEON_LAYOUT_SALT = 0x44554E474C41594FULL; // "DUNGLAYO"
static constexpr int ROOM_SIZE_MIN = 7;
static constexpr int ROOM_SIZE_MAX = 12;
static constexpr int ROOM_OVERLAP_PADDING = 4;
static constexpr int ROOM_DISTANCE_MIN = 4;
static constexpr int ROOM_DISTANCE_MAX = 11;
static constexpr int ROOM_LATERAL_MIN = -5;
static constexpr int ROOM_LATERAL_MAX = 5;
static constexpr int ATTEMPTS_PER_TARGET_ROOM = 40;
static constexpr int FIRST_ROOM_EXIT_LIMIT = 4;

static Vector2i room_center(const PlacedDungeonRoom& p_room) {
    return Vector2i(
        p_room.bounds.origin.x + p_room.bounds.size.x / 2,
        p_room.bounds.origin.y + p_room.bounds.size.y / 2
    );
}

static int clamp_int(int p_value, int p_min, int p_max) {
    if (p_value < p_min) return p_min;
    if (p_value > p_max) return p_max;
    return p_value;
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

static void finalize_layout(DungeonLayout& r_layout) {
    r_layout.corridor_cells.clear();
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

    for (const DungeonCorridor& corridor : r_layout.corridors) {
        for (const Vector2i& cell : corridor.cells) {
            r_layout.corridor_cells.insert(cell_key(cell.x, cell.y));
            expand_bounds(r_layout.bounds, bounds_initialized, DungeonRect{cell, Vector2i(1, 1)});
        }
    }

    for (const DungeonCorridor& corridor : r_layout.corridors) {
        for (const Vector2i& cell : corridor.cells) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    const int wx = cell.x + dx;
                    const int wy = cell.y + dy;
                    const uint64_t key = cell_key(wx, wy);
                    if (r_layout.corridor_cells.count(key) == 0) {
                        r_layout.corridor_wall_cells.insert(key);
                    }
                }
            }
        }
    }

    for (const Vector2i& door : r_layout.doors) {
        r_layout.door_cells.insert(cell_key(door.x, door.y));
    }
}

class DungeonLayoutBuilder {
public:
    DungeonLayoutBuilder(const DungeonInfo& p_info, const Vector2i& p_entrance_chunk, int p_world_seed) :
        info(p_info),
        entrance_chunk(p_entrance_chunk),
        rng(Rng::at(static_cast<uint32_t>(p_world_seed), p_entrance_chunk, Rng::BIOME, DUNGEON_LAYOUT_SALT)) {
        layout.dungeon_type = info.id;
        layout.z = info.start_z;
        layout.entrance_chunk = entrance_chunk;
    }

    DungeonLayout build() {
        const Vector2i entrance_origin(
            entrance_chunk.x * WorldCoords::CHUNK_SIZE,
            entrance_chunk.y * WorldCoords::CHUNK_SIZE
        );
        const Vector2i entrance_center = entrance_origin + Vector2i(WorldCoords::CHUNK_SIZE / 2, WorldCoords::CHUNK_SIZE / 2);

        if (!add_first_room(entrance_origin, entrance_center)) {
            finalize_layout(layout);
            return layout;
        }

        grow_rooms(rng.range(info.room_count_min, info.room_count_max));
        finalize_layout(layout);
        return layout;
    }

private:
    const DungeonInfo& info;
    Vector2i entrance_chunk;
    Rng::Seeded rng;
    DungeonLayout layout;
    std::vector<int> room_exit_counts;
    std::vector<int> room_exit_limits;

    void push_corridor_cell(DungeonCorridor& p_corridor, const Vector2i& p_cell) const {
        for (const Vector2i& existing : p_corridor.cells) {
            if (existing == p_cell) {
                return;
            }
        }
        p_corridor.cells.push_back(p_cell);
    }

    void build_corridor_segment(DungeonCorridor& p_corridor, const Vector2i& p_a, const Vector2i& p_b) const {
        if (p_a.x == p_b.x) {
            const int step = p_b.y >= p_a.y ? 1 : -1;
            for (int y = p_a.y; y != p_b.y + step; y += step) {
                push_corridor_cell(p_corridor, Vector2i(p_a.x, y));
            }
            return;
        }

        const int step = p_b.x >= p_a.x ? 1 : -1;
        for (int x = p_a.x; x != p_b.x + step; x += step) {
            push_corridor_cell(p_corridor, Vector2i(x, p_a.y));
        }
    }

    bool corridor_crosses_room(const DungeonCorridor& p_corridor) const {
        for (const Vector2i& cell : p_corridor.cells) {
            for (const PlacedDungeonRoom& room : layout.rooms) {
                if (DungeonGenerator::rect_has_point(room.bounds, cell.x, cell.y)) {
                    return true;
                }
            }
        }
        return false;
    }

    bool add_corridor_path(const std::vector<Vector2i>& p_points) {
        DungeonCorridor corridor;
        if (p_points.size() < 2) {
            return false;
        }

        for (int i = 0; i < (int)p_points.size() - 1; i++) {
            build_corridor_segment(corridor, p_points[i], p_points[i + 1]);
        }

        if (corridor.cells.empty() || corridor_crosses_room(corridor)) {
            return false;
        }

        layout.corridors.push_back(corridor);
        return true;
    }

    void add_door(const Vector2i& p_pos) {
        for (const Vector2i& existing : layout.doors) {
            if (existing == p_pos) {
                return;
            }
        }
        layout.doors.push_back(p_pos);
    }

    int random_exit_limit() {
        const int roll = rng.range(0, 99);
        if (roll < 14) return 1;
        if (roll < 62) return 2;
        if (roll < 90) return 3;
        return 4;
    }

    bool add_room(const Vector2i& p_origin, const Vector2i& p_size) {
        if (p_size.x <= 2 || p_size.y <= 2) return false;
        DungeonRect bounds{p_origin, p_size};
        for (const PlacedDungeonRoom& existing : layout.rooms) {
            if (DungeonGenerator::rects_overlap(existing.bounds, bounds, ROOM_OVERLAP_PADDING)) {
                return false;
            }
        }

        PlacedDungeonRoom room;
        room.bounds = bounds;
        layout.rooms.push_back(room);
        room_exit_counts.push_back(0);
        room_exit_limits.push_back(random_exit_limit());
        return true;
    }

    Vector2i random_room_size() {
        const int side = rng.range(ROOM_SIZE_MIN, ROOM_SIZE_MAX);
        return Vector2i(side, side);
    }

    Vector2i side_door(const PlacedDungeonRoom& p_room, const Vector2i& p_side_dir, int p_cross_axis_hint) const {
        const int left = p_room.bounds.origin.x;
        const int right = p_room.bounds.origin.x + p_room.bounds.size.x - 1;
        const int top = p_room.bounds.origin.y;
        const int bottom = p_room.bounds.origin.y + p_room.bounds.size.y - 1;
        const Vector2i center = room_center(p_room);

        if (p_side_dir.x < 0) {
            return Vector2i(left, clamp_int(p_cross_axis_hint, top + 1, bottom - 1));
        }
        if (p_side_dir.x > 0) {
            return Vector2i(right, clamp_int(p_cross_axis_hint, top + 1, bottom - 1));
        }
        if (p_side_dir.y < 0) {
            return Vector2i(clamp_int(p_cross_axis_hint, left + 1, right - 1), top);
        }
        if (p_side_dir.y > 0) {
            return Vector2i(clamp_int(p_cross_axis_hint, left + 1, right - 1), bottom);
        }
        return center;
    }

    Vector2i outside_from_door(const Vector2i& p_door, const Vector2i& p_side_dir) const {
        return Vector2i(p_door.x + p_side_dir.x, p_door.y + p_side_dir.y);
    }

    bool add_room_corridor(int p_room_a, int p_room_b, const Vector2i& p_target_dir) {
        const Vector2i center_a = room_center(layout.rooms[p_room_a]);
        const Vector2i center_b = room_center(layout.rooms[p_room_b]);
        const Vector2i parent_side = p_target_dir;
        const Vector2i target_side = Vector2i(-p_target_dir.x, -p_target_dir.y);
        const int parent_hint = p_target_dir.x != 0 ? center_a.y : center_a.x;
        const int target_hint = p_target_dir.x != 0 ? center_b.y : center_b.x;
        const Vector2i edge_a = side_door(layout.rooms[p_room_a], parent_side, parent_hint);
        const Vector2i edge_b = side_door(layout.rooms[p_room_b], target_side, target_hint);
        const Vector2i outside_a = outside_from_door(edge_a, parent_side);
        const Vector2i outside_b = outside_from_door(edge_b, target_side);

        std::vector<Vector2i> path;
        if (p_target_dir.x != 0) {
            const int mid_x = (outside_a.x + outside_b.x) / 2;
            path = {
                outside_a,
                Vector2i(mid_x, outside_a.y),
                Vector2i(mid_x, outside_b.y),
                outside_b
            };
        } else {
            const int mid_y = (outside_a.y + outside_b.y) / 2;
            path = {
                outside_a,
                Vector2i(outside_a.x, mid_y),
                Vector2i(outside_b.x, mid_y),
                outside_b
            };
        }

        if (!add_corridor_path(path)) {
            return false;
        }
        add_door(edge_a);
        add_door(edge_b);
        if (p_room_a >= 0 && p_room_a < (int)room_exit_counts.size()) {
            room_exit_counts[p_room_a]++;
        }
        if (p_room_b >= 0 && p_room_b < (int)room_exit_counts.size()) {
            room_exit_counts[p_room_b]++;
        }
        return true;
    }

    bool add_first_room(const Vector2i& p_entrance_origin, const Vector2i& p_entrance_center) {
        Vector2i first_size = random_room_size();
        Vector2i first_origin(
            p_entrance_center.x - first_size.x / 2,
            p_entrance_origin.y + WorldCoords::CHUNK_SIZE + ROOM_DISTANCE_MIN
        );
        if (!add_room(first_origin, first_size)) {
            return false;
        }

        const Vector2i first_side = Vector2i(0, -1);
        Vector2i first_door = side_door(layout.rooms.front(), first_side, room_center(layout.rooms.front()).x);
        Vector2i first_outside = outside_from_door(first_door, first_side);
        const int first_mid_y = (p_entrance_center.y + first_outside.y) / 2;
        std::vector<Vector2i> first_path = {
            p_entrance_center,
            Vector2i(p_entrance_center.x, first_mid_y),
            Vector2i(first_outside.x, first_mid_y),
            first_outside
        };
        if (add_corridor_path(first_path)) {
            add_door(first_door);
            room_exit_counts[0]++;
            room_exit_limits[0] = FIRST_ROOM_EXIT_LIMIT;
        }
        return true;
    }

    int choose_parent_room() {
        int total_weight = 0;
        for (int i = 0; i < (int)room_exit_counts.size(); i++) {
            const int remaining = room_exit_limits[i] - room_exit_counts[i];
            if (remaining <= 0) continue;

            int weight = remaining * remaining;
            if (room_exit_counts[i] >= 1) weight += 3;
            if (room_exit_counts[i] >= 2) weight += 4;
            total_weight += weight;
        }

        if (total_weight <= 0) return -1;

        int roll = rng.range(1, total_weight);
        for (int i = 0; i < (int)room_exit_counts.size(); i++) {
            const int remaining = room_exit_limits[i] - room_exit_counts[i];
            if (remaining <= 0) continue;

            int weight = remaining * remaining;
            if (room_exit_counts[i] >= 1) weight += 3;
            if (room_exit_counts[i] >= 2) weight += 4;
            roll -= weight;
            if (roll <= 0) return i;
        }

        return -1;
    }

    void grow_rooms(int p_target_rooms) {
        static const Vector2i directions[4] = {
            Vector2i(1, 0),
            Vector2i(-1, 0),
            Vector2i(0, 1),
            Vector2i(0, -1)
        };
        int attempts = 0;
        while ((int)layout.rooms.size() < p_target_rooms && attempts < p_target_rooms * ATTEMPTS_PER_TARGET_ROOM) {
            attempts++;
            if (layout.rooms.empty()) break;

            const int parent_index = choose_parent_room();
            if (parent_index < 0) break;

            const PlacedDungeonRoom& parent = layout.rooms[parent_index];
            const Vector2i parent_center = room_center(parent);
            const Vector2i dir = directions[rng.range(0, 3)];
            const Vector2i perp(-dir.y, dir.x);

            Vector2i size = random_room_size();
            const int parent_extent = (dir.x != 0) ? parent.bounds.size.x / 2 : parent.bounds.size.y / 2;
            const int room_extent = (dir.x != 0) ? size.x / 2 : size.y / 2;
            const int distance = parent_extent + room_extent + rng.range(ROOM_DISTANCE_MIN, ROOM_DISTANCE_MAX);
            const int lateral = rng.range(ROOM_LATERAL_MIN, ROOM_LATERAL_MAX);
            Vector2i new_center = parent_center + Vector2i(dir.x * distance, dir.y * distance) + Vector2i(perp.x * lateral, perp.y * lateral);
            Vector2i origin(new_center.x - size.x / 2, new_center.y - size.y / 2);

            if (!add_room(origin, size)) continue;

            const int placed_index = static_cast<int>(layout.rooms.size()) - 1;
            if (!add_room_corridor(parent_index, placed_index, dir)) {
                layout.rooms.pop_back();
                room_exit_counts.pop_back();
                room_exit_limits.pop_back();
                continue;
            }
        }
    }
};

} // namespace

DungeonLayout DungeonGenerator::build_layout(
    const DungeonInfo& p_info,
    const Vector2i& p_entrance_chunk,
    int p_world_seed
) {
    return DungeonLayoutBuilder(p_info, p_entrance_chunk, p_world_seed).build();
}
