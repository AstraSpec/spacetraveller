#include "health.h"
#include <godot_cpp/variant/variant.hpp>
#include <algorithm>

using namespace godot;

void Health::init(HealthData& data, float max_blood) {
    data.max_blood = std::max(0.01f, max_blood);
    data.current_blood = data.max_blood;
    data.alive = true;
}

void Health::drain_blood(HealthData& data, float amount) {
    if (!data.alive) return;
    data.current_blood = std::max(0.0f, data.current_blood - std::max(0.0f, amount));
    if (data.current_blood <= 0.0f) {
        data.alive = false;
    }
}

void Health::kill(HealthData& data) {
    data.alive = false;
}

bool Health::is_alive(const HealthData& data) {
    return data.alive;
}

float Health::get_blood_percent(const HealthData& data) {
    if (data.max_blood <= 0.0f) return 0.0f;
    return data.current_blood / data.max_blood;
}

Dictionary Health::serialize(const HealthData& data) {
    Dictionary d;
    d["current_blood"] = data.current_blood;
    d["max_blood"] = data.max_blood;
    d["alive"] = data.alive;
    return d;
}

void Health::deserialize(HealthData& data, const Dictionary& dict, float default_max_blood) {
    const float fallback = std::max(0.01f, default_max_blood);
    data.alive = dict.get("alive", true);
    data.max_blood = std::max(0.01f, static_cast<float>(static_cast<double>(
        dict.get("max_blood", fallback))));
    data.current_blood = std::max(0.0f, static_cast<float>(static_cast<double>(
        dict.get("current_blood", data.max_blood))));
    data.current_blood = std::min(data.current_blood, data.max_blood);
    if (data.current_blood <= 0.0f) data.alive = false;
}
