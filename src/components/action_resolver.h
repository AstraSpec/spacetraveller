#ifndef SPACETRAVELLER_ACTION_RESOLVER_H
#define SPACETRAVELLER_ACTION_RESOLVER_H

#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/string.hpp>
#include <cstdint>

namespace godot {

class WorldBubble;
struct Entity;
struct LocomotionData;
struct HealthData;
struct EquipmentData;
struct AnatomyData;
struct StyleInfo;
struct StaminaData;

enum class IntentType { NONE, MOVE, ATTACK, SMASH, PICKUP };

struct Intent {
    IntentType type = IntentType::NONE;
    Vector2i target;
    String param;
};

struct AttackResult {
    bool hit = false;
    bool killed = false;
    bool crit = false;
    bool exhausted = false;
    float damage = 0.0f;
    float speed = 1.0f;
    String verb;
    String part_name;
};

// Action duration costs, expressed in in-game seconds.
// Convention: TimeManager treats 1 cost unit == 1 in-game second; an entity with
// speed s consumes (cost / s) seconds of simulated time per action.
// These replace the previous 150/200/100 literals (a deliberate gameplay rebalance,
// reducing the attack/smash/pickup durations by a factor of 100).
namespace ActionCost {
    inline constexpr float ATTACK = 1.5f; // in-game seconds (was 150)
    inline constexpr float SMASH  = 2.0f; // in-game seconds (was 200)
    inline constexpr float PICKUP = 1.0f; // in-game seconds (was 100)
}

namespace CombatTuning {
    inline constexpr float CRIT_CHANCE = 0.2f;
    inline constexpr float CRIT_MULT = 2.0f;
    inline constexpr float DAMAGE_VARIANCE_MIN = 0.75f;
    inline constexpr float DAMAGE_VARIANCE_MAX = 1.25f;
    inline constexpr float SMASH_FAIL_CHANCE = 0.5f;
}

namespace ActionResolver {
    float resolve_move(const Intent& intent, Entity& entity, WorldBubble& bubble, LocomotionData& loco);
    AttackResult resolve_attack(const AnatomyData& attacker_anatomy, AnatomyData& defender_anatomy, HealthData& defender_health, EquipmentData& attacker_equip, float base_damage = 10.0f, const StyleInfo* style = nullptr, StaminaData* attacker_stamina = nullptr);
    float resolve_smash(const Intent& intent, Entity& entity, WorldBubble& bubble, const String& tile_db_path = "");
    float resolve_pickup(const Intent& intent, Entity& entity, WorldBubble& bubble, void* inventory = nullptr);
    float resolve(uint32_t entity_id, const Intent& intent, WorldBubble& bubble, Entity& entity, LocomotionData& loco);
    bool is_hostile_entity_at(const WorldBubble& bubble, int x, int y, uint32_t self_id);
}

}

#endif // SPACETRAVELLER_ACTION_RESOLVER_H
