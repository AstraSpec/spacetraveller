#include "stamina.h"
#include <godot_cpp/variant/variant.hpp>
#include <algorithm>

using namespace godot;

void Stamina::init(StaminaData& data, float max_stamina) {
    data.max_stamina = max_stamina;
    data.current_stamina = max_stamina;
}

void Stamina::drain(StaminaData& data, float amount) {
    data.current_stamina = std::max(0.0f, data.current_stamina - amount);
}

void Stamina::regen(StaminaData& data, float amount) {
    data.current_stamina = std::min(data.max_stamina, data.current_stamina + amount);
}

bool Stamina::can_afford(const StaminaData& data, float cost) {
    return data.current_stamina >= cost;
}

float Stamina::get_percent(const StaminaData& data) {
    if (data.max_stamina <= 0.0f) return 0.0f;
    return data.current_stamina / data.max_stamina;
}

float Stamina::move_cost_multiplier(const StaminaData& data) {
    float pct = get_percent(data);
    if (pct >= StaminaTuning::MOVE_PENALTY_THRESHOLD) return 1.0f;
    // Scale linearly from 1.0 at the threshold up to MOVE_PENALTY_MAX at 0%.
    float t = pct / StaminaTuning::MOVE_PENALTY_THRESHOLD; // 1.0 at threshold, 0.0 at empty
    return 1.0f + (1.0f - t) * (StaminaTuning::MOVE_PENALTY_MAX - 1.0f);
}

Dictionary Stamina::serialize(const StaminaData& data) {
    Dictionary d;
    d["current_stamina"] = data.current_stamina;
    d["max_stamina"] = data.max_stamina;
    return d;
}

void Stamina::deserialize(StaminaData& data, const Dictionary& dict) {
    data.current_stamina = static_cast<float>(static_cast<double>(dict.get("current_stamina", 100.0)));
    data.max_stamina = static_cast<float>(static_cast<double>(dict.get("max_stamina", 100.0)));
}
