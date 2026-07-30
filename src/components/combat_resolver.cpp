#include "combat_resolver.h"

#include "anatomy.h"
#include "clothing.h"
#include "combat_state.h"
#include "equipment.h"
#include "effects.h"
#include "health.h"
#include "physiology.h"
#include "stamina.h"
#include "data/ability_db.h"
#include "data/body_part_db.h"
#include "data/item_db.h"
#include "data/race_db.h"
#include "data/style_db.h"
#include "data/weapon_profile_db.h"
#include <vector>

using namespace godot;

namespace {

struct AttackCandidate {
    String id;
    String packet_id;
    String display_name;
    String verb;
    std::vector<String> required_limbs;
    std::vector<String> target_heights;
    float damage_mult = 1.0f;
    float accuracy = 0.75f;
    float speed = 1.0f;
    float stamina_cost = 10.0f;
    float weight = 1.0f;
    bool allow_exhausted = false;
    DamageType damage_type = DamageType::BASH;
    String effect_type;
    String effect_mode;
    float effect_magnitude = 0.0f;
    float effect_duration = 0.0f;

    bool armed = false;
    String weapon_slot;
    String weapon_item_id;
    float weapon_damage = 0.0f;
    float handling_ratio = 1.0f;
    int reach = 1;
    float natural_limb_integrity = 1.0f;
    float natural_limb_ratio = 1.0f;
    std::vector<int> natural_limb_indices;
    const StyleInfo* natural_style = nullptr;
};

struct AttackPool {
    std::vector<AttackCandidate> candidates;
    float total_weight = 0.0f;
    bool limb_capable = false;
    bool limb_blocked = false;
};

float candidate_accuracy(
    const CombatContext& ctx,
    const AttackCandidate& candidate
) {
    float accuracy = candidate.accuracy;
    if (!candidate.armed && candidate.natural_style) {
        accuracy += candidate.natural_style->accuracy_mod;
    }
    accuracy +=
        (ctx.attacker_combat_skill - CombatTuning::DEFAULT_COMBAT_SKILL) *
        CombatTuning::SKILL_ACCURACY_PER_LEVEL;
    if (ctx.attacker_stamina) {
        accuracy += Stamina::combat_accuracy_modifier(*ctx.attacker_stamina);
    }
    if (candidate.armed) {
        accuracy += Equipment::handling_accuracy_modifier(
            candidate.handling_ratio);
    } else {
        accuracy += CombatMath::natural_limb_accuracy_modifier(
            candidate.natural_limb_ratio);
    }
    if (ctx.attacker_physiology) {
        accuracy += CombatMath::pain_accuracy_modifier(
            ctx.attacker_effective_pain);
        accuracy += CombatMath::consciousness_accuracy_modifier(
            Physiology::get_consciousness_percent(
                *ctx.attacker_physiology));
    }
    return CLAMP(accuracy, 0.05f, 0.95f);
}

float candidate_base_damage(
    const CombatContext& ctx,
    const AttackCandidate& candidate
) {
    float damage = (candidate.armed
        ? candidate.weapon_damage
        : ctx.base_damage) * candidate.damage_mult;
    if (!candidate.armed && candidate.natural_style) {
        damage *= candidate.natural_style->damage_mult;
    }
    if (candidate.armed) {
        damage *= Equipment::handling_damage_multiplier(
            candidate.handling_ratio);
    } else {
        damage *= CombatMath::natural_limb_damage_multiplier(
            candidate.natural_limb_ratio);
    }
    return MAX(0.0f, damage);
}

float candidate_speed(
    const CombatContext& ctx,
    const AttackCandidate& candidate
) {
    float speed = candidate.speed;
    if (ctx.attacker_stamina) {
        speed *= Stamina::combat_speed_multiplier(*ctx.attacker_stamina);
    }
    if (candidate.armed) {
        speed *= Equipment::handling_speed_multiplier(
            candidate.handling_ratio);
    } else {
        speed *= CombatMath::natural_limb_speed_multiplier(
            candidate.natural_limb_ratio);
    }
    if (ctx.attacker_physiology) {
        speed *= CombatMath::pain_speed_multiplier(
            ctx.attacker_effective_pain);
        speed *= CombatMath::consciousness_speed_multiplier(
            Physiology::get_consciousness_percent(
                *ctx.attacker_physiology));
    }
    return MAX(0.01f, speed);
}

void add_natural_candidates(
    AttackPool& pool,
    const CombatContext& ctx
) {
    AbilityDb* ability_db = AbilityDb::get_singleton();
    if (!ctx.style || !ability_db) return;

    for (const StyleAbilityEntry& entry : ctx.style->abilities) {
        const AbilityInfo* ability =
            ability_db->get_ability_info(entry.ability_id);
        if (!ability) continue;
        const NaturalLimbAllocation limb_allocation =
            Anatomy::allocate_natural_attack_limbs(
                ctx.attacker_anatomy,
                ability->required_limbs);
        if (!limb_allocation.valid) {
            pool.limb_blocked = true;
            continue;
        }
        pool.limb_capable = true;
        if (ctx.attacker_stamina &&
            !Stamina::can_afford(
                *ctx.attacker_stamina,
                ability->stamina_cost) &&
            !ability->allow_exhausted) {
            continue;
        }

        AttackCandidate candidate;
        candidate.id = ability->id;
        candidate.packet_id = ability->id;
        candidate.display_name = ability->name;
        candidate.verb = ability->verb;
        candidate.required_limbs = ability->required_limbs;
        candidate.target_heights = ctx.style->target_heights;
        candidate.damage_mult = ability->damage_mult;
        candidate.accuracy = ability->accuracy;
        candidate.speed = ability->speed;
        candidate.stamina_cost = ability->stamina_cost;
        candidate.weight = entry.weight;
        candidate.allow_exhausted = ability->allow_exhausted;
        candidate.damage_type = ability->damage_type;
        candidate.effect_type = ability->effect_type;
        candidate.effect_mode = ability->effect_mode;
        candidate.effect_magnitude = ability->effect_magnitude;
        candidate.effect_duration = ability->effect_duration;
        candidate.natural_limb_integrity =
            limb_allocation.raw_integrity;
        candidate.natural_limb_ratio = limb_allocation.ratio;
        candidate.natural_limb_indices =
            limb_allocation.part_indices;
        candidate.natural_style = ctx.style;
        pool.total_weight += candidate.weight;
        pool.candidates.push_back(candidate);
    }
}

bool add_weapon_candidates(
    AttackPool& pool,
    const CombatContext& ctx
) {
    ItemDb* item_db = ItemDb::get_singleton();
    WeaponProfileDb* profile_db = WeaponProfileDb::get_singleton();
    if (!item_db || !profile_db) return false;

    const float manipulation =
        Anatomy::get_manipulation_units(ctx.attacker_anatomy);
    const std::vector<WeaponHandling> handling =
        Equipment::get_weapon_handling(
            ctx.attacker_equipment,
            manipulation);
    if (handling.empty()) return false;

    bool found_weapon = false;
    for (const WeaponHandling& held : handling) {
        if (held.ratio < 0.25f) {
            found_weapon = true;
            pool.limb_blocked = true;
            continue;
        }
        const Dictionary weapon =
            item_db->get_weapon_data(held.item_id);
        if (weapon.is_empty()) continue;
        found_weapon = true;
        const String profile_id = weapon.get("attack_profile", "");
        const WeaponProfileInfo* profile =
            profile_db->get_profile_info(profile_id);
        if (!profile) {
            pool.limb_blocked = true;
            continue;
        }
        pool.limb_capable = true;

        for (const WeaponAttackPacket& packet : profile->packets) {
            if (ctx.attacker_stamina &&
                !Stamina::can_afford(
                    *ctx.attacker_stamina,
                    packet.stamina_cost) &&
                !packet.allow_exhausted) {
                continue;
            }

            AttackCandidate candidate;
            candidate.id =
                held.slot_name + String(":") + held.item_id +
                String(":") + packet.id;
            candidate.packet_id = packet.id;
            candidate.display_name = packet.name;
            candidate.verb = packet.verb;
            candidate.target_heights = packet.target_heights;
            candidate.damage_mult = packet.damage_mult;
            candidate.accuracy = packet.accuracy;
            candidate.speed = packet.speed;
            candidate.stamina_cost = packet.stamina_cost;
            candidate.weight = packet.weight;
            candidate.allow_exhausted = packet.allow_exhausted;
            candidate.damage_type = packet.damage_type;
            candidate.effect_type = packet.effect_type;
            candidate.effect_mode = packet.effect_mode;
            candidate.effect_magnitude = packet.effect_magnitude;
            candidate.effect_duration = packet.effect_duration;
            candidate.armed = true;
            candidate.weapon_slot = held.slot_name;
            candidate.weapon_item_id = held.item_id;
            candidate.weapon_damage = static_cast<float>(
                static_cast<double>(weapon.get("damage", 0.0)));
            candidate.handling_ratio = held.ratio;
            candidate.reach = MAX(
                1,
                static_cast<int>(weapon.get("reach", 1)));
            pool.total_weight += candidate.weight;
            pool.candidates.push_back(candidate);
        }
    }
    return found_weapon;
}

AttackPool build_attack_pool(const CombatContext& ctx) {
    AttackPool pool;
    if (!add_weapon_candidates(pool, ctx)) {
        add_natural_candidates(pool, ctx);
    }
    return pool;
}

AttackCandidate select_attack(
    const AttackPool& pool,
    Rng::Seeded& rng
) {
    if (pool.total_weight <= 0.0f || pool.candidates.empty()) {
        return AttackCandidate();
    }
    float roll = rng.unit() * pool.total_weight;
    for (const AttackCandidate& candidate : pool.candidates) {
        roll -= candidate.weight;
        if (roll <= 0.0f) return candidate;
    }
    return pool.candidates.back();
}

String quality_name(CombatMath::HitQuality quality) {
    switch (quality) {
        case CombatMath::HitQuality::GLANCING: return "glancing";
        case CombatMath::HitQuality::SOLID: return "solid";
        case CombatMath::HitQuality::STRONG: return "strong";
        case CombatMath::HitQuality::CRITICAL: return "critical";
    }
    return "solid";
}

void reconcile_defender_weapons(
    const CombatContext& ctx,
    CombatOutcome& outcome
) {
    if (!ctx.defender_equipment) return;
    const std::vector<WeaponHandling> dropped =
        Equipment::reconcile_handling(
            *ctx.defender_equipment,
            Anatomy::get_manipulation_units(ctx.defender_anatomy));
    for (const WeaponHandling& item : dropped) {
        outcome.dropped_weapon_ids.push_back(item.item_id);
    }
}

void apply_damage(
    const CombatContext& ctx,
    const AttackCandidate& candidate,
    CombatOutcome& outcome,
    int location,
    float damage
) {
    BodyPartDb* body_db = BodyPartDb::get_singleton();
    if (!body_db || location < 0) return;

    outcome.hit_part_index = location;
    outcome.hit_part_type =
        Anatomy::get_type_id(ctx.defender_anatomy, location);
    outcome.part_name =
        body_db->get_body_part_name(outcome.hit_part_type);

    const float pre_armor_damage = damage;
    if (ctx.defender_clothing) {
        for (const ProtectionLayer& layer :
            Clothing::get_protection_layers_for_part(
                *ctx.defender_clothing,
                ctx.defender_anatomy,
                location)) {
            if (CombatMath::armor_coverage_applies(
                    ctx.rng.unit(),
                    layer.coverage)) {
                damage = Clothing::apply_covered_layer(
                    damage,
                    candidate.damage_type,
                    layer);
            }
            if (damage <= 0.0f) break;
        }
    }

    RaceDb* race_db = RaceDb::get_singleton();
    const RaceInfo* race = race_db
        ? race_db->get_race_info(ctx.defender_anatomy.race_id)
        : nullptr;
    if (race && race->natural_armor.enabled && damage > 0.0f) {
        ProtectionLayer natural;
        natural.item_id = "natural_armor";
        natural.layer = "natural";
        natural.coverage = race->natural_armor.coverage;
        natural.bash = race->natural_armor.bash;
        natural.cut = race->natural_armor.cut;
        natural.pierce = race->natural_armor.pierce;
        natural.bash_transmission =
            race->natural_armor.bash_transmission;
        if (CombatMath::armor_coverage_applies(
                ctx.rng.unit(),
                natural.coverage)) {
            damage = Clothing::apply_covered_layer(
                damage,
                candidate.damage_type,
                natural);
        }
    }

    const PartDamageResult applied = Anatomy::apply_damage(
        ctx.defender_anatomy,
        location,
        damage);
    outcome.damage = applied.applied_damage;
    if (applied.lethal) Health::kill(ctx.defender_health);
    if (!applied.lethal && damage > 0.0f && ctx.defender_physiology) {
        const float part_max =
            Anatomy::get_part_max_integrity(
                ctx.defender_anatomy,
                location);
        const bool destroyed_nonlethal =
            applied.newly_destroyed && !applied.lethal;
        const float consciousness_loss =
            CombatMath::consciousness_loss_from_hit(
                damage,
                part_max,
                body_db->get_body_part_consciousness_multiplier(
                    outcome.hit_part_type),
                candidate.damage_type,
                destroyed_nonlethal);
        const float pain_gain = CombatMath::pain_gain_from_hit(
            damage,
            part_max,
            body_db->get_body_part_pain_multiplier(
                outcome.hit_part_type),
            candidate.damage_type,
            destroyed_nonlethal);
        const float pain_floor = Anatomy::get_wound_pain_floor(
            ctx.defender_anatomy,
            ctx.defender_effects
                ? Effects::total_bleed(*ctx.defender_effects)
                : 0.0f);
        const PhysiologyUpdate physiology = Physiology::apply_trauma(
            *ctx.defender_physiology,
            consciousness_loss,
            pain_gain,
            Health::get_blood_percent(ctx.defender_health),
            pain_floor);
        outcome.consciousness_lost = physiology.consciousness_lost;
        outcome.pain_added = physiology.pain_added;
        outcome.newly_downed = physiology.newly_downed;
        if (physiology.reached_zero) {
            Health::kill(ctx.defender_health);
            outcome.consciousness_death = true;
        }
    }
    reconcile_defender_weapons(ctx, outcome);

    const float penetration_fraction = pre_armor_damage > 0.0f
        ? CLAMP(damage / pre_armor_damage, 0.0f, 1.0f)
        : 0.0f;
    outcome.effect_type = candidate.effect_type;
    outcome.effect_mode = candidate.effect_mode;
    outcome.effect_magnitude =
        candidate.effect_magnitude * penetration_fraction;
    outcome.effect_duration = candidate.effect_duration;
}

void resolve_candidate(
    const CombatContext& ctx,
    const AttackCandidate& candidate,
    CombatOutcome& outcome
) {
    const float base_accuracy = candidate_accuracy(ctx, candidate);
    const float stamina_speed = candidate_speed(ctx, candidate);
    outcome.verb = candidate.verb;
    outcome.speed = stamina_speed;
    outcome.base_accuracy = base_accuracy;

    if (ctx.attacker_stamina) {
        Stamina::drain(
            *ctx.attacker_stamina,
            candidate.stamina_cost);
    }

    const bool targeted = ctx.forced_body_part_index >= 0;
    float effective_accuracy = base_accuracy;
    if (targeted) {
        outcome.intended_part_index = ctx.forced_body_part_index;
        outcome.aim_penalty = Anatomy::get_targeting_penalty(
            ctx.defender_anatomy,
            ctx.forced_body_part_index);
        outcome.aim_time_multiplier = 1.0f + outcome.aim_penalty;
        effective_accuracy = MAX(
            0.0f,
            base_accuracy - outcome.aim_penalty);
    }
    outcome.effective_accuracy = effective_accuracy;

    const float attack_roll = ctx.rng.unit();
    int location = -1;
    float attack_margin = 0.0f;
    float margin_accuracy = base_accuracy;
    if (!targeted) {
        if (attack_roll >= base_accuracy) return;
        location = Anatomy::pick_hit_location(
            ctx.defender_anatomy,
            candidate.target_heights,
            ctx.rng);
        attack_margin = base_accuracy - attack_roll;
    } else {
        const CombatMath::TargetedContact contact =
            CombatMath::targeted_contact(
                attack_roll,
                base_accuracy,
                effective_accuracy);
        if (contact == CombatMath::TargetedContact::EXACT) {
            location = ctx.forced_body_part_index;
            attack_margin = effective_accuracy - attack_roll;
            margin_accuracy = MAX(effective_accuracy, 0.0001f);
        } else if (contact == CombatMath::TargetedContact::DEVIATED) {
            location = Anatomy::pick_deviation_location(
                ctx.defender_anatomy,
                ctx.forced_body_part_index,
                ctx.rng);
            if (location < 0) return;
            outcome.deviated = true;
            attack_margin = base_accuracy - attack_roll;
        } else {
            return;
        }
    }
    if (location < 0) return;
    outcome.attack_margin = attack_margin;

    if (ctx.defender_combat_state) {
        const float defender_movement =
            ctx.defender_physiology &&
            ctx.defender_physiology->downed
                ? 0.0f
                : ctx.defender_movement_speed;
        const float dodge = CombatMath::effective_dodge(
            defender_movement,
            ctx.defender_combat_state->successive_defenses);
        outcome.dodge_chance = dodge;
        CombatState::record_defense(*ctx.defender_combat_state);
        if (CombatMath::dodge_succeeds(dodge, ctx.rng.unit())) {
            outcome.dodged = true;
            return;
        }
    }

    outcome.hit = true;
    outcome.attack_margin = attack_margin;
    outcome.quality = CombatMath::hit_quality(
        attack_margin,
        margin_accuracy);
    if (outcome.deviated) {
        outcome.quality = CombatMath::downgrade_quality(outcome.quality);
    }
    outcome.crit =
        outcome.quality == CombatMath::HitQuality::CRITICAL;
    outcome.quality_name = quality_name(outcome.quality);

    float damage = candidate_base_damage(ctx, candidate);
    damage *= CombatMath::hit_quality_multiplier(outcome.quality);
    const float variance =
        CombatTuning::DAMAGE_VARIANCE_MIN +
        ctx.rng.unit() *
            (CombatTuning::DAMAGE_VARIANCE_MAX -
             CombatTuning::DAMAGE_VARIANCE_MIN);
    damage *= variance;

    apply_damage(ctx, candidate, outcome, location, damage);
    outcome.killed = !ctx.defender_health.alive;
}

}

