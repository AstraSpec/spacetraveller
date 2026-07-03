#ifndef SPACETRAVELLER_LIGHT_LEVEL_H
#define SPACETRAVELLER_LIGHT_LEVEL_H

#include <cstdint>

namespace godot {

enum class LightLevel : uint8_t {
    Blank = 0,
    Low = 1,
    Lit = 2,
    Bright = 3
};

enum class LightFace : uint8_t {
    North = 0,
    East = 1,
    South = 2,
    West = 3
};

using LightStrength = uint16_t;

static constexpr LightStrength LIGHT_STRENGTH_BLANK = 0;
static constexpr LightStrength LIGHT_STRENGTH_LOW = 1;
static constexpr LightStrength LIGHT_STRENGTH_LIT = 4;
static constexpr LightStrength LIGHT_STRENGTH_BRIGHT = 6;
static constexpr LightStrength LIGHT_STRENGTH_DAYLIGHT = 13;
static constexpr LightStrength LIGHT_STRENGTH_FALLOFF_PER_TILE = 1;

inline LightLevel light_stronger(LightLevel a, LightLevel b) {
    return static_cast<uint8_t>(a) >= static_cast<uint8_t>(b) ? a : b;
}

inline LightStrength light_strength_stronger(LightStrength a, LightStrength b) {
    return a >= b ? a : b;
}

inline LightLevel light_level_from_strength(LightStrength p_strength) {
    if (p_strength >= LIGHT_STRENGTH_BRIGHT) {
        return LightLevel::Bright;
    }
    if (p_strength >= LIGHT_STRENGTH_LIT) {
        return LightLevel::Lit;
    }
    if (p_strength >= LIGHT_STRENGTH_LOW) {
        return LightLevel::Low;
    }
    return LightLevel::Blank;
}

inline bool light_is_perceptible(LightLevel p_level) {
    return static_cast<uint8_t>(p_level) >= static_cast<uint8_t>(LightLevel::Low);
}

inline bool light_reveals_detail(LightLevel p_level) {
    return static_cast<uint8_t>(p_level) >= static_cast<uint8_t>(LightLevel::Low);
}

inline bool light_reveals_dynamics(LightLevel p_level) {
    return static_cast<uint8_t>(p_level) >= static_cast<uint8_t>(LightLevel::Lit);
}

inline LightLevel dim_light(LightLevel p_level) {
    switch (p_level) {
        case LightLevel::Bright:
            return LightLevel::Lit;
        case LightLevel::Lit:
            return LightLevel::Low;
        case LightLevel::Low:
            return LightLevel::Blank;
        default:
            return LightLevel::Blank;
    }
}

struct LightSample {
    LightStrength cell = LIGHT_STRENGTH_BLANK;
    LightStrength north = LIGHT_STRENGTH_BLANK;
    LightStrength east = LIGHT_STRENGTH_BLANK;
    LightStrength south = LIGHT_STRENGTH_BLANK;
    LightStrength west = LIGHT_STRENGTH_BLANK;

    LightStrength get_face(LightFace p_face) const {
        switch (p_face) {
            case LightFace::North:
                return north;
            case LightFace::East:
                return east;
            case LightFace::South:
                return south;
            case LightFace::West:
                return west;
        }
        return LIGHT_STRENGTH_BLANK;
    }

    void raise_cell(LightStrength p_strength) {
        cell = light_strength_stronger(cell, p_strength);
    }

    void raise_face(LightFace p_face, LightStrength p_strength) {
        switch (p_face) {
            case LightFace::North:
                north = light_strength_stronger(north, p_strength);
                break;
            case LightFace::East:
                east = light_strength_stronger(east, p_strength);
                break;
            case LightFace::South:
                south = light_strength_stronger(south, p_strength);
                break;
            case LightFace::West:
                west = light_strength_stronger(west, p_strength);
                break;
        }
    }

    void raise_all_faces(LightStrength p_strength) {
        raise_face(LightFace::North, p_strength);
        raise_face(LightFace::East, p_strength);
        raise_face(LightFace::South, p_strength);
        raise_face(LightFace::West, p_strength);
    }

    LightStrength strongest_face() const {
        return light_strength_stronger(light_strength_stronger(north, east), light_strength_stronger(south, west));
    }
};

}

#endif // SPACETRAVELLER_LIGHT_LEVEL_H
