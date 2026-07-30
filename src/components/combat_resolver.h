#ifndef SPACETRAVELLER_COMBAT_RESOLVER_H
#define SPACETRAVELLER_COMBAT_RESOLVER_H

#include "combat_math.h"
#include "equipment.h"
#include "core/rng.h"
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

struct AnatomyData;
struct ClothingData;
struct EquipmentData;
struct HealthData;
struct StaminaData;
struct StyleInfo;
struct CombatStateData;
struct PhysiologyData;
struct EffectsData;

namespace CombatTuning {
    constexpr float DAMAGE_VARIANCE_MIN = CombatMath::DAMAGE_VARIANCE_MIN;
    constexpr float DAMAGE_VARIANCE_MAX = CombatMath::DAMAGE_VARIANCE_MAX;
    constexpr float DEFAULT_COMBAT_SKILL = CombatMath::DEFAULT_COMBAT_SKILL;
    constexpr float SKILL_ACCURACY_PER_LEVEL = CombatMath::SKILL_ACCURACY_PER_LEVEL;
}

struct CombatContext {
    const AnatomyData& attacker_anatomy;
    AnatomyData& defender_anatomy;
    HealthData& defender_health;
    EquipmentData& attacker_equipment;
    const ClothingData* defender_clothing = nullptr;
    Rng::Seeded& rng;
    float base_damage = 10.0f;
    float attacker_combat_skill = CombatTuning::DEFAULT_COMBAT_SKILL;
    const StyleInfo* style = nullptr;
    StaminaData* attacker_stamina = nullptr;
    String forced_attack_id;
    int forced_body_part_index = -1;
    EquipmentData* defender_equipment = nullptr;
    CombatStateData* defender_combat_state = nullptr;
    float defender_movement_speed = 0.0f;
    PhysiologyData* attacker_physiology = nullptr;
    PhysiologyData* defender_physiology = nullptr;
    const EffectsData* defender_effects = nullptr;
    float attacker_effective_pain = 0.0f;
};

struct CombatOutcome {
    bool hit = false;
    bool killed = false;
    bool crit = false;
    bool dodged = false;
    bool deviated = false;
    bool exhausted = false;
    bool no_limbs = false;
    bool invalid_selection = false;
    bool incapacitated = false;
    bool newly_downed = false;
    bool consciousness_death = false;
    float damage = 0.0f;
    float pain_added = 0.0f;
    float consciousness_lost = 0.0f;
    float speed = 1.0f;
    float aim_penalty = 0.0f;
    float aim_time_multiplier = 1.0f;
    float base_accuracy = 0.0f;
    float effective_accuracy = 0.0f;
    float dodge_chance = 0.0f;
    float attack_margin = 0.0f;
    String verb;
    CombatMath::HitQuality quality = CombatMath::HitQuality::SOLID;
    String quality_name;
    String part_name;
    int hit_part_index = -1;
    int intended_part_index = -1;
    String hit_part_type;
    String effect_type;
    String effect_mode;
    float effect_magnitude = 0.0f;
    float effect_duration = 0.0f;
    std::vector<String> dropped_weapon_ids;
};

namespace CombatResolver {
    CombatOutcome resolve_attack(const CombatContext& ctx);
    Array get_attack_options(const CombatContext& ctx);
}

}

#endif // SPACETRAVELLER_COMBAT_RESOLVER_H
