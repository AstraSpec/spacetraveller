#ifndef SPACETRAVELLER_CELL_AREA_H
#define SPACETRAVELLER_CELL_AREA_H

#include "core/world_coords.h"

#include <godot_cpp/variant/vector2i.hpp>
#include <cstdint>
#include <vector>

namespace godot {

enum class CellAreaShape {
    Square,
    Circle
};

struct CellArea {
    Vector2i center;
    int z = 0;
    int radius = 0;
    CellAreaShape shape = CellAreaShape::Square;

    static CellArea square(const Vector2i& p_center, int p_z, int p_radius) {
        return CellArea{p_center, p_z, p_radius, CellAreaShape::Square};
    }

    static CellArea circle(const Vector2i& p_center, int p_z, int p_radius) {
        return CellArea{p_center, p_z, p_radius, CellAreaShape::Circle};
    }

    bool contains_offset(int ox, int oy) const {
        if (radius <= 0) {
            return ox == 0 && oy == 0;
        }

        if (shape == CellAreaShape::Square) {
            return ox >= -radius && ox < radius && oy >= -radius && oy < radius;
        }

        const int radius_sq = radius * radius;
        return ox * ox + oy * oy < radius_sq;
    }

    bool contains_world(int x, int y, int p_z) const {
        return p_z == z && contains_offset(x - center.x, y - center.y);
    }

    std::vector<uint64_t> offset_keys() const {
        std::vector<uint64_t> keys;
        if (radius <= 0) {
            keys.push_back(WorldCoords::pack_coords(0, 0));
            return keys;
        }

        const int diameter = radius * 2;
        keys.reserve(static_cast<size_t>(diameter) * static_cast<size_t>(diameter));
        for (int oy = -radius; oy < radius; oy++) {
            for (int ox = -radius; ox < radius; ox++) {
                if (contains_offset(ox, oy)) {
                    keys.push_back(WorldCoords::pack_coords(ox, oy));
                }
            }
        }
        return keys;
    }

    std::vector<uint64_t> world_keys() const {
        std::vector<uint64_t> keys;
        std::vector<uint64_t> offsets = offset_keys();
        keys.reserve(offsets.size());
        for (uint64_t offset_key : offsets) {
            Vector2i offset = WorldCoords::unpack_coords(offset_key);
            keys.push_back(WorldCoords::pack_coords_3d(center.x + offset.x, center.y + offset.y, z));
        }
        return keys;
    }
};

}

#endif // SPACETRAVELLER_CELL_AREA_H
