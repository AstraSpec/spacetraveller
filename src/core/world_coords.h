#ifndef SPACETRAVELLER_WORLD_COORDS_H
#define SPACETRAVELLER_WORLD_COORDS_H

#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/vector3i.hpp>
#include <cstdint>

namespace godot {

struct WorldCoords {
    static constexpr int REGION_SIZE = 256;
    static constexpr int CHUNK_SIZE = 24;

    static constexpr uint32_t ROTATION_MASK = 0x03;
    static constexpr uint32_t ORIENTATION_SHIFT = 16;
    static constexpr uint32_t ID_MASK = 0xFFFF;
    static constexpr uint32_t NEIGHBOR_SHIFT = 20;
    static constexpr uint32_t NEIGHBOR_MASK = 0x0F;

    enum Rotation {
        ROT_SOUTH = 0,
        ROT_WEST = 1,
        ROT_NORTH = 2,
        ROT_EAST = 3
    };

    enum NeighborBits {
        NEIGH_NORTH = 1 << 0,
        NEIGH_EAST = 1 << 1,
        NEIGH_SOUTH = 1 << 2,
        NEIGH_WEST = 1 << 3
    };

    static inline int chunk_coord(int value) {
        const int quotient = value / CHUNK_SIZE;
        const int remainder = value % CHUNK_SIZE;
        return remainder < 0 ? quotient - 1 : quotient;
    }

    static inline uint64_t pack_coords(int x, int y) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) |
               static_cast<uint64_t>(static_cast<uint32_t>(y));
    }

    static inline Vector2i unpack_coords(uint64_t key) {
        return Vector2i(
            static_cast<int>(static_cast<int32_t>(key >> 32)),
            static_cast<int>(static_cast<int32_t>(key & 0xFFFFFFFF))
        );
    }

    static inline uint64_t pack_coords_3d(int x, int y, int z) {
        const uint64_t ux = static_cast<uint64_t>(static_cast<uint32_t>(x) & 0xFFFFFFu);
        const uint64_t uy = static_cast<uint64_t>(static_cast<uint32_t>(y) & 0xFFFFFFu);
        const uint64_t uz = static_cast<uint64_t>(static_cast<uint32_t>(z) & 0xFFFFu);
        return (uz << 48) | (ux << 24) | uy;
    }

    static inline int sign_extend_24(uint32_t value) {
        return (value & 0x800000u) ? static_cast<int>(value | 0xFF000000u) : static_cast<int>(value);
    }

    static inline Vector3i unpack_coords_3d(uint64_t key) {
        return Vector3i(
            sign_extend_24(static_cast<uint32_t>((key >> 24) & 0xFFFFFFu)),
            sign_extend_24(static_cast<uint32_t>(key & 0xFFFFFFu)),
            static_cast<int>(static_cast<int16_t>((key >> 48) & 0xFFFFu))
        );
    }
};

}

#endif // SPACETRAVELLER_WORLD_COORDS_H
