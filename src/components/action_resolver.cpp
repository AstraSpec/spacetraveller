#include "action_resolver.h"
#include "entities/entity.h"
#include "world/world_bubble.h"
#include "sim/game_event.h"
#include "components/inventory.h"
#include "core/id_registry.h"
#include "locomotion.h"
#include "data/tile_db.h"
#include "core/tag_registry.h"
#include "entities/entity_ledger.h"
#include "entities/entity_tracker.h"
#include "world/traversal_rules.h"
#include <cmath>
#include <godot_cpp/variant/vector3i.hpp>

using namespace godot;

namespace {

bool tile_is_air(uint16_t tile_id) {
    IdRegistry* id_reg = IdRegistry::get_singleton();
    return id_reg && tile_id == id_reg->get_id("air");
}

bool can_enter_tile(uint32_t entity_id, uint16_t tile_id, const EntityLedger* ledger) {
    return ledger
        ? TraversalRules::can_enter(entity_id, tile_id, *ledger)
        : TraversalRules::can_profile_enter("walker", tile_id);
}

bool find_fall_landing(
    uint32_t entity_id,
    int x,
    int y,
    int start_z,
    WorldBubble& bubble,
    const EntityLedger* ledger,
    int& r_landing_z
) {
    static constexpr int MAX_FALL_SCAN_DEPTH = 64;
    for (int depth = 1; depth <= MAX_FALL_SCAN_DEPTH; depth++) {
        const int candidate_z = start_z - depth;
        const uint16_t tile_id = bubble.query_tile_id_at_z(x, y, candidate_z);
        if (tile_id == 0 || tile_is_air(tile_id)) {
            continue;
        }
        if (!can_enter_tile(entity_id, tile_id, ledger)) {
            return false;
        }

        r_landing_z = candidate_z;
        return true;
    }

    return false;
}

}

ActionResult ActionResolver::resolve_move(const Intent& intent, Entity& entity, WorldBubble& bubble, LocomotionData& loco, const EntityLedger* ledger, EntityTracker* tracker) {
    int dx = abs(intent.target.x - entity.x);
    int dy = abs(intent.target.y - entity.y);

    if (dx > 1 || dy > 1 || (dx == 0 && dy == 0)) {
        return ActionResult::make_failure(ActionFailure::INVALID_TARGET);
    }

    uint16_t tile_id = bubble.query_tile_id(intent.target.x, intent.target.y);
    int target_z = entity.z;
    if (tile_is_air(tile_id)) {
        if (!find_fall_landing(entity.id, intent.target.x, intent.target.y, entity.z, bubble, ledger, target_z)) {
            return ActionResult::make_failure(ActionFailure::BLOCKED_TILE);
        }
    } else if (!can_enter_tile(entity.id, tile_id, ledger)) {
        return ActionResult::make_failure(ActionFailure::BLOCKED_TILE);
    }

    const WorldBubble::CellEntity* occupant = bubble.get_entity_at(intent.target.x, intent.target.y);
    if (occupant) return ActionResult::make_failure(ActionFailure::OCCUPIED);
    if (target_z != entity.z && tracker) {
        const uint32_t landing_occupant = tracker->get_at(Vector3i(intent.target.x, intent.target.y, target_z));
        if (landing_occupant != INVALID_ENTITY_ID && landing_occupant != entity.id) {
            return ActionResult::make_failure(ActionFailure::OCCUPIED);
        }
    }

    int old_x = entity.x, old_y = entity.y, old_z = entity.z;
    if (target_z == old_z) {
        if (!bubble.update_entity_position(old_x, old_y, intent.target.x, intent.target.y, entity.id)) {
            return ActionResult::make_failure(ActionFailure::OCCUPIED);
        }
        if (tracker && !tracker->move(entity.id, Vector3i(old_x, old_y, old_z), Vector3i(intent.target.x, intent.target.y, old_z))) {
            bubble.update_entity_position(intent.target.x, intent.target.y, old_x, old_y, entity.id);
            return ActionResult::make_failure(ActionFailure::OCCUPIED);
        }
    } else {
        if (!tracker) {
            return ActionResult::make_failure(ActionFailure::MISSING_COMPONENT);
        }
        const Vector3i old_pos(old_x, old_y, old_z);
        const Vector3i new_pos(intent.target.x, intent.target.y, target_z);
        const int previous_active_z = bubble.get_active_z();
        bubble.set_active_z(old_z);
        bubble.remove_entity(old_x, old_y);
        bubble.set_active_z(previous_active_z);

        if (!tracker->move(entity.id, old_pos, new_pos)) {
            const int restore_active_z = bubble.get_active_z();
            bubble.set_active_z(old_z);
            bubble.set_entity(old_x, old_y, entity.id);
            bubble.set_active_z(restore_active_z);
            return ActionResult::make_failure(ActionFailure::OCCUPIED);
        }

        const int set_active_z = bubble.get_active_z();
        bubble.set_active_z(target_z);
        bool set_target = bubble.set_entity(intent.target.x, intent.target.y, entity.id);
        bubble.set_active_z(set_active_z);
        if (!set_target) {
            tracker->move(entity.id, new_pos, old_pos);
            const int restore_active_z = bubble.get_active_z();
            bubble.set_active_z(old_z);
            bubble.set_entity(old_x, old_y, entity.id);
            bubble.set_active_z(restore_active_z);
            return ActionResult::make_failure(ActionFailure::OCCUPIED);
        }
    }
    entity.x = intent.target.x;
    entity.y = intent.target.y;
    entity.z = target_z;

    Locomotion::advance_step(loco);

    return ActionResult::make_success(Locomotion::get_step_cost(old_x, old_y, intent.target.x, intent.target.y));
}

