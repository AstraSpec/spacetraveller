#include "health.h"
#include <algorithm>

using namespace godot;

void Health::init(HealthData& data, float max_hp) {
    data.max_hp = max_hp;
    data.current_hp = max_hp;
    data.alive = true;
}

void Health::damage(HealthData& data, float amount) {
    if (!data.alive) return;
    data.current_hp = std::max(0.0f, data.current_hp - amount);
    if (data.current_hp <= 0.0f) {
        data.alive = false;
    }
}

void Health::heal(HealthData& data, float amount) {
    if (!data.alive) return;
    data.current_hp = std::min(data.max_hp, data.current_hp + amount);
}

bool Health::is_alive(const HealthData& data) {
    return data.alive;
}

float Health::get_percent(const HealthData& data) {
    if (data.max_hp <= 0.0f) return 0.0f;
    return data.current_hp / data.max_hp;
}
