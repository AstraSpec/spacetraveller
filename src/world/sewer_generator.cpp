#include "sewer_generator.h"

#include "core/rng.h"
#include "core/world_coords.h"
#include <algorithm>
#include <cstdlib>

namespace godot {

namespace {

constexpr int CENTER_LOW = 9;
constexpr int CENTER_HIGH = 14;
constexpr uint64_t SEWER_CELL_SALT = 0x534557455243454CULL; // "SEWERCEL"

int floor_mod_chunk(int p_value) {
    int result = p_value % WorldCoords::CHUNK_SIZE;
    return result < 0 ? result + WorldCoords::CHUNK_SIZE : result;
}

int connection_count(uint8_t p_connections) {
    int count = 0;
    for (int bit = 0; bit < 4; bit++) {
        if ((p_connections & (1 << bit)) != 0) count++;
    }
    return count;
}

struct ManholePlacement {
    bool valid = false;
    WorldCoords::NeighborBits side = WorldCoords::NEIGH_WEST;
    int cover_x = 0;
    int cover_y = 0;
    int lower_x = 0;
    int lower_y = 0;
};

ManholePlacement get_manhole_placement(const SewerChunkDescriptor& p_chunk) {
    ManholePlacement placement;
    if (!p_chunk.manhole) return placement;

    const bool north = (p_chunk.connections & WorldCoords::NEIGH_NORTH) != 0;
    const bool east = (p_chunk.connections & WorldCoords::NEIGH_EAST) != 0;
    const bool south = (p_chunk.connections & WorldCoords::NEIGH_SOUTH) != 0;
    const bool west = (p_chunk.connections & WorldCoords::NEIGH_WEST) != 0;
    const int count = connection_count(p_chunk.connections);

    if (count == 1) {
        if (north || south) {
            placement.side = (p_chunk.variant & 8ULL) != 0 ? WorldCoords::NEIGH_EAST : WorldCoords::NEIGH_WEST;
        } else {
            placement.side = (p_chunk.variant & 8ULL) != 0 ? WorldCoords::NEIGH_SOUTH : WorldCoords::NEIGH_NORTH;
        }
    } else if (count == 2 && east && west && !north && !south) {
        const WorldCoords::NeighborBits room_side = (p_chunk.variant & 8ULL) != 0
            ? WorldCoords::NEIGH_SOUTH
            : WorldCoords::NEIGH_NORTH;
        placement.side = room_side == WorldCoords::NEIGH_NORTH ? WorldCoords::NEIGH_SOUTH : WorldCoords::NEIGH_NORTH;
    } else if (count == 2 && north && south && !east && !west) {
        const WorldCoords::NeighborBits room_side = (p_chunk.variant & 8ULL) != 0
            ? WorldCoords::NEIGH_EAST
            : WorldCoords::NEIGH_WEST;
        placement.side = room_side == WorldCoords::NEIGH_WEST ? WorldCoords::NEIGH_EAST : WorldCoords::NEIGH_WEST;
    } else {
        WorldCoords::NeighborBits open_sides[4];
        int open_count = 0;
        const WorldCoords::NeighborBits sides[4] = {
            WorldCoords::NEIGH_NORTH,
            WorldCoords::NEIGH_EAST,
            WorldCoords::NEIGH_SOUTH,
            WorldCoords::NEIGH_WEST
        };
        for (WorldCoords::NeighborBits side : sides) {
            if ((p_chunk.connections & side) == 0) open_sides[open_count++] = side;
        }
        if (open_count > 0) {
            int side_index = static_cast<int>((p_chunk.variant >> 8) % open_count);
            if (count == 2 && open_count == 2) side_index = 1 - side_index;
            placement.side = open_sides[side_index];
        } else {
            placement.side = sides[(p_chunk.variant >> 8) % 4];
        }
    }

    const int axis = 10 + static_cast<int>((p_chunk.variant >> 16) % 4ULL);
    placement.valid = true;
    switch (placement.side) {
        case WorldCoords::NEIGH_NORTH:
            placement.cover_x = axis;
            placement.cover_y = 2;
            placement.lower_x = axis;
            placement.lower_y = 3;
            break;
        case WorldCoords::NEIGH_EAST:
            placement.cover_x = 21;
            placement.cover_y = axis;
            placement.lower_x = 20;
            placement.lower_y = axis;
            break;
        case WorldCoords::NEIGH_SOUTH:
            placement.cover_x = axis;
            placement.cover_y = 21;
            placement.lower_x = axis;
            placement.lower_y = 20;
            break;
        case WorldCoords::NEIGH_WEST:
            placement.cover_x = 2;
            placement.cover_y = axis;
            placement.lower_x = 3;
            placement.lower_y = axis;
            break;
    }
    return placement;
}

bool in_manhole_access(const ManholePlacement& p_access, int p_x, int p_y) {
    if (!p_access.valid) return false;
    switch (p_access.side) {
        case WorldCoords::NEIGH_NORTH:
            return p_y >= p_access.lower_y && p_y <= 8 && std::abs(p_x - p_access.lower_x) <= 1;
        case WorldCoords::NEIGH_EAST:
            return p_x >= 15 && p_x <= p_access.lower_x && std::abs(p_y - p_access.lower_y) <= 1;
        case WorldCoords::NEIGH_SOUTH:
            return p_y >= 15 && p_y <= p_access.lower_y && std::abs(p_x - p_access.lower_x) <= 1;
        case WorldCoords::NEIGH_WEST:
            return p_x >= p_access.lower_x && p_x <= 8 && std::abs(p_y - p_access.lower_y) <= 1;
    }
    return false;
}

bool is_wet_cell(int p_x, int p_y, uint8_t p_connections) {
    const bool in_center_x = p_x >= CENTER_LOW && p_x <= CENTER_HIGH;
    const bool in_center_y = p_y >= CENTER_LOW && p_y <= CENTER_HIGH;
    if (in_center_x && in_center_y) return true;

    if ((p_connections & WorldCoords::NEIGH_NORTH) != 0 && in_center_x && p_y <= CENTER_HIGH) return true;
    if ((p_connections & WorldCoords::NEIGH_EAST) != 0 && in_center_y && p_x >= CENTER_LOW) return true;
    if ((p_connections & WorldCoords::NEIGH_SOUTH) != 0 && in_center_x && p_y >= CENTER_LOW) return true;
    if ((p_connections & WorldCoords::NEIGH_WEST) != 0 && in_center_y && p_x <= CENTER_HIGH) return true;
    return false;
}

int distance_to_wet(int p_x, int p_y, uint8_t p_connections) {
    if (is_wet_cell(p_x, p_y, p_connections)) return 0;
    for (int distance = 1; distance <= 3; distance++) {
        for (int dy = -distance; dy <= distance; dy++) {
            for (int dx = -distance; dx <= distance; dx++) {
                if (std::max(std::abs(dx), std::abs(dy)) != distance) continue;
                if (is_wet_cell(p_x + dx, p_y + dy, p_connections)) return distance;
            }
        }
    }
    return 4;
}

bool bridge_at(int p_x, int p_y, const SewerChunkDescriptor& p_chunk) {
    const bool north_south = (p_chunk.connections & (WorldCoords::NEIGH_NORTH | WorldCoords::NEIGH_SOUTH)) ==
        (WorldCoords::NEIGH_NORTH | WorldCoords::NEIGH_SOUTH);
    const bool east_west = (p_chunk.connections & (WorldCoords::NEIGH_EAST | WorldCoords::NEIGH_WEST)) ==
        (WorldCoords::NEIGH_EAST | WorldCoords::NEIGH_WEST);
    const int count = connection_count(p_chunk.connections);
    const int offset = (p_chunk.variant & 1ULL) != 0 ? 7 : 16;

    if (count == 2 && east_west && !north_south && (p_chunk.variant % 3ULL) == 0 && p_x >= offset && p_x <= offset + 1) {
        return true;
    }
    if (count == 2 && north_south && !east_west && (p_chunk.variant % 3ULL) == 0 && p_y >= offset && p_y <= offset + 1) {
        return true;
    }
    if (count >= 3) {
        WorldCoords::NeighborBits connected_arms[4];
        int arm_count = 0;
        const WorldCoords::NeighborBits arms[4] = {
            WorldCoords::NEIGH_NORTH,
            WorldCoords::NEIGH_EAST,
            WorldCoords::NEIGH_SOUTH,
            WorldCoords::NEIGH_WEST
        };
        for (WorldCoords::NeighborBits arm : arms) {
            if ((p_chunk.connections & arm) != 0) connected_arms[arm_count++] = arm;
        }

        const WorldCoords::NeighborBits selected_arm = connected_arms[(p_chunk.variant >> 2) % arm_count];
        switch (selected_arm) {
            case WorldCoords::NEIGH_NORTH: return p_y == 5 || p_y == 6;
            case WorldCoords::NEIGH_EAST: return p_x == 17 || p_x == 18;
            case WorldCoords::NEIGH_SOUTH: return p_y == 17 || p_y == 18;
            case WorldCoords::NEIGH_WEST: return p_x == 5 || p_x == 6;
        }
    }
    return false;
}

bool contaminated_cell(int p_world_x, int p_world_y, int p_world_seed, uint8_t p_danger, bool p_dry) {
    const int threshold = p_dry ? 58 : 32;
    if (p_danger <= threshold) return false;
    const int max_chance = p_dry ? 22 : 78;
    const int chance = std::min(max_chance, (static_cast<int>(p_danger) - threshold) * (p_dry ? 1 : 2));
    const uint64_t hash = Rng::at(
        static_cast<uint32_t>(p_world_seed),
        Vector2i(p_world_x, p_world_y),
        Rng::BIOME,
        SEWER_CELL_SALT
    ).state;
    return static_cast<int>(hash % 100ULL) < chance;
}

}

SewerRoomPlacement SewerGenerator::get_room_placement(const SewerChunkDescriptor& p_chunk) const {
    auto placement_for_side = [](WorldCoords::NeighborBits p_side) {
        SewerRoomPlacement placement;
        placement.valid = true;
        switch (p_side) {
            case WorldCoords::NEIGH_NORTH:
                placement.interior_x = 7;
                placement.interior_y = 1;
                placement.interior_width = 10;
                placement.interior_height = 5;
                placement.rotation = WorldCoords::ROT_SOUTH;
                break;
            case WorldCoords::NEIGH_EAST:
                placement.interior_x = 18;
                placement.interior_y = 7;
                placement.interior_width = 5;
                placement.interior_height = 10;
                placement.rotation = WorldCoords::ROT_WEST;
                break;
            case WorldCoords::NEIGH_SOUTH:
                placement.interior_x = 7;
                placement.interior_y = 18;
                placement.interior_width = 10;
                placement.interior_height = 5;
                placement.rotation = WorldCoords::ROT_NORTH;
                break;
            case WorldCoords::NEIGH_WEST:
                placement.interior_x = 1;
                placement.interior_y = 7;
                placement.interior_width = 5;
                placement.interior_height = 10;
                placement.rotation = WorldCoords::ROT_EAST;
                break;
        }
        return placement;
    };

    // Endpoints and access shafts always get a staging room. Other eligible
    // sewer chunks receive one 30% of the time.
    const bool should_have_room = p_chunk.endpoint || p_chunk.manhole || (p_chunk.variant % 10ULL) < 3;
    if (!should_have_room) return {};

    const int count = connection_count(p_chunk.connections);
    const bool north_south = (p_chunk.connections & (WorldCoords::NEIGH_NORTH | WorldCoords::NEIGH_SOUTH)) ==
        (WorldCoords::NEIGH_NORTH | WorldCoords::NEIGH_SOUTH);
    const bool east_west = (p_chunk.connections & (WorldCoords::NEIGH_EAST | WorldCoords::NEIGH_WEST)) ==
        (WorldCoords::NEIGH_EAST | WorldCoords::NEIGH_WEST);

    if (count == 2 && east_west && !north_south) {
        return placement_for_side((p_chunk.variant & 8ULL) != 0 ? WorldCoords::NEIGH_SOUTH : WorldCoords::NEIGH_NORTH);
    }
    if (count == 2 && north_south && !east_west) {
        return placement_for_side((p_chunk.variant & 8ULL) != 0 ? WorldCoords::NEIGH_EAST : WorldCoords::NEIGH_WEST);
    }
    if (count == 1) {
        if ((p_chunk.connections & WorldCoords::NEIGH_NORTH) != 0) return placement_for_side(WorldCoords::NEIGH_SOUTH);
        if ((p_chunk.connections & WorldCoords::NEIGH_SOUTH) != 0) return placement_for_side(WorldCoords::NEIGH_NORTH);
        if ((p_chunk.connections & WorldCoords::NEIGH_WEST) != 0) return placement_for_side(WorldCoords::NEIGH_EAST);
        return placement_for_side(WorldCoords::NEIGH_WEST);
    }
    if (count == 2 || count == 3) {
        WorldCoords::NeighborBits missing_sides[2];
        int missing_count = 0;
        const WorldCoords::NeighborBits sides[4] = {
            WorldCoords::NEIGH_NORTH,
            WorldCoords::NEIGH_EAST,
            WorldCoords::NEIGH_SOUTH,
            WorldCoords::NEIGH_WEST
        };
        for (WorldCoords::NeighborBits side : sides) {
            if ((p_chunk.connections & side) == 0) missing_sides[missing_count++] = side;
        }
        return placement_for_side(missing_sides[(p_chunk.variant >> 8) % missing_count]);
    }
    return {};
}

uint16_t SewerGenerator::get_tile(
    int p_world_x,
    int p_world_y,
    int p_z,
    int p_world_seed,
    const SewerChunkDescriptor& p_chunk
) const {
    const int x = floor_mod_chunk(p_world_x);
    const int y = floor_mod_chunk(p_world_y);
    const ManholePlacement access = get_manhole_placement(p_chunk);

    if (p_z == 0) {
        return access.valid && x == access.cover_x && y == access.cover_y ? tiles.manhole : tiles.void_tile;
    }

    if (p_z == -1) {
        if (!access.valid || std::abs(x - access.cover_x) > 2 || std::abs(y - access.cover_y) > 2) return tiles.void_tile;
        if (x == access.cover_x && y == access.cover_y) return tiles.stairs_up;
        if (x == access.lower_x && y == access.lower_y) return tiles.stairs_down;
        if (std::abs(x - access.cover_x) == 2 || std::abs(y - access.cover_y) == 2) return tiles.wall;
        return tiles.floor;
    }

    if (p_z != -2) return tiles.void_tile;

    if (access.valid && x == access.lower_x && y == access.lower_y) return tiles.stairs_up;
    if (in_manhole_access(access, x, y)) return tiles.floor;

    const SewerRoomPlacement room = get_room_placement(p_chunk);
    if (room.valid) {
        const int outer_min_x = room.interior_x - 1;
        const int outer_min_y = room.interior_y - 1;
        const int outer_max_x = room.interior_x + room.interior_width;
        const int outer_max_y = room.interior_y + room.interior_height;
        if (x >= outer_min_x && x <= outer_max_x && y >= outer_min_y && y <= outer_max_y) {
            bool wall = x == outer_min_x || x == outer_max_x || y == outer_min_y || y == outer_max_y;
            const int center_x_a = room.interior_x + room.interior_width / 2 - 1;
            const int center_x_b = center_x_a + 1;
            const int center_y_a = room.interior_y + room.interior_height / 2 - 1;
            const int center_y_b = center_y_a + 1;
            if (room.rotation == WorldCoords::ROT_SOUTH && y == outer_max_y && (x == center_x_a || x == center_x_b)) wall = false;
            if (room.rotation == WorldCoords::ROT_NORTH && y == outer_min_y && (x == center_x_a || x == center_x_b)) wall = false;
            if (room.rotation == WorldCoords::ROT_WEST && x == outer_min_x && (y == center_y_a || y == center_y_b)) wall = false;
            if (room.rotation == WorldCoords::ROT_EAST && x == outer_max_x && (y == center_y_a || y == center_y_b)) wall = false;
            if (wall) return tiles.wall;
            if (p_chunk.endpoint && contaminated_cell(p_world_x, p_world_y, p_world_seed, 100, true)) {
                return tiles.contaminated_floor;
            }
            return tiles.floor;
        }
    }

    const int distance = distance_to_wet(x, y, p_chunk.connections);
    if (distance == 0) {
        if (bridge_at(x, y, p_chunk)) return tiles.bridge;
        return contaminated_cell(p_world_x, p_world_y, p_world_seed, p_chunk.danger, false)
            ? tiles.contaminated_sewage
            : tiles.sewage;
    }
    if (distance <= 2) {
        return contaminated_cell(p_world_x, p_world_y, p_world_seed, p_chunk.danger, true)
            ? tiles.contaminated_floor
            : tiles.floor;
    }
    if (distance == 3) return tiles.wall;
    return tiles.void_tile;
}

}