ActionResult ActionResolver::resolve_smash(const Intent& intent, Entity& entity, WorldBubble& bubble, const String& tile_db_path) {
    int dx = abs(intent.target.x - entity.x);
    int dy = abs(intent.target.y - entity.y);
    if (dx > 1 || dy > 1 || (dx == 0 && dy == 0)) {
        return ActionResult::make_failure(ActionFailure::INVALID_TARGET);
    }

    bubble.place_tile(intent.target.x, intent.target.y, "dirt", WorldBubble::LAYER_TILE);

    return ActionResult::make_success(ActionCost::SMASH);
}

ActionResult ActionResolver::resolve_open(const Intent& intent, const Entity& entity, WorldBubble& bubble) {
    int dx = abs(intent.target.x - entity.x);
    int dy = abs(intent.target.y - entity.y);
    if (dx > 1 || dy > 1 || (dx == 0 && dy == 0)) {
        return ActionResult::make_failure(ActionFailure::INVALID_TARGET);
    }
    if (bubble.get_entity_at(intent.target.x, intent.target.y)) {
        return ActionResult::make_failure(ActionFailure::OCCUPIED);
    }

    TileDb* tile_db = TileDb::get_singleton();
    TagRegistry* tag_reg = TagRegistry::get_singleton();
    if (!tile_db || !tag_reg) return ActionResult::make_failure(ActionFailure::MISSING_COMPONENT);

    uint16_t tile_id = bubble.query_tile_id(intent.target.x, intent.target.y);
    uint16_t can_open = tag_reg->get_tag_id("CAN_OPEN");
    const TileInfo* info = tile_db->get_tile_info(tile_id);
    if (!info || can_open == 0 || info->opens_to == 0 || !tile_db->has_tag(tile_id, can_open)) {
        return ActionResult::make_failure(ActionFailure::BLOCKED_TILE);
    }

    bubble.place_tile_id(intent.target.x, intent.target.y, info->opens_to);
    return ActionResult::make_success(ActionCost::INTERACT);
}

ActionResult ActionResolver::resolve_close(const Intent& intent, const Entity& entity, WorldBubble& bubble) {
    int dx = abs(intent.target.x - entity.x);
    int dy = abs(intent.target.y - entity.y);
    if (dx > 1 || dy > 1 || (dx == 0 && dy == 0)) {
        return ActionResult::make_failure(ActionFailure::INVALID_TARGET);
    }
    if (bubble.get_entity_at(intent.target.x, intent.target.y)) {
        return ActionResult::make_failure(ActionFailure::OCCUPIED);
    }

    TileDb* tile_db = TileDb::get_singleton();
    TagRegistry* tag_reg = TagRegistry::get_singleton();
    if (!tile_db || !tag_reg) return ActionResult::make_failure(ActionFailure::MISSING_COMPONENT);

    uint16_t tile_id = bubble.query_tile_id(intent.target.x, intent.target.y);
    uint16_t can_close = tag_reg->get_tag_id("CAN_CLOSE");
    const TileInfo* info = tile_db->get_tile_info(tile_id);
    if (!info || can_close == 0 || info->closes_to == 0 || !tile_db->has_tag(tile_id, can_close)) {
        return ActionResult::make_failure(ActionFailure::BLOCKED_TILE);
    }

    bubble.place_tile_id(intent.target.x, intent.target.y, info->closes_to);
    return ActionResult::make_success(ActionCost::INTERACT);
}