CombatOutcome CombatResolver::resolve_attack(const CombatContext& ctx) {
    CombatOutcome outcome;
    if (!ctx.defender_health.alive) return outcome;
    if (!BodyPartDb::get_singleton()) return outcome;
    if (ctx.attacker_physiology && ctx.attacker_physiology->downed) {
        outcome.incapacitated = true;
        return outcome;
    }

    const AttackPool pool = build_attack_pool(ctx);
    AttackCandidate chosen;
    if (!ctx.forced_attack_id.is_empty()) {
        for (const AttackCandidate& candidate : pool.candidates) {
            if (candidate.id == ctx.forced_attack_id) {
                chosen = candidate;
                break;
            }
        }
        if (chosen.id.is_empty()) {
            outcome.invalid_selection = true;
            return outcome;
        }
    } else {
        chosen = select_attack(pool, ctx.rng);
    }

    if (chosen.id.is_empty()) {
        if (pool.limb_capable && ctx.attacker_stamina) {
            outcome.exhausted = true;
        } else if (pool.limb_blocked) {
            outcome.no_limbs = true;
        }
        return outcome;
    }

    if (ctx.forced_body_part_index >= 0 &&
        !Anatomy::is_functional(
            ctx.defender_anatomy,
            ctx.forced_body_part_index)) {
        outcome.invalid_selection = true;
        return outcome;
    }

    resolve_candidate(ctx, chosen, outcome);
    return outcome;
}

