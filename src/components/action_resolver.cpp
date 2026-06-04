#include "action_resolver.h"
#include "entities/entity.h"
#include "world/world_bubble.h"
#include "sim/game_event.h"
#include "components/inventory.h"
#include "core/id_registry.h"
#include "locomotion.h"
#include "health.h"
#include "stamina.h"
#include "equipment.h"
#include "anatomy.h"
#include "data/tile_db.h"
#include "data/body_part_db.h"
#include "data/style_db.h"
#include "data/ability_db.h"
#include <godot_cpp/variant/utility_functions.hpp>
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

AttackResult ActionResolver::resolve_attack(const AnatomyData& attacker_anatomy, AnatomyData& defender_anatomy, HealthData& defender_health, EquipmentData& attacker_equip, float base_damage, const StyleInfo* style, StaminaData* attacker_stamina) {
    AttackResult result;
    if (!defender_health.alive) return result;

    BodyPartDb* db = BodyPartDb::get_singleton();
    AbilityDb* ability_db = AbilityDb::get_singleton();
    if (!db || !ability_db) return result;

    float attack_power = Equipment::get_attack_power(attacker_equip);
    bool unarmed = attack_power <= 0.0f;
    bool style_active = style && (!style->requires_unarmed || unarmed);

    std::vector<const AbilityInfo*> pool;
    std::vector<float> weights;
    float total_weight = 0.0f;
    bool limb_capable = false;

    if (style) {
        for (const auto& entry : style->abilities) {
            const AbilityInfo* ability = ability_db->get_ability_info(entry.ability_id);
            if (!ability) continue;
            if (!Anatomy::has_functional_limbs(attacker_anatomy, ability->required_limbs)) continue;
            limb_capable = true;
            if (attacker_stamina && !Stamina::can_afford(*attacker_stamina, ability->stamina_cost)) continue;
            pool.push_back(ability);
            weights.push_back(entry.weight);
            total_weight += entry.weight;
        }
    }

    const AbilityInfo* chosen = nullptr;
    if (total_weight > 0.0f) {
        float roll = static_cast<float>(UtilityFunctions::randf()) * total_weight;
        for (size_t i = 0; i < pool.size(); i++) {
            roll -= weights[i];
            if (roll <= 0.0f) { chosen = pool[i]; break; }
        }
        if (!chosen) chosen = pool.back();
    }

    if (!chosen) {
        if (limb_capable && attacker_stamina) result.exhausted = true;
        return result;
    }

    if (attacker_stamina) Stamina::drain(*attacker_stamina, chosen->stamina_cost);

    result.verb = chosen->verb;
    result.speed = chosen->speed;

    float limb_integrity = Anatomy::min_required_integrity(attacker_anatomy, chosen->required_limbs);
    float effective_accuracy = chosen->accuracy * limb_integrity;
    if (style_active) effective_accuracy += style->accuracy_mod;
    if (UtilityFunctions::randf() > effective_accuracy) {
        result.hit = false;
        return result;
    }
    result.hit = true;

    float damage = base_damage * chosen->damage_mult + attack_power;
    if (style_active) damage *= style->damage_mult;

    float variance = CombatTuning::DAMAGE_VARIANCE_MIN +
        static_cast<float>(UtilityFunctions::randf()) * (CombatTuning::DAMAGE_VARIANCE_MAX - CombatTuning::DAMAGE_VARIANCE_MIN);
    damage *= variance;

    float crit_chance = CombatTuning::CRIT_CHANCE;
    if (style_active) crit_chance += style->crit_chance_mod;
    if (UtilityFunctions::randf() < crit_chance) {
        result.crit = true;
        damage *= CombatTuning::CRIT_MULT;
    }
    result.damage = damage;

    Health::damage(defender_health, damage);
    result.killed = !defender_health.alive;

    result.effect_type = chosen->effect_type;
    result.effect_mode = chosen->effect_mode;
    result.effect_magnitude = chosen->effect_magnitude;
    result.effect_duration = chosen->effect_duration;

    std::vector<String> target_heights;
    if (style_active) target_heights = style->target_heights;

    int loc_idx = Anatomy::pick_hit_location(defender_anatomy, target_heights);
    if (loc_idx >= 0) {
        result.hit_part_index = loc_idx;
        result.hit_part_type = Anatomy::get_type_id(defender_anatomy, loc_idx);
        result.part_name = db->get_body_part_name(result.hit_part_type);
        float part_size = db->get_body_part_size(result.hit_part_type);
        float durability = part_size * 10.0f;
        float integrity_loss = (durability > 0.0f) ? (damage / durability) : 1.0f;
        float current = Anatomy::get_integrity(defender_anatomy, loc_idx);
        Anatomy::set_integrity(defender_anatomy, loc_idx, MAX(0.0f, current - integrity_loss));
    }

    return result;
}

float ActionResolver::resolve_smash(const Intent& intent, Entity& entity, WorldBubble& bubble, const String& tile_db_path) {
    int dx = abs(intent.target.x - entity.x);
    int dy = abs(intent.target.y - entity.y);
    if (dx > 1 || dy > 1 || (dx == 0 && dy == 0)) return 0.0f;

    bubble.place_tile(intent.target.x, intent.target.y, "dirt", WorldBubble::LAYER_TILE);

    return ActionCost::SMASH;
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

        case IntentType::PICKUP:
            return 0.0f;
        
        case IntentType::ATTACK:
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
