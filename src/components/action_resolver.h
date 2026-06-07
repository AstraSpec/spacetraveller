#ifndef SPACETRAVELLER_ACTION_RESOLVER_H
#define SPACETRAVELLER_ACTION_RESOLVER_H

#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/string.hpp>
#include <cstdint>

namespace godot {

class WorldBubble;
class IGameEventListener;
struct Entity;
struct LocomotionData;
struct HealthData;
struct EquipmentData;
struct AnatomyData;
struct StyleInfo;
struct StaminaData;
struct InventoryData;

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

struct PickupResult {
    int amount_picked = 0;   // 0 means nothing happened (no items, can't carry, etc.)
    bool success = false;
};

namespace ActionCost {
    constexpr float ATTACK = 1.5f;
    constexpr float SMASH  = 2.0f;
    constexpr float PICKUP = 1.0f;
    constexpr float WAIT = 1.0f;
}

namespace CombatTuning {
    constexpr float CRIT_CHANCE = 0.2f;
    constexpr float CRIT_MULT = 2.0f;
    constexpr float DAMAGE_VARIANCE_MIN = 0.75f;
    constexpr float DAMAGE_VARIANCE_MAX = 1.25f;
    constexpr float SMASH_FAIL_CHANCE = 0.5f;
}

namespace ActionResolver {
    float resolve_move(const Intent& intent, Entity& entity, WorldBubble& bubble, LocomotionData& loco);
    AttackResult resolve_attack(const AnatomyData& attacker_anatomy, AnatomyData& defender_anatomy, HealthData& defender_health, EquipmentData& attacker_equip, float base_damage = 10.0f, const StyleInfo* style = nullptr, StaminaData* attacker_stamina = nullptr);
    float resolve_smash(const Intent& intent, Entity& entity, WorldBubble& bubble, const String& tile_db_path = "");
    PickupResult resolve_pickup(uint32_t picker_id, const Vector2i& pos, const String& item_id, int requested_amount, WorldBubble& bubble, InventoryData& inv, IGameEventListener* listener);
    float resolve(uint32_t entity_id, const Intent& intent, WorldBubble& bubble, Entity& entity, LocomotionData& loco);
    bool is_hostile_entity_at(const WorldBubble& bubble, int x, int y, uint32_t self_id);
}

}

#endif // SPACETRAVELLER_ACTION_RESOLVER_H
