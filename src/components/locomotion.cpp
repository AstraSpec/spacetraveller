#include "locomotion.h"
#include <godot_cpp/variant/variant.hpp>
#include <cmath>

using namespace godot;

void Locomotion::init(LocomotionData& data, float speed) {
    data.speed = speed;
    data.movement_mode = MovementMode::WALK;
    data.path.clear();
    data.path_index = 0;
    data.path_goal = Vector2i();
    data.path_goal_radius = 0;
}

bool Locomotion::movement_mode_from_id(const String& id, MovementMode& out_mode) {
    if (id == "walk") {
        out_mode = MovementMode::WALK;
        return true;
    }
    if (id == "run") {
        out_mode = MovementMode::RUN;
        return true;
    }
    if (id == "prone") {
        out_mode = MovementMode::PRONE;
        return true;
    }
    return false;
}

String Locomotion::movement_mode_id(MovementMode mode) {
    switch (mode) {
        case MovementMode::RUN: return "run";
        case MovementMode::PRONE: return "prone";
        case MovementMode::WALK:
        default: return "walk";
    }
}

String Locomotion::movement_mode_name(MovementMode mode) {
    switch (mode) {
        case MovementMode::RUN: return "Run";
        case MovementMode::PRONE: return "Prone";
        case MovementMode::WALK:
        default: return "Walk";
    }
}

float Locomotion::movement_mode_speed_multiplier(MovementMode mode) {
    switch (mode) {
        case MovementMode::RUN: return 2.0f;
        case MovementMode::PRONE: return 0.2f;
        case MovementMode::WALK:
        default: return 1.0f;
    }
}

void Locomotion::set_path(LocomotionData& data, const std::vector<Vector2i>& new_path, const Vector2i& goal, int goal_radius) {
    data.path = new_path;
    data.path_index = 0;
    data.path_goal = goal;
    data.path_goal_radius = std::max(0, goal_radius);
}

bool Locomotion::peek_next_step(const LocomotionData& data, Vector2i& out_tile) {
    if (data.path_index >= static_cast<int>(data.path.size())) return false;
    out_tile = data.path[data.path_index];
    return true;
}

void Locomotion::advance_step(LocomotionData& data) {
    if (data.path_index < static_cast<int>(data.path.size())) {
        data.path_index++;
    }
}

bool Locomotion::has_path(const LocomotionData& data) {
    return data.path_index < static_cast<int>(data.path.size());
}

void Locomotion::clear_path(LocomotionData& data) {
    data.path.clear();
    data.path_index = 0;
    data.path_goal = Vector2i();
    data.path_goal_radius = 0;
}

float Locomotion::get_step_cost(int from_x, int from_y, int to_x, int to_y) {
    int dx = abs(to_x - from_x);
    int dy = abs(to_y - from_y);
    if (dx == 0 && dy == 0) return 0.0f;
    return (dx == 1 && dy == 1) ? 1.414f : 1.0f;
}

Dictionary Locomotion::serialize(const LocomotionData& data) {
    Dictionary d;
    d["speed"] = data.speed;
    d["movement_mode"] = movement_mode_id(data.movement_mode);
    return d;
}

void Locomotion::deserialize(LocomotionData& data, const Dictionary& dict) {
    data.speed = static_cast<float>(static_cast<double>(dict.get("speed", 0.8)));
    data.movement_mode = MovementMode::WALK;
    MovementMode loaded_mode;
    if (movement_mode_from_id(String(dict.get("movement_mode", "walk")), loaded_mode)) {
        data.movement_mode = loaded_mode;
    }
    data.path.clear();
    data.path_index = 0;
    data.path_goal = Vector2i();
    data.path_goal_radius = 0;
}
