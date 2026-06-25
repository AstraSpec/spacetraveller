#ifndef SPACETRAVELLER_COMBAT_RESOLVER_H
#define SPACETRAVELLER_COMBAT_RESOLVER_H

#include "core/rng.h"
#include <godot_cpp/variant/string.hpp>

namespace godot {

struct AnatomyData;
struct ClothingData;
struct EquipmentData;
struct HealthData;
struct StaminaData;
struct StyleInfo;

namespace CombatTuning {
    constexpr float CRIT_CHANCE = 0.2f;
    constexpr float CRIT_MULT = 2.0f;
    constexpr float DAMAGE_VARIANCE_MIN = 0.75f;
    constexpr float DAMAGE_VARIANCE_MAX = 1.25f;
}

struct CombatContext {
    const AnatomyData& attacker_anatomy;
    AnatomyData& defender_anatomy;
    HealthData& defender_health;
    EquipmentData& attacker_equipment;
    const ClothingData* defender_clothing = nullptr;
    Rng::Seeded& rng;
    float base_damage = 10.0f;
    const StyleInfo* style = nullptr;
    StaminaData* attacker_stamina = nullptr;
};

struct CombatOutcome {
    bool hit = false;
    bool killed = false;
    bool crit = false;
    bool exhausted = false;
    bool no_limbs = false;
    float damage = 0.0f;
    float speed = 1.0f;
    String verb;
    String part_name;
    int hit_part_index = -1;
    String hit_part_type;
    String effect_type;
    String effect_mode;
    float effect_magnitude = 0.0f;
    float effect_duration = 0.0f;
};

namespace CombatResolver {
    CombatOutcome resolve_attack(const CombatContext& ctx);
}

}

#endif // SPACETRAVELLER_COMBAT_RESOLVER_H
