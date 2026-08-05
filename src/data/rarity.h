#ifndef SPACETRAVELLER_RARITY_H
#define SPACETRAVELLER_RARITY_H

#include <cstdint>
#include <godot_cpp/variant/string.hpp>

namespace godot {

enum class RarityTier : uint8_t {
    COMMON = 0,
    RARE = 1,
    EPIC = 2,
    LEGENDARY = 3,
};

inline bool rarity_from_string(const String& p_value, RarityTier& r_rarity) {
    const String value = p_value.to_lower().strip_edges();
    if (value == "common") {
        r_rarity = RarityTier::COMMON;
        return true;
    }
    if (value == "rare") {
        r_rarity = RarityTier::RARE;
        return true;
    }
    if (value == "epic") {
        r_rarity = RarityTier::EPIC;
        return true;
    }
    if (value == "legendary") {
        r_rarity = RarityTier::LEGENDARY;
        return true;
    }
    return false;
}

inline String rarity_to_string(RarityTier p_rarity) {
    switch (p_rarity) {
        case RarityTier::RARE: return "rare";
        case RarityTier::EPIC: return "epic";
        case RarityTier::LEGENDARY: return "legendary";
        default: return "common";
    }
}

inline String rarity_display_name(RarityTier p_rarity) {
    return rarity_to_string(p_rarity).capitalize();
}

inline bool rarity_meets(RarityTier p_available, RarityTier p_required) {
    return static_cast<uint8_t>(p_available) >= static_cast<uint8_t>(p_required);
}

}

#endif
