#ifndef SPACETRAVELLER_LIGHT_EMISSION_H
#define SPACETRAVELLER_LIGHT_EMISSION_H

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <cstdint>
#include <limits>

namespace godot {

struct LightEmissionInfo {
    uint16_t strength = 0;

    bool emits() const {
        return strength > 0;
    }
};

inline LightEmissionInfo parse_light_emission(const Variant& p_light) {
    LightEmissionInfo info;
    if (p_light.get_type() != Variant::DICTIONARY) {
        return info;
    }

    Dictionary light = p_light;
    int strength = static_cast<int>(light.get("strength", 0));
    if (strength < 0) {
        strength = 0;
    } else if (strength > std::numeric_limits<uint16_t>::max()) {
        strength = std::numeric_limits<uint16_t>::max();
    }
    info.strength = static_cast<uint16_t>(strength);
    return info;
}

}

#endif // SPACETRAVELLER_LIGHT_EMISSION_H
