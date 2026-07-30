#ifndef SPACETRAVELLER_STAMINA_H
#define SPACETRAVELLER_STAMINA_H

#include "combat_math.h"
#include <godot_cpp/variant/dictionary.hpp>

namespace godot {

struct StaminaData {
    float current_stamina = 100.0f;
    float max_stamina = 100.0f;
};

namespace StaminaTuning {
    inline constexpr float REGEN_FRACTION_PER_TIME =
        CombatMath::STAMINA_REGEN_FRACTION_PER_TIME;
    inline constexpr float MOVE_PENALTY_THRESHOLD = 0.5f;
    inline constexpr float MOVE_PENALTY_MAX = 3.0f;
    inline constexpr float SMASH_COST = 5.0f;
}

namespace Stamina {
    void init(StaminaData& data, float max_stamina);
    void drain(StaminaData& data, float amount);
    void regen(StaminaData& data, float amount);
    bool can_afford(const StaminaData& data, float cost);
    float get_percent(const StaminaData& data);
    void recover_for_time(StaminaData& data, float elapsed, float multiplier = 1.0f);
    float combat_accuracy_modifier(const StaminaData& data);
    float combat_speed_multiplier(const StaminaData& data);
    float moving_capacity_factor(const StaminaData& data);
    Dictionary serialize(const StaminaData& data);
    void deserialize(StaminaData& data, const Dictionary& dict);
}

}

#endif // SPACETRAVELLER_STAMINA_H
