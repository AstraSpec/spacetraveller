#ifndef SPACETRAVELLER_COMBAT_FLOW_FIELD_H
#define SPACETRAVELLER_COMBAT_FLOW_FIELD_H

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <cstdint>
#include <unordered_map>

namespace godot {

class WorldBubble;

class CombatFlowField {
public:
    bool build(
        WorldBubble& bubble,
        const Vector2i& target,
        int z,
        int radius,
        const String& profile_id,
        bool allow_openable_tiles,
        uint64_t terrain_revision
    );

    bool matches(
        const Vector2i& target,
        int z,
        int radius,
        const String& profile_id,
        bool allow_openable_tiles,
        uint64_t terrain_revision
    ) const;

    bool get_step(const Vector2i& position, Vector2i& out_step) const;
    bool is_valid() const { return valid; }

private:
    bool valid = false;
    Vector2i target_position;
    int target_z = 0;
    int radius = 0;
    String profile_id;
    bool allow_openable_tiles = false;
    uint64_t terrain_revision = 0;
    std::unordered_map<uint64_t, Vector2i> next_steps;
};

}

#endif // SPACETRAVELLER_COMBAT_FLOW_FIELD_H
