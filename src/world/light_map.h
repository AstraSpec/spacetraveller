#ifndef SPACETRAVELLER_LIGHT_MAP_H
#define SPACETRAVELLER_LIGHT_MAP_H

#include "light_level.h"

#include <godot_cpp/variant/vector2i.hpp>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace godot {

struct LightEmitter {
    Vector2i position;
    int z = 0;
    LightStrength strength = LIGHT_STRENGTH_BLANK;
};

class LightMap {
public:
    using TileResolver = std::function<uint16_t(int world_x, int world_y, int world_z)>;
    using SkyResolver = std::function<bool(int world_x, int world_y, int world_z)>;

    static void compute_natural_light(
        const Vector2i& p_origin,
        int p_z,
        const std::vector<uint64_t>& p_offset_keys,
        const TileResolver& p_resolve_tile,
        const SkyResolver& p_is_sky_exposed,
        const std::vector<LightEmitter>& p_emitters,
        std::unordered_map<uint64_t, LightSample>& r_samples
    );

    static LightLevel get_level(
        const std::unordered_map<uint64_t, LightSample>& p_samples,
        uint64_t p_cell_key
    );

    static LightSample get_sample(
        const std::unordered_map<uint64_t, LightSample>& p_samples,
        uint64_t p_cell_key
    );
};

}

#endif // SPACETRAVELLER_LIGHT_MAP_H