Array CombatResolver::get_attack_options(const CombatContext& ctx) {
    Array result;
    if (ctx.attacker_physiology && ctx.attacker_physiology->downed) {
        return result;
    }
    CombatContext option_context = ctx;
    option_context.attacker_stamina = nullptr;
    const AttackPool pool = build_attack_pool(option_context);
    ItemDb* item_db = ItemDb::get_singleton();

    for (const AttackCandidate& candidate : pool.candidates) {
        Dictionary option;
        option["id"] = candidate.id;
        option["packet_id"] = candidate.packet_id;
        option["display_name"] = candidate.display_name;
        option["verb"] = candidate.verb;
        option["armed"] = candidate.armed;
        option["weapon_id"] = candidate.weapon_item_id;
        option["weapon_slot"] = candidate.weapon_slot;
        option["weapon_name"] =
            candidate.armed && item_db
                ? item_db->get_item_name(candidate.weapon_item_id)
                : String();
        option["handling_ratio"] = candidate.handling_ratio;
        option["handling_accuracy_modifier"] = candidate.armed
            ? Equipment::handling_accuracy_modifier(
                candidate.handling_ratio)
            : 0.0f;
        option["handling_speed_multiplier"] = candidate.armed
            ? Equipment::handling_speed_multiplier(
                candidate.handling_ratio)
            : 1.0f;
        option["handling_damage_multiplier"] = candidate.armed
            ? Equipment::handling_damage_multiplier(
                candidate.handling_ratio)
            : 1.0f;
        Array natural_limb_indices;
        for (const int index : candidate.natural_limb_indices) {
            natural_limb_indices.push_back(index);
        }
        option["natural_limb_indices"] = natural_limb_indices;
        option["natural_limb_integrity"] =
            candidate.natural_limb_integrity;
        option["natural_limb_ratio"] = candidate.natural_limb_ratio;
        option["natural_limb_accuracy_modifier"] = candidate.armed
            ? 0.0f
            : CombatMath::natural_limb_accuracy_modifier(
                candidate.natural_limb_ratio);
        option["natural_limb_speed_multiplier"] = candidate.armed
            ? 1.0f
            : CombatMath::natural_limb_speed_multiplier(
                candidate.natural_limb_ratio);
        option["natural_limb_damage_multiplier"] = candidate.armed
            ? 1.0f
            : CombatMath::natural_limb_damage_multiplier(
                candidate.natural_limb_ratio);
        option["accuracy"] = candidate_accuracy(ctx, candidate);
        option["speed"] = candidate_speed(ctx, candidate);
        option["damage"] = candidate_base_damage(ctx, candidate);
        option["stamina_cost"] = candidate.stamina_cost;
        option["reach"] = candidate.reach;
        option["allow_exhausted"] = candidate.allow_exhausted;
        const bool disabled = ctx.attacker_stamina &&
            !Stamina::can_afford(
                *ctx.attacker_stamina,
                candidate.stamina_cost) &&
            !candidate.allow_exhausted;
        option["disabled"] = disabled;
        result.push_back(option);
    }
    return result;
}
