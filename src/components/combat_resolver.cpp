#include "combat_resolver.h"

#include "anatomy.h"
#include "clothing.h"
#include "equipment.h"
#include "health.h"
#include "stamina.h"
#include "data/ability_db.h"
#include "data/body_part_db.h"
#include "data/item_db.h"
#include "data/style_db.h"
#include <vector>

using namespace godot;

namespace {

struct AttackCandidate {
    const AbilityInfo* ability = nullptr;
    const StyleInfo* style = nullptr;
    float weight = 1.0f;
    float weapon_damage = 0.0f;
};

struct AttackPool {
    std::vector<AttackCandidate> candidates;
    float total_weight = 0.0f;
    bool limb_capable = false;
    bool limb_blocked = false;
};

struct DamagePacket {
    const AbilityInfo* ability = nullptr;
    const StyleInfo* style = nullptr;
    float weapon_damage = 0.0f;
};

void add_style_candidates(
    AttackPool& pool,
    const AnatomyData& attacker_anatomy,
    const StyleInfo* style,
    StaminaData* attacker_stamina,
    float weapon_damage
) {
    AbilityDb* ability_db = AbilityDb::get_singleton();
    if (!style || !ability_db) return;

    for (const auto& entry : style->abilities) {
        const AbilityInfo* ability = ability_db->get_ability_info(entry.ability_id);
        if (!ability) continue;
        if (!Anatomy::has_functional_limbs(attacker_anatomy, ability->required_limbs)) {
            pool.limb_blocked = true;
            continue;
        }
        pool.limb_capable = true;
        if (attacker_stamina && !Stamina::can_afford(*attacker_stamina, ability->stamina_cost)) continue;
        pool.candidates.push_back({ability, style, entry.weight, weapon_damage});
        pool.total_weight += entry.weight;
    }
}

AttackPool build_attack_pool(const CombatContext& ctx) {
    AttackPool pool;

    bool armed = false;
    ItemDb* item_db = ItemDb::get_singleton();
    StyleDb* style_db = StyleDb::get_singleton();
    if (item_db && style_db) {
        std::vector<String> weapons = Equipment::get_wielded_weapon_ids(ctx.attacker_equipment);
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
            int functional_grasps = Anatomy::count_functional_parts_with_tag(ctx.attacker_anatomy, "GRASP");
            if (total_required_grasps <= functional_grasps) {
                for (const WieldedWeapon& weapon : valid_weapons) {
                    add_style_candidates(pool, ctx.attacker_anatomy, weapon.style, ctx.attacker_stamina, weapon.damage);
                }
            } else {
                pool.limb_blocked = true;
            }
        }
    }

    if (!armed) {
        add_style_candidates(pool, ctx.attacker_anatomy, ctx.style, ctx.attacker_stamina, Equipment::get_attack_power(ctx.attacker_equipment));
    }

    return pool;
}

AttackCandidate select_attack(const AttackPool& pool, Rng::Seeded& rng) {
    if (pool.total_weight <= 0.0f || pool.candidates.empty()) return AttackCandidate();

    float roll = rng.unit() * pool.total_weight;
    for (const AttackCandidate& candidate : pool.candidates) {
        roll -= candidate.weight;
        if (roll <= 0.0f) return candidate;
    }
    return pool.candidates.back();
}

DamagePacket make_damage_packet(const AttackCandidate& candidate) {
    DamagePacket packet;
    packet.ability = candidate.ability;
    packet.style = candidate.style;
    packet.weapon_damage = candidate.weapon_damage;
    return packet;
}

void apply_damage_packet(const CombatContext& ctx, const DamagePacket& packet, CombatOutcome& outcome) {
    BodyPartDb* body_db = BodyPartDb::get_singleton();
    if (!body_db || !packet.ability) return;

    if (ctx.attacker_stamina) Stamina::drain(*ctx.attacker_stamina, packet.ability->stamina_cost);

    outcome.verb = packet.ability->verb;
    outcome.speed = packet.ability->speed;

    float limb_integrity = Anatomy::min_required_integrity(ctx.attacker_anatomy, packet.ability->required_limbs);
    float effective_accuracy = packet.ability->accuracy * limb_integrity;
    if (packet.style) effective_accuracy += packet.style->accuracy_mod;
    if (ctx.rng.unit() > effective_accuracy) {
        outcome.hit = false;
        return;
    }
    outcome.hit = true;

    float damage = ctx.base_damage * packet.ability->damage_mult + packet.weapon_damage;
    if (packet.style) damage *= packet.style->damage_mult;

    float variance = CombatTuning::DAMAGE_VARIANCE_MIN +
        ctx.rng.unit() * (CombatTuning::DAMAGE_VARIANCE_MAX - CombatTuning::DAMAGE_VARIANCE_MIN);
    damage *= variance;

    float crit_chance = CombatTuning::CRIT_CHANCE;
    if (packet.style) crit_chance += packet.style->crit_chance_mod;
    if (ctx.rng.unit() < crit_chance) {
        outcome.crit = true;
        damage *= CombatTuning::CRIT_MULT;
    }

    std::vector<String> target_heights;
    if (packet.style) target_heights = packet.style->target_heights;

    int loc_idx = Anatomy::pick_hit_location(ctx.defender_anatomy, target_heights, ctx.rng);
    if (loc_idx >= 0) {
        outcome.hit_part_index = loc_idx;
        outcome.hit_part_type = Anatomy::get_type_id(ctx.defender_anatomy, loc_idx);
        outcome.part_name = body_db->get_body_part_name(outcome.hit_part_type);
        if (ctx.defender_clothing) {
            float armor = Clothing::get_armor_for_part(*ctx.defender_clothing, ctx.defender_anatomy, loc_idx);
            if (armor > 0.0f) {
                damage *= 10.0f / (10.0f + armor);
            }
        }
        float part_size = body_db->get_body_part_size(outcome.hit_part_type);
        float durability = part_size * 10.0f;
        float integrity_loss = (durability > 0.0f) ? (damage / durability) : 1.0f;
        float current = Anatomy::get_integrity(ctx.defender_anatomy, loc_idx);
        Anatomy::set_integrity(ctx.defender_anatomy, loc_idx, MAX(0.0f, current - integrity_loss));
    }

    outcome.damage = damage;

    Health::damage(ctx.defender_health, damage);
    outcome.killed = !ctx.defender_health.alive;

    outcome.effect_type = packet.ability->effect_type;
    outcome.effect_mode = packet.ability->effect_mode;
    outcome.effect_magnitude = packet.ability->effect_magnitude;
    outcome.effect_duration = packet.ability->effect_duration;
}

}

CombatOutcome CombatResolver::resolve_attack(const CombatContext& ctx) {
    CombatOutcome outcome;
    if (!ctx.defender_health.alive) return outcome;
    if (!BodyPartDb::get_singleton() || !AbilityDb::get_singleton()) return outcome;

    AttackPool pool = build_attack_pool(ctx);
    AttackCandidate chosen = select_attack(pool, ctx.rng);

    if (!chosen.ability) {
        if (pool.limb_capable && ctx.attacker_stamina) outcome.exhausted = true;
        else if (pool.limb_blocked) outcome.no_limbs = true;
        return outcome;
    }

    DamagePacket packet = make_damage_packet(chosen);
    apply_damage_packet(ctx, packet, outcome);
    return outcome;
}
