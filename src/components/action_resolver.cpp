#include "action_resolver.h"
#include "entities/entity.h"
#include "world/world_bubble.h"
#include "sim/game_event.h"
#include "components/inventory.h"
#include "core/id_registry.h"
#include "locomotion.h"
#include "data/tile_db.h"
#include "core/tag_registry.h"
#include <cmath>

using namespace godot;

float ActionResolver::resolve_move(const Intent& intent, Entity& entity, WorldBubble& bubble, LocomotionData& loco) {
    int dx = abs(intent.target.x - entity.x);
    int dy = abs(intent.target.y - entity.y);

    if (dx > 1 || dy > 1 || (dx == 0 && dy == 0)) return 0.0f;

    if (TileDb* tile_db = TileDb::get_singleton()) {
        uint16_t tile_id = bubble.query_tile_id(intent.target.x, intent.target.y);
        if (tile_id != 0) {
            const TileInfo* info = tile_db->get_tile_info(tile_id);
            if (info && info->solid) return 0.0f;
        }
    }

    const WorldBubble::CellEntity* occupant = bubble.get_entity_at(intent.target.x, intent.target.y);
    if (occupant) return 0.0f;

    int old_x = entity.x, old_y = entity.y;
    if (!bubble.update_entity_position(old_x, old_y, intent.target.x, intent.target.y, entity.id)) {
        return 0.0f;
    }
    entity.x = intent.target.x;
    entity.y = intent.target.y;

    Locomotion::advance_step(loco);

    return Locomotion::get_step_cost(old_x, old_y, intent.target.x, intent.target.y);
}

float ActionResolver::resolve_smash(const Intent& intent, Entity& entity, WorldBubble& bubble, const String& tile_db_path) {
    int dx = abs(intent.target.x - entity.x);
    int dy = abs(intent.target.y - entity.y);
    if (dx > 1 || dy > 1 || (dx == 0 && dy == 0)) return 0.0f;

    bubble.place_tile(intent.target.x, intent.target.y, "dirt", WorldBubble::LAYER_TILE);

    return ActionCost::SMASH;
}

float ActionResolver::resolve_open(const Intent& intent, const Entity& entity, WorldBubble& bubble) {
    int dx = abs(intent.target.x - entity.x);
    int dy = abs(intent.target.y - entity.y);
    if (dx > 1 || dy > 1 || (dx == 0 && dy == 0)) return 0.0f;
    if (bubble.get_entity_at(intent.target.x, intent.target.y)) return 0.0f;

    TileDb* tile_db = TileDb::get_singleton();
    TagRegistry* tag_reg = TagRegistry::get_singleton();
    if (!tile_db || !tag_reg) return 0.0f;

    uint16_t tile_id = bubble.query_tile_id(intent.target.x, intent.target.y);
    uint16_t can_open = tag_reg->get_tag_id("CAN_OPEN");
    const TileInfo* info = tile_db->get_tile_info(tile_id);
    if (!info || can_open == 0 || info->opens_to == 0 || !tile_db->has_tag(tile_id, can_open)) {
        return 0.0f;
    }

    bubble.place_tile_id(intent.target.x, intent.target.y, info->opens_to);
    return ActionCost::INTERACT;
}

float ActionResolver::resolve_close(const Intent& intent, const Entity& entity, WorldBubble& bubble) {
    int dx = abs(intent.target.x - entity.x);
    int dy = abs(intent.target.y - entity.y);
    if (dx > 1 || dy > 1 || (dx == 0 && dy == 0)) return 0.0f;
    if (bubble.get_entity_at(intent.target.x, intent.target.y)) return 0.0f;

    TileDb* tile_db = TileDb::get_singleton();
    TagRegistry* tag_reg = TagRegistry::get_singleton();
    if (!tile_db || !tag_reg) return 0.0f;

    uint16_t tile_id = bubble.query_tile_id(intent.target.x, intent.target.y);
    uint16_t can_close = tag_reg->get_tag_id("CAN_CLOSE");
    const TileInfo* info = tile_db->get_tile_info(tile_id);
    if (!info || can_close == 0 || info->closes_to == 0 || !tile_db->has_tag(tile_id, can_close)) {
        return 0.0f;
    }

    bubble.place_tile_id(intent.target.x, intent.target.y, info->closes_to);
    return ActionCost::INTERACT;
}

PickupResult ActionResolver::resolve_pickup(uint32_t picker_id, const Vector2i& pos, const String& item_id, int requested_amount, WorldBubble& bubble, InventoryData& inv, IGameEventListener* listener) {
    PickupResult result;
    if (requested_amount <= 0 || item_id.is_empty()) return result;

    IdRegistry* reg = IdRegistry::get_singleton();
    if (!reg) return result;
    uint16_t numeric_id = reg->get_id(item_id);
    if (numeric_id == 0) return result;

    int available = bubble.peek_item_amount(pos, numeric_id);
    if (available <= 0) return result;

    int to_pickup = MIN(requested_amount, available);
    if (!Inventory::add_item(inv, numeric_id, to_pickup)) return result;

    bubble.remove_item(pos, numeric_id, to_pickup);

    result.amount_picked = to_pickup;
    result.success = true;

    if (listener) {
        GameEvent e;
        e.type = GameEventType::ITEM_PICKED_UP;
        e.subject_id = picker_id;
        e.item_id = numeric_id;
        e.amount = to_pickup;
        e.position = pos;
        listener->on_game_event(e);
    }

    return result;
}

float ActionResolver::resolve(uint32_t entity_id, const Intent& intent, WorldBubble& bubble, Entity& entity, LocomotionData& loco) {
    switch (intent.type) {
        case IntentType::MOVE:
            return resolve_move(intent, entity, bubble, loco);

        case IntentType::SMASH:
            return resolve_smash(intent, entity, bubble);

        case IntentType::OPEN:
            return resolve_open(intent, entity, bubble);

        case IntentType::CLOSE:
            return resolve_close(intent, entity, bubble);

        // Combat and item pickup use dedicated paths: CombatResolver through
        // SimulationDirector, and resolve_pickup() through GameWorld.
        case IntentType::ATTACK:
        case IntentType::PICKUP:
            return 0.0f;

        case IntentType::NONE:
            return ActionCost::WAIT;
        default:
            return 0.0f;
    }
}

bool ActionResolver::is_hostile_entity_at(const WorldBubble& bubble, int x, int y, uint32_t self_id) {
    const WorldBubble::CellEntity* occupant = bubble.get_entity_at(x, y);
    return occupant != nullptr && occupant->entity_id != self_id;
}
