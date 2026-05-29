#include "locomotion.h"
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/array.hpp>
#include <cmath>

using namespace godot;

void Locomotion::init(LocomotionData& data, float speed) {
    data.speed = speed;
    data.path.clear();
    data.path_index = 0;
    data.path_goal = Vector2i();
}

void Locomotion::set_path(LocomotionData& data, const std::vector<Vector2i>& new_path, const Vector2i& goal) {
    data.path = new_path;
    data.path_index = 0;
    data.path_goal = goal;
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
    Array path_arr;
    for (const auto& p : data.path) path_arr.push_back(p);
    d["path"] = path_arr;
    d["path_index"] = data.path_index;
    return d;
}

void Locomotion::deserialize(LocomotionData& data, const Dictionary& dict) {
    data.speed = static_cast<float>(static_cast<double>(dict.get("speed", 0.8)));
    data.path.clear();
    Array path_arr = dict.get("path", Array());
    for (int i = 0; i < path_arr.size(); i++) {
        data.path.push_back(path_arr[i]);
    }
    data.path_index = static_cast<int>(dict.get("path_index", 0));
}
