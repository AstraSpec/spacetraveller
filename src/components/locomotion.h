#ifndef SPACETRAVELLER_LOCOMOTION_H
#define SPACETRAVELLER_LOCOMOTION_H

#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <cstdint>
#include <vector>

namespace godot {

class WorldBubble;

enum class MovementMode : uint8_t {
    WALK,
    RUN,
    PRONE
};

struct LocomotionData {
    float speed = 0.8f;
    MovementMode movement_mode = MovementMode::WALK;
    std::vector<Vector2i> path;
    int path_index = 0;
    Vector2i path_goal;
    int path_goal_radius = 0;
};

namespace Locomotion {
    void init(LocomotionData& data, float speed);
    bool movement_mode_from_id(const String& id, MovementMode& out_mode);
    String movement_mode_id(MovementMode mode);
    String movement_mode_name(MovementMode mode);
    float movement_mode_speed_multiplier(MovementMode mode);
    void set_path(LocomotionData& data, const std::vector<Vector2i>& new_path, const Vector2i& goal, int goal_radius = 0);
    bool peek_next_step(const LocomotionData& data, Vector2i& out_tile);
    void advance_step(LocomotionData& data);
    bool has_path(const LocomotionData& data);
    void clear_path(LocomotionData& data);
    float get_step_cost(int from_x, int from_y, int to_x, int to_y);
    Dictionary serialize(const LocomotionData& data);
    void deserialize(LocomotionData& data, const Dictionary& dict);
}

namespace MovementTuning {
    inline constexpr float RUN_STAMINA_PER_CARDINAL_STEP = 2.0f;
    inline constexpr float RUN_MIN_STAMINA_PERCENT = 0.25f;
}

}

#endif // SPACETRAVELLER_LOCOMOTION_H
