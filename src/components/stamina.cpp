#include "stamina.h"
#include "combat_math.h"
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

void Stamina::recover_for_time(StaminaData& data, float elapsed, float multiplier) {
    if (elapsed <= 0.0f || multiplier <= 0.0f) return;
    regen(data, CombatMath::stamina_recovery(
        data.max_stamina, elapsed, multiplier));
}

float Stamina::combat_accuracy_modifier(const StaminaData& data) {
    return CombatMath::stamina_accuracy_modifier(get_percent(data));
}

float Stamina::combat_speed_multiplier(const StaminaData& data) {
    return CombatMath::stamina_speed_multiplier(get_percent(data));
}

float Stamina::moving_capacity_factor(const StaminaData& data) {
    float pct = get_percent(data);
    if (pct >= StaminaTuning::MOVE_PENALTY_THRESHOLD) return 1.0f;
    const float t = pct / StaminaTuning::MOVE_PENALTY_THRESHOLD;
    const float cost_multiplier =
        1.0f + (1.0f - t) * (StaminaTuning::MOVE_PENALTY_MAX - 1.0f);
    return 1.0f / cost_multiplier;
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
