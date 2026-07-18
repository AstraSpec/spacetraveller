#ifndef SPACETRAVELLER_SEWER_GENERATOR_H
#define SPACETRAVELLER_SEWER_GENERATOR_H

#include <cstdint>

namespace godot {

struct SewerChunkDescriptor {
    uint8_t connections = 0;
    bool manhole = false;
    bool endpoint = false;
    uint8_t danger = 0;
    uint64_t variant = 0;
};

struct SewerRoomPlacement {
    bool valid = false;
    int interior_x = 0;
    int interior_y = 0;
    int interior_width = 0;
    int interior_height = 0;
    uint8_t rotation = 0;
};

struct SewerTileSet {
    uint16_t void_tile = 0;
    uint16_t wall = 0;
    uint16_t floor = 0;
    uint16_t sewage = 0;
    uint16_t contaminated_floor = 0;
    uint16_t contaminated_sewage = 0;
    uint16_t bridge = 0;
    uint16_t manhole = 0;
    uint16_t stairs_up = 0;
    uint16_t stairs_down = 0;
};

class SewerGenerator {
public:
    void setup(const SewerTileSet& p_tiles) { tiles = p_tiles; }
    SewerRoomPlacement get_room_placement(const SewerChunkDescriptor& p_chunk) const;
    uint16_t get_tile(
        int p_world_x,
        int p_world_y,
        int p_z,
        int p_world_seed,
        const SewerChunkDescriptor& p_chunk
    ) const;

private:
    SewerTileSet tiles;
};

}

#endif // SPACETRAVELLER_SEWER_GENERATOR_H
