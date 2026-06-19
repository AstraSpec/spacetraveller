#ifndef SPACETRAVELLER_ACTION_RESOLVER_H
#define SPACETRAVELLER_ACTION_RESOLVER_H

#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/string.hpp>
#include <cstdint>

namespace godot {

class WorldBubble;
class IGameEventListener;
class EntityLedger;
class EntityTracker;
struct Entity;
struct LocomotionData;
struct InventoryData;

enum class IntentType { NONE, MOVE, ATTACK, SMASH, PICKUP, CLOSE, OPEN, CHANGE_Z };

struct Intent {
    IntentType type = IntentType::NONE;
    Vector2i target;
    String param;
    int amount = 0;
};

enum class ActionFailure {
    NONE,
    INVALID_TARGET,
    BLOCKED_TILE,
    OCCUPIED,
    MISSING_COMPONENT,
    UNKNOWN_ITEM,
    NO_ITEMS,
    CARRY_LIMIT,
    EXHAUSTED,
    UNSUPPORTED
};

struct ActionResult {
    bool success = false;
    float cost = 0.0f;
    ActionFailure failure = ActionFailure::NONE;
    int amount = 0;

    static ActionResult make_success(float p_cost, int p_amount = 0) {
        ActionResult result;
        result.success = true;
        result.cost = p_cost;
        result.amount = p_amount;
        return result;
    }

    static ActionResult make_failure(ActionFailure p_failure) {
        ActionResult result;
        result.failure = p_failure;
        return result;
    }
};

struct MovePreview {
    bool can_move = false;
    bool will_fall = false;
    int from_z = 0;
    int to_z = 0;
    ActionFailure failure = ActionFailure::NONE;
};

namespace ActionCost {
    constexpr float ATTACK = 1.5f;
    constexpr float SMASH  = 2.0f;
    constexpr float PICKUP = 1.0f;
    constexpr float WAIT = 1.0f;
    constexpr float INTERACT = 1.0f;
}

namespace ActionTuning {
    constexpr float SMASH_FAIL_CHANCE = 0.5f;
}

namespace ActionResolver {
    MovePreview preview_move(const Intent& intent, const Entity& entity, WorldBubble& bubble, const EntityLedger* ledger = nullptr, EntityTracker* tracker = nullptr);
    ActionResult resolve_move(const Intent& intent, Entity& entity, WorldBubble& bubble, LocomotionData& loco, const EntityLedger* ledger = nullptr, EntityTracker* tracker = nullptr);
    ActionResult resolve_smash(const Intent& intent, Entity& entity, WorldBubble& bubble, const String& tile_db_path = "");
    ActionResult resolve_open(const Intent& intent, const Entity& entity, WorldBubble& bubble);
    ActionResult resolve_close(const Intent& intent, const Entity& entity, WorldBubble& bubble);
    ActionResult resolve_change_z(const Intent& intent, Entity& entity, WorldBubble& bubble, const EntityLedger* ledger = nullptr, EntityTracker* tracker = nullptr);
    ActionResult resolve_pickup(uint32_t picker_id, const Vector2i& pos, const String& item_id, int requested_amount, WorldBubble& bubble, InventoryData& inv, IGameEventListener* listener);
    ActionResult resolve(uint32_t entity_id, const Intent& intent, WorldBubble& bubble, Entity& entity, LocomotionData& loco, const EntityLedger* ledger = nullptr, EntityTracker* tracker = nullptr);
    bool is_hostile_entity_at(const WorldBubble& bubble, int x, int y, uint32_t self_id);
}

}

#endif // SPACETRAVELLER_ACTION_RESOLVER_H
