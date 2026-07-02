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

inline LightLevel light_stronger(LightLevel a, LightLevel b) {
    return static_cast<uint8_t>(a) >= static_cast<uint8_t>(b) ? a : b;
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
    LightLevel cell = LightLevel::Blank;
    LightLevel north = LightLevel::Blank;
    LightLevel east = LightLevel::Blank;
    LightLevel south = LightLevel::Blank;
    LightLevel west = LightLevel::Blank;

    LightLevel get_face(LightFace p_face) const {
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
        return LightLevel::Blank;
    }

    void raise_cell(LightLevel p_level) {
        cell = light_stronger(cell, p_level);
    }

    void raise_face(LightFace p_face, LightLevel p_level) {
        switch (p_face) {
            case LightFace::North:
                north = light_stronger(north, p_level);
                break;
            case LightFace::East:
                east = light_stronger(east, p_level);
                break;
            case LightFace::South:
                south = light_stronger(south, p_level);
                break;
            case LightFace::West:
                west = light_stronger(west, p_level);
                break;
        }
    }

    void raise_all_faces(LightLevel p_level) {
        raise_face(LightFace::North, p_level);
        raise_face(LightFace::East, p_level);
        raise_face(LightFace::South, p_level);
        raise_face(LightFace::West, p_level);
    }

    LightLevel strongest_face() const {
        return light_stronger(light_stronger(north, east), light_stronger(south, west));
    }
};

}

#endif // SPACETRAVELLER_LIGHT_LEVEL_H
