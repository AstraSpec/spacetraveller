#ifndef SPACETRAVELLER_LOCOMOTION_H
#define SPACETRAVELLER_LOCOMOTION_H

#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <cstdint>
#include <vector>

namespace godot {

class WorldBubble;

struct LocomotionData {
    float speed = 0.8f;
    std::vector<Vector2i> path;
    int path_index = 0;
    Vector2i path_goal;
};

namespace Locomotion {
    void init(LocomotionData& data, float speed);
    void set_path(LocomotionData& data, const std::vector<Vector2i>& new_path, const Vector2i& goal);
    bool peek_next_step(const LocomotionData& data, Vector2i& out_tile);
    void advance_step(LocomotionData& data);
    bool has_path(const LocomotionData& data);
    void clear_path(LocomotionData& data);
    float get_step_cost(int from_x, int from_y, int to_x, int to_y);
    Dictionary serialize(const LocomotionData& data);
    void deserialize(LocomotionData& data, const Dictionary& dict);
}

}

#endif // SPACETRAVELLER_LOCOMOTION_H
