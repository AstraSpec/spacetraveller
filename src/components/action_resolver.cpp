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
#include "data/item_db.h"
#include "core/tag_registry.h"
#include <godot_cpp/variant/utility_functions.hpp>
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

    struct AttackCandidate {
        const AbilityInfo* ability = nullptr;
        const StyleInfo* style = nullptr;
        float weight = 1.0f;
        float weapon_damage = 0.0f;
    };

    std::vector<AttackCandidate> pool;
    float total_weight = 0.0f;
    bool limb_capable = false;
    bool limb_blocked = false;

    auto add_style_candidates = [&](const StyleInfo* candidate_style, float weapon_damage) {
        if (!candidate_style) return;
        for (const auto& entry : candidate_style->abilities) {
            const AbilityInfo* ability = ability_db->get_ability_info(entry.ability_id);
            if (!ability) continue;
            if (!Anatomy::has_functional_limbs(attacker_anatomy, ability->required_limbs)) {
                limb_blocked = true;
                continue;
            }
            limb_capable = true;
            if (attacker_stamina && !Stamina::can_afford(*attacker_stamina, ability->stamina_cost)) continue;
            pool.push_back({ability, candidate_style, entry.weight, weapon_damage});
            total_weight += entry.weight;
        }
    };

    bool armed = false;
    ItemDb* item_db = ItemDb::get_singleton();
    StyleDb* style_db = StyleDb::get_singleton();
    if (item_db && style_db) {
        std::vector<String> weapons = Equipment::get_wielded_weapon_ids(attacker_equip);
        int total_required_grasps = 0;
        struct WieldedWeapon {
            const StyleInfo* style = nullptr;
            float damage = 0.0f;
            int grasp_required = 1;
        };
        std::vector<WieldedWeapon> valid_weapons;

        for (const String& item_id : weapons) {
            Dictionary weapon = item_db->get_weapon_data(item_id);
            if (weapon.is_empty()) continue;
            String style_id = weapon.get("style", "");
            const StyleInfo* weapon_style = style_db->get_style_info(style_id);
            if (!weapon_style) continue;

            WieldedWeapon wielded;
            wielded.style = weapon_style;
            wielded.damage = static_cast<float>(static_cast<double>(weapon.get("damage", 0.0)));
            wielded.grasp_required = MAX(1, static_cast<int>(weapon.get("grasp_required", 1)));
            total_required_grasps += wielded.grasp_required;
            valid_weapons.push_back(wielded);
        }

        if (!valid_weapons.empty()) {
            armed = true;
            int functional_grasps = Anatomy::count_functional_parts_with_tag(attacker_anatomy, "GRASP");
            if (total_required_grasps <= functional_grasps) {
                for (const WieldedWeapon& weapon : valid_weapons) {
                    add_style_candidates(weapon.style, weapon.damage);
                }
            } else {
                limb_blocked = true;
            }
        }
    }

    if (!armed) {
        add_style_candidates(style, Equipment::get_attack_power(attacker_equip));
    }

    AttackCandidate chosen;
    if (total_weight > 0.0f) {
        float roll = static_cast<float>(UtilityFunctions::randf()) * total_weight;
        for (size_t i = 0; i < pool.size(); i++) {
            roll -= pool[i].weight;
            if (roll <= 0.0f) { chosen = pool[i]; break; }
        }
        if (!chosen.ability) chosen = pool.back();
    }

    if (!chosen.ability) {
        if (limb_capable && attacker_stamina) result.exhausted = true;
        else if (limb_blocked) result.no_limbs = true;
        return result;
    }

    if (attacker_stamina) Stamina::drain(*attacker_stamina, chosen.ability->stamina_cost);

    result.verb = chosen.ability->verb;
    result.speed = chosen.ability->speed;

    float limb_integrity = Anatomy::min_required_integrity(attacker_anatomy, chosen.ability->required_limbs);
    float effective_accuracy = chosen.ability->accuracy * limb_integrity;
    if (chosen.style) effective_accuracy += chosen.style->accuracy_mod;
    if (UtilityFunctions::randf() > effective_accuracy) {
        result.hit = false;
        return result;
    }
    result.hit = true;

    float damage = base_damage * chosen.ability->damage_mult + chosen.weapon_damage;
    if (chosen.style) damage *= chosen.style->damage_mult;

    float variance = CombatTuning::DAMAGE_VARIANCE_MIN +
        static_cast<float>(UtilityFunctions::randf()) * (CombatTuning::DAMAGE_VARIANCE_MAX - CombatTuning::DAMAGE_VARIANCE_MIN);
    damage *= variance;

    float crit_chance = CombatTuning::CRIT_CHANCE;
    if (chosen.style) crit_chance += chosen.style->crit_chance_mod;
    if (UtilityFunctions::randf() < crit_chance) {
        result.crit = true;
        damage *= CombatTuning::CRIT_MULT;
    }
    result.damage = damage;

    Health::damage(defender_health, damage);
    result.killed = !defender_health.alive;

    result.effect_type = chosen.ability->effect_type;
    result.effect_mode = chosen.ability->effect_mode;
    result.effect_magnitude = chosen.ability->effect_magnitude;
    result.effect_duration = chosen.ability->effect_duration;

    std::vector<String> target_heights;
    if (chosen.style) target_heights = chosen.style->target_heights;

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
