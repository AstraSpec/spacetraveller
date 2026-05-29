#include "action_resolver.h"
#include "entities/entity.h"
#include "world/world_bubble.h"
#include "locomotion.h"
#include "health.h"
#include "equipment.h"
#include "data/tile_db.h"
#include <cmath>

using namespace godot;

float ActionResolver::resolve_move(const Intent& intent, Entity& entity, WorldBubble& bubble, LocomotionData& loco) {
    int dx = abs(intent.target.x - entity.x);
    int dy = abs(intent.target.y - entity.y);

    if (dx > 1 || dy > 1 || (dx == 0 && dy == 0)) return 0.0f;

    TileDb* tile_db = TileDb::get_singleton();
    if (tile_db) {
        uint16_t tile_id = bubble.query_tile_id(intent.target.x, intent.target.y);
        if (tile_id != 0) {
            const TileInfo* info = tile_db->get_tile_info(tile_id);
            if (info && info->solid) return 0.0f;
        }
    }

    const WorldBubble::CellEntity* occupant = bubble.get_entity_at(intent.target.x, intent.target.y);
    if (occupant) return 0.0f;

    int old_x = entity.x, old_y = entity.y;
    bubble.update_entity_position(old_x, old_y, intent.target.x, intent.target.y, entity.id);
    entity.x = intent.target.x;
    entity.y = intent.target.y;

    Locomotion::advance_step(loco);

    return Locomotion::get_step_cost(old_x, old_y, intent.target.x, intent.target.y);
}

float ActionResolver::resolve_attack(uint32_t attacker_id, uint32_t defender_id, WorldBubble& bubble, HealthData& defender_health, EquipmentData& attacker_equip, float base_damage) {
    if (!defender_health.alive) return 0.0f;

    float attack_power = Equipment::get_attack_power(attacker_equip);
    float damage = base_damage + attack_power;

    Health::damage(defender_health, damage);

    return ActionCost::ATTACK;
}

float ActionResolver::resolve_smash(const Intent& intent, Entity& entity, WorldBubble& bubble, const String& tile_db_path) {
    int dx = abs(intent.target.x - entity.x);
    int dy = abs(intent.target.y - entity.y);
    if (dx > 1 || dy > 1 || (dx == 0 && dy == 0)) return 0.0f;

    bubble.place_tile(intent.target.x, intent.target.y, "dirt", WorldBubble::LAYER_TILE);

    return ActionCost::SMASH;
}

float ActionResolver::resolve_pickup(const Intent& intent, Entity& entity, WorldBubble& bubble, void* inventory) {
    int dx = abs(intent.target.x - entity.x);
    int dy = abs(intent.target.y - entity.y);
    if (dx > 1 || dy > 1) return 0.0f;

    return ActionCost::PICKUP;
}

float ActionResolver::resolve(uint32_t entity_id, const Intent& intent, WorldBubble& bubble, Entity& entity, LocomotionData& loco) {
    switch (intent.type) {
        case IntentType::MOVE:
            return resolve_move(intent, entity, bubble, loco);

        case IntentType::SMASH:
            return resolve_smash(intent, entity, bubble);

        case IntentType::PICKUP:
            return resolve_pickup(intent, entity, bubble);

        case IntentType::ATTACK:
        case IntentType::NONE:
        default:
            return 0.0f;
    }
}

bool ActionResolver::is_hostile_entity_at(const WorldBubble& bubble, int x, int y, uint32_t self_id) {
    const WorldBubble::CellEntity* occupant = bubble.get_entity_at(x, y);
    return occupant != nullptr && occupant->entity_id != self_id;
}