ActionResult ActionResolver::resolve_change_z(const Intent& intent, Entity& entity, WorldBubble& bubble, const EntityLedger* ledger, EntityTracker* tracker) {
    const int delta = intent.amount;
    if (delta != 1 && delta != -1) {
        return ActionResult::make_failure(ActionFailure::INVALID_TARGET);
    }
    if (!ledger || !tracker) {
        return ActionResult::make_failure(ActionFailure::MISSING_COMPONENT);
    }

    TileDb* tile_db = TileDb::get_singleton();
    TagRegistry* tag_reg = TagRegistry::get_singleton();
    if (!tile_db || !tag_reg) {
        return ActionResult::make_failure(ActionFailure::MISSING_COMPONENT);
    }

    const uint16_t tile_id = bubble.query_tile_id_at_z(entity.x, entity.y, entity.z);
    const uint16_t required_tag = tag_reg->get_tag_id(delta > 0 ? "ASCEND_LEVEL" : "DESCENT_LEVEL");
    if (required_tag == 0 || !tile_db->has_tag(tile_id, required_tag)) {
        return ActionResult::make_failure(ActionFailure::BLOCKED_TILE);
    }

    const int old_z = entity.z;
    const int new_z = old_z + delta;
    const Vector3i old_pos(entity.x, entity.y, old_z);
    const Vector3i new_pos(entity.x, entity.y, new_z);
    const uint32_t occupant = tracker->get_at(new_pos);
    if (occupant != INVALID_ENTITY_ID && occupant != entity.id) {
        return ActionResult::make_failure(ActionFailure::OCCUPIED);
    }

    const uint16_t destination_tile_id = bubble.query_tile_id_at_z(entity.x, entity.y, new_z);
    if (destination_tile_id == 0 || !TraversalRules::can_enter(entity.id, destination_tile_id, *ledger)) {
        return ActionResult::make_failure(ActionFailure::BLOCKED_TILE);
    }

    if (!tracker->move(entity.id, old_pos, new_pos)) {
        return ActionResult::make_failure(ActionFailure::OCCUPIED);
    }

    if (old_z == bubble.get_active_z()) {
        bubble.remove_entity(entity.x, entity.y);
    }
    entity.z = new_z;
    if (new_z == bubble.get_active_z()) {
        if (!bubble.set_entity(entity.x, entity.y, entity.id)) {
            entity.z = old_z;
            tracker->move(entity.id, new_pos, old_pos);
            return ActionResult::make_failure(ActionFailure::OCCUPIED);
        }
    }

    return ActionResult::make_success(ActionCost::INTERACT);
}

ActionResult ActionResolver::resolve_pickup(uint32_t picker_id, const Vector2i& pos, const String& item_id, int requested_amount, WorldBubble& bubble, InventoryData& inv, IGameEventListener* listener) {
    if (requested_amount <= 0 || item_id.is_empty()) {
        return ActionResult::make_failure(ActionFailure::INVALID_TARGET);
    }

    IdRegistry* reg = IdRegistry::get_singleton();
    if (!reg) return ActionResult::make_failure(ActionFailure::MISSING_COMPONENT);
    uint16_t numeric_id = reg->get_id(item_id);
    if (numeric_id == 0) return ActionResult::make_failure(ActionFailure::UNKNOWN_ITEM);

    int available = bubble.peek_item_amount(pos, numeric_id);
    if (available <= 0) return ActionResult::make_failure(ActionFailure::NO_ITEMS);

    int to_pickup = MIN(requested_amount, available);
    if (!Inventory::add_item(inv, numeric_id, to_pickup)) {
        return ActionResult::make_failure(ActionFailure::CARRY_LIMIT);
    }

    bubble.remove_item(pos, numeric_id, to_pickup);

    if (listener) {
        GameEvent e;
        e.type = GameEventType::ITEM_PICKED_UP;
        e.subject_id = picker_id;
        e.item_id = numeric_id;
        e.amount = to_pickup;
        e.position = pos;
        listener->on_game_event(e);
    }

    return ActionResult::make_success(ActionCost::PICKUP, to_pickup);
}

ActionResult ActionResolver::resolve(uint32_t entity_id, const Intent& intent, WorldBubble& bubble, Entity& entity, LocomotionData& loco, const EntityLedger* ledger, EntityTracker* tracker) {
    switch (intent.type) {
        case IntentType::MOVE:
            return resolve_move(intent, entity, bubble, loco, ledger, tracker);

        case IntentType::SMASH:
            return resolve_smash(intent, entity, bubble);

        case IntentType::OPEN:
            return resolve_open(intent, entity, bubble);

        case IntentType::CLOSE:
            return resolve_close(intent, entity, bubble);

        case IntentType::CHANGE_Z:
            return resolve_change_z(intent, entity, bubble, ledger, tracker);

        case IntentType::ATTACK:
        case IntentType::PICKUP:
            return ActionResult::make_failure(ActionFailure::UNSUPPORTED);

        case IntentType::NONE:
            return ActionResult::make_success(ActionCost::WAIT);
        default:
            return ActionResult::make_failure(ActionFailure::UNSUPPORTED);
    }
}

bool ActionResolver::is_hostile_entity_at(const WorldBubble& bubble, int x, int y, uint32_t self_id) {
    const WorldBubble::CellEntity* occupant = bubble.get_entity_at(x, y);
    return occupant != nullptr && occupant->entity_id != self_id;
}
