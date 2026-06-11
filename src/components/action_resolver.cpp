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
#include "world/traversal_rules.h"
#include <cmath>

using namespace godot;

ActionResult ActionResolver::resolve_move(const Intent& intent, Entity& entity, WorldBubble& bubble, LocomotionData& loco, const EntityLedger* ledger) {
    int dx = abs(intent.target.x - entity.x);
    int dy = abs(intent.target.y - entity.y);

    if (dx > 1 || dy > 1 || (dx == 0 && dy == 0)) {
        return ActionResult::make_failure(ActionFailure::INVALID_TARGET);
    }

    uint16_t tile_id = bubble.query_tile_id(intent.target.x, intent.target.y);
    bool can_enter = ledger
        ? TraversalRules::can_enter(entity.id, tile_id, *ledger)
        : TraversalRules::can_profile_enter("walker", tile_id);
    if (!can_enter) {
        return ActionResult::make_failure(ActionFailure::BLOCKED_TILE);
    }

    const WorldBubble::CellEntity* occupant = bubble.get_entity_at(intent.target.x, intent.target.y);
    if (occupant) return ActionResult::make_failure(ActionFailure::OCCUPIED);

    int old_x = entity.x, old_y = entity.y;
    if (!bubble.update_entity_position(old_x, old_y, intent.target.x, intent.target.y, entity.id)) {
        return ActionResult::make_failure(ActionFailure::OCCUPIED);
    }
    entity.x = intent.target.x;
    entity.y = intent.target.y;

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

ActionResult ActionResolver::resolve(uint32_t entity_id, const Intent& intent, WorldBubble& bubble, Entity& entity, LocomotionData& loco, const EntityLedger* ledger) {
    switch (intent.type) {
        case IntentType::MOVE:
            return resolve_move(intent, entity, bubble, loco, ledger);

        case IntentType::SMASH:
            return resolve_smash(intent, entity, bubble);

        case IntentType::OPEN:
            return resolve_open(intent, entity, bubble);

        case IntentType::CLOSE:
            return resolve_close(intent, entity, bubble);

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
