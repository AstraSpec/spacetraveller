#ifndef SPACETRAVELLER_DUNGEON_GENERATOR_H
#define SPACETRAVELLER_DUNGEON_GENERATOR_H

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace godot {

struct DungeonInfo;

struct DungeonRect {
    Vector2i origin;
    Vector2i size;
};

struct PlacedDungeonRoom {
    DungeonRect bounds;
};

struct DungeonCorridor {
    std::vector<Vector2i> cells;
};

struct DungeonLayout {
    String dungeon_type;
    int z = -1;
    Vector2i entrance_chunk;
    DungeonRect bounds;
    std::vector<PlacedDungeonRoom> rooms;
    std::vector<DungeonCorridor> corridors;
    std::vector<Vector2i> doors;
    std::unordered_set<uint64_t> corridor_cells;
    std::unordered_set<uint64_t> corridor_wall_cells;
    std::unordered_set<uint64_t> door_cells;

    bool might_contain(int p_x, int p_y) const;
    bool has_corridor(int p_x, int p_y) const;
    bool has_corridor_wall(int p_x, int p_y) const;
    bool has_door(int p_x, int p_y) const;
};

class DungeonGenerator {
public:
    static DungeonLayout build_layout(const DungeonInfo& p_info, const Vector2i& p_entrance_chunk, int p_world_seed);
    static bool rect_has_point(const DungeonRect& p_rect, int p_x, int p_y);
    static bool rects_overlap(const DungeonRect& p_a, const DungeonRect& p_b, int p_padding = 0);
    static bool room_boundary_has_point(const DungeonRect& p_rect, int p_x, int p_y);
};

}

#endif // SPACETRAVELLER_DUNGEON_GENERATOR_H
