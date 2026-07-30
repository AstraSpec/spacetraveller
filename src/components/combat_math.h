#ifndef SPACETRAVELLER_COMBAT_MATH_H
#define SPACETRAVELLER_COMBAT_MATH_H

#include "damage.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace godot::CombatMath {

inline constexpr float DAMAGE_VARIANCE_MIN = 0.90f;
inline constexpr float DAMAGE_VARIANCE_MAX = 1.10f;
inline constexpr float DEFAULT_COMBAT_SKILL = 5.0f;
inline constexpr float SKILL_ACCURACY_PER_LEVEL = 0.02f;
inline constexpr float STAMINA_REGEN_FRACTION_PER_TIME = 0.025f;
inline constexpr float DODGE_PER_MOVEMENT_SPEED = 0.15f;
inline constexpr float DODGE_SUCCESSIVE_PENALTY = 0.05f;
inline constexpr float DODGE_MAX = 0.30f;
inline constexpr float CONSCIOUSNESS_DOWN_THRESHOLD = 0.20f;
inline constexpr float CONSCIOUSNESS_RECOVERY_THRESHOLD = 0.35f;
inline constexpr float CONSCIOUSNESS_RECOVERY_PER_TIME = 2.0f;
inline constexpr float PAIN_DECAY_PER_TIME = 1.0f / 3600.0f;
inline constexpr float PREFERRED_HEIGHT_CHANCE = 0.70f;

enum class HitQuality {
    GLANCING,
    SOLID,
    STRONG,
    CRITICAL
};

enum class TargetedContact {
    EXACT,
    DEVIATED,
    MISS
};

inline float effective_dodge(float movement_speed, int successive_defenses) {
    return std::clamp(
        DODGE_PER_MOVEMENT_SPEED * std::max(0.0f, movement_speed) -
            DODGE_SUCCESSIVE_PENALTY * std::max(0, successive_defenses),
        0.0f,
        DODGE_MAX);
}

inline bool dodge_succeeds(float dodge_chance, float roll) {
    return std::clamp(roll, 0.0f, 1.0f) <
        std::clamp(dodge_chance, 0.0f, 1.0f);
}

inline bool use_preferred_height_pool(
    bool has_preferred_parts,
    float roll
) {
    return has_preferred_parts &&
        std::clamp(roll, 0.0f, 1.0f) < PREFERRED_HEIGHT_CHANCE;
}

inline float movement_endpoint_capacity(
    float proximal_integrity,
    float endpoint_integrity,
    float digit_integrity
) {
    const float distal_support =
        0.25f +
        0.70f * std::clamp(endpoint_integrity, 0.0f, 1.0f) +
        0.05f * std::clamp(digit_integrity, 0.0f, 1.0f);
    const float raw = std::min(
        std::clamp(proximal_integrity, 0.0f, 1.0f),
        distal_support);
    if (raw < 0.70f) {
        return raw * (0.90f / 0.70f);
    }
    return 0.90f + (raw - 0.70f) * (0.10f / 0.30f);
}

inline float wound_pain_source(
    float missing_integrity,
    float part_pain_multiplier,
    float significance
) {
    const float severity = std::clamp(
        (std::clamp(missing_integrity, 0.0f, 1.0f) - 0.25f) /
            0.75f,
        0.0f,
        1.0f);
    return 0.40f *
        severity *
        std::max(0.0f, part_pain_multiplier) *
        std::max(0.0f, significance);
}

inline float bleeding_pain_floor(float total_bleed) {
    return std::min(0.25f, 0.25f * std::max(0.0f, total_bleed));
}

inline float pain_after_elapsed(
    float pain,
    float elapsed,
    float pain_floor
) {
    return std::max(
        std::clamp(pain_floor, 0.0f, 0.75f),
        std::clamp(pain, 0.0f, 1.0f) -
            PAIN_DECAY_PER_TIME * std::max(0.0f, elapsed));
}

inline float combined_pain_floor(
    const std::vector<float>& wound_sources,
    float total_bleed
) {
    float largest = 0.0f;
    float total = 0.0f;
    for (float source : wound_sources) {
        const float value = std::max(0.0f, source);
        largest = std::max(largest, value);
        total += value;
    }
    const float anatomy_floor = std::min(
        0.65f,
        largest + 0.35f * std::max(0.0f, total - largest));
    return std::clamp(
        anatomy_floor + bleeding_pain_floor(total_bleed),
        0.0f,
        0.75f);
}

inline float bleed_decay_rate(float magnitude) {
    const float value = std::max(0.0f, magnitude);
    if (value <= 0.25f) return 0.0014f;
    if (value <= 0.50f) return 0.0005f;
    if (value <= 0.75f) return 0.0002f;
    return 0.0001f;
}

struct BleedAdvance {
    float magnitude = 0.0f;
    float magnitude_time = 0.0f;
};

inline BleedAdvance advance_bleed(
    float starting_magnitude,
    float elapsed
) {
    BleedAdvance result;
    float magnitude = std::clamp(starting_magnitude, 0.0f, 1.0f);
    float remaining = std::max(0.0f, elapsed);
    while (magnitude > 0.0f && remaining > 0.0f) {
        const float rate = bleed_decay_rate(magnitude);
        float lower_boundary = 0.0f;
        if (magnitude > 0.75f) lower_boundary = 0.75f;
        else if (magnitude > 0.50f) lower_boundary = 0.50f;
        else if (magnitude > 0.25f) lower_boundary = 0.25f;
        const float boundary_time =
            (magnitude - lower_boundary) / rate;
        const float step = std::min(remaining, boundary_time);
        const float next = std::max(
            lower_boundary,
            magnitude - rate * step);
        result.magnitude_time +=
            0.5f * (magnitude + next) * step;
        magnitude = next;
        remaining -= step;
        if (boundary_time <= 0.00001f) {
            magnitude = std::max(0.0f, magnitude - rate * remaining);
            remaining = 0.0f;
        }
    }
    result.magnitude = magnitude;
    return result;
}

inline float interpolate_band(
    float value,
    float low,
    float high,
    float low_value,
    float high_value
) {
    if (high <= low) return high_value;
    const float t =
        std::clamp((value - low) / (high - low), 0.0f, 1.0f);
    return low_value + (high_value - low_value) * t;
}

inline float pain_accuracy_modifier(float pain) {
    const float p = std::clamp(pain, 0.0f, 1.0f);
    if (p <= 0.20f) return 0.0f;
    if (p <= 0.50f) {
        return interpolate_band(p, 0.20f, 0.50f, 0.0f, -0.03f);
    }
    if (p <= 0.80f) {
        return interpolate_band(p, 0.50f, 0.80f, -0.03f, -0.07f);
    }
    return interpolate_band(p, 0.80f, 1.0f, -0.07f, -0.10f);
}

inline float pain_speed_multiplier(float pain) {
    const float p = std::clamp(pain, 0.0f, 1.0f);
    if (p <= 0.20f) return 1.0f;
    if (p <= 0.50f) {
        return interpolate_band(p, 0.20f, 0.50f, 1.0f, 0.90f);
    }
    if (p <= 0.80f) {
        return interpolate_band(p, 0.50f, 0.80f, 0.90f, 0.75f);
    }
    return interpolate_band(p, 0.80f, 1.0f, 0.75f, 0.60f);
}

inline float pain_movement_multiplier(float pain) {
    return pain_speed_multiplier(pain);
}

inline bool should_be_downed(
    float consciousness_percent,
    bool currently_downed
) {
    const float consciousness =
        std::clamp(consciousness_percent, 0.0f, 1.0f);
    return currently_downed
        ? consciousness < CONSCIOUSNESS_RECOVERY_THRESHOLD
        : consciousness <= CONSCIOUSNESS_DOWN_THRESHOLD;
}

inline float consciousness_accuracy_modifier(
    float consciousness_percent
) {
    const float c = std::clamp(consciousness_percent, 0.0f, 1.0f);
    if (c >= 0.50f) return 0.0f;
    return interpolate_band(c, 0.20f, 0.50f, -0.05f, 0.0f);
}

inline float consciousness_speed_multiplier(
    float consciousness_percent
) {
    const float c = std::clamp(consciousness_percent, 0.0f, 1.0f);
    if (c >= 0.50f) return 1.0f;
    return interpolate_band(c, 0.20f, 0.50f, 0.75f, 1.0f);
}

inline float consciousness_movement_multiplier(
    float consciousness_percent
) {
    const float c = std::clamp(consciousness_percent, 0.0f, 1.0f);
    if (c >= 0.50f) return 1.0f;
    return interpolate_band(c, 0.20f, 0.50f, 0.60f, 1.0f);
}

inline float blood_consciousness_ceiling(float blood_percent) {
    return std::clamp(2.0f * blood_percent, 0.0f, 1.0f);
}

inline float pain_consciousness_ceiling(float pain) {
    return std::max(
        0.15f,
        1.0f - 1.7f * std::max(0.0f, pain - 0.50f));
}

inline float consciousness_ceiling_fraction(
    float blood_percent,
    float pain
) {
    return std::min(
        blood_consciousness_ceiling(blood_percent),
        pain_consciousness_ceiling(pain));
}

inline float consciousness_recovery(
    float blood_percent,
    float pain,
    float elapsed,
    float action_recovery
) {
    if (blood_percent <= 0.10f) return 0.0f;
    const float blood_factor =
        std::clamp(2.0f * blood_percent, 0.0f, 1.0f);
    const float pain_factor =
        std::max(0.25f, 1.0f - 0.75f * std::clamp(pain, 0.0f, 1.0f));
    return CONSCIOUSNESS_RECOVERY_PER_TIME *
        std::max(0.0f, elapsed) *
        std::clamp(action_recovery, 0.0f, 1.0f) *
        blood_factor *
        pain_factor;
}

inline float trauma_fraction(
    float post_armor_damage,
    float part_max_integrity
) {
    if (part_max_integrity <= 0.0f) return 0.0f;
    return std::clamp(
        post_armor_damage / part_max_integrity,
        0.0f,
        1.0f);
}

inline float consciousness_damage_type_multiplier(DamageType type) {
    switch (type) {
        case DamageType::BASH: return 1.25f;
        case DamageType::PIERCE: return 0.85f;
        case DamageType::CUT: return 0.70f;
    }
    return 1.0f;
}

inline float pain_damage_type_multiplier(DamageType type) {
    switch (type) {
        case DamageType::BASH: return 0.85f;
        case DamageType::PIERCE: return 1.15f;
        case DamageType::CUT: return 1.00f;
    }
    return 1.0f;
}

inline float consciousness_loss_from_hit(
    float post_armor_damage,
    float part_max_integrity,
    float part_multiplier,
    DamageType type,
    bool newly_destroyed_nonlethal
) {
    return 40.0f *
        trauma_fraction(post_armor_damage, part_max_integrity) *
        std::max(0.0f, part_multiplier) *
        consciousness_damage_type_multiplier(type) +
        (newly_destroyed_nonlethal ? 5.0f : 0.0f);
}

inline float pain_gain_from_hit(
    float post_armor_damage,
    float part_max_integrity,
    float part_multiplier,
    DamageType type,
    bool newly_destroyed_nonlethal
) {
    return 0.45f *
        trauma_fraction(post_armor_damage, part_max_integrity) *
        std::max(0.0f, part_multiplier) *
        pain_damage_type_multiplier(type) +
        (newly_destroyed_nonlethal ? 0.10f : 0.0f);
}

inline TargetedContact targeted_contact(
    float attack_roll,
    float base_accuracy,
    float targeted_accuracy
) {
    if (attack_roll < std::max(0.0f, targeted_accuracy)) {
        return TargetedContact::EXACT;
    }
    if (attack_roll < std::max(0.0f, base_accuracy)) {
        return TargetedContact::DEVIATED;
    }
    return TargetedContact::MISS;
}

inline float handling_accuracy_modifier(float ratio) {
    return -0.20f * (1.0f - std::clamp(ratio, 0.0f, 1.0f));
}

inline float handling_speed_multiplier(float ratio) {
    return 0.5f + 0.5f * std::clamp(ratio, 0.0f, 1.0f);
}

inline float handling_damage_multiplier(float ratio) {
    return 0.5f + 0.5f * std::clamp(ratio, 0.0f, 1.0f);
}

inline float natural_limb_handling_ratio(float raw_integrity) {
    return std::sqrt(std::clamp(raw_integrity, 0.0f, 1.0f));
}

inline float natural_limb_accuracy_modifier(float ratio) {
    return handling_accuracy_modifier(ratio);
}

inline float natural_limb_speed_multiplier(float ratio) {
    return handling_speed_multiplier(ratio);
}

inline float natural_limb_damage_multiplier(float ratio) {
    return handling_damage_multiplier(ratio);
}

inline std::vector<float> allocate_handling_ratios(
    const std::vector<float>& loads,
    float manipulation_units
) {
    std::vector<float> result;
    float remaining = std::max(0.0f, manipulation_units);
    for (float raw_load : loads) {
        const float load = std::max(0.01f, raw_load);
        const float allocated = std::min(remaining, load);
        result.push_back(allocated / load);
        remaining -= allocated;
    }
    return result;
}

inline float manipulation_endpoint_units(
    float chain_integrity,
    float digit_integrity
) {
    return std::clamp(chain_integrity, 0.0f, 1.0f) *
        (0.2f + 0.8f * std::clamp(digit_integrity, 0.0f, 1.0f));
}

inline HitQuality hit_quality(float net_margin, float effective_accuracy) {
    const float ratio = effective_accuracy > 0.0f
        ? std::clamp(net_margin / effective_accuracy, 0.0f, 1.0f)
        : 0.0f;
    if (ratio < 0.20f) return HitQuality::GLANCING;
    if (ratio < 0.80f) return HitQuality::SOLID;
    if (ratio < 0.97f) return HitQuality::STRONG;
    return HitQuality::CRITICAL;
}

inline HitQuality downgrade_quality(HitQuality quality) {
    switch (quality) {
        case HitQuality::CRITICAL: return HitQuality::STRONG;
        case HitQuality::STRONG: return HitQuality::SOLID;
        case HitQuality::SOLID:
        case HitQuality::GLANCING:
        default:
            return HitQuality::GLANCING;
    }
}

inline float hit_quality_multiplier(HitQuality quality) {
    switch (quality) {
        case HitQuality::GLANCING: return 0.65f;
        case HitQuality::SOLID: return 1.0f;
        case HitQuality::STRONG: return 1.15f;
        case HitQuality::CRITICAL: return 1.25f;
    }
    return 1.0f;
}

inline float targeted_location_penalty(float part_weight, float total_weight) {
    if (part_weight <= 0.0f || total_weight <= 0.0f) return 0.35f;
    const float share = part_weight / total_weight;
    if (share >= 0.25f) return 0.0f;
    return std::clamp(
        0.05f * std::log2(0.25f / share),
        0.0f,
        0.35f);
}

inline float stamina_recovery(float maximum, float elapsed, float multiplier = 1.0f) {
    return std::max(0.0f, maximum) *
        STAMINA_REGEN_FRACTION_PER_TIME *
        std::max(0.0f, elapsed) *
        std::max(0.0f, multiplier);
}

inline float stamina_accuracy_modifier(float stamina_percent) {
    const float pct = std::clamp(stamina_percent, 0.0f, 1.0f);
    if (pct >= 0.5f) return 0.0f;
    if (pct >= 0.25f) return -0.02f * ((0.5f - pct) / 0.25f);
    return -0.02f - 0.04f * ((0.25f - pct) / 0.25f);
}

inline float stamina_speed_multiplier(float stamina_percent) {
    const float pct = std::clamp(stamina_percent, 0.0f, 1.0f);
    if (pct >= 0.5f) return 1.0f;
    if (pct >= 0.25f) return 1.0f - 0.13f * ((0.5f - pct) / 0.25f);
    return 0.87f - 0.20f * ((0.25f - pct) / 0.25f);
}

inline float armor_damage_after_covered_layer(
    float incoming_damage,
    DamageType damage_type,
    float resistance,
    float bash_transmission
) {
    const float incoming = std::max(0.0f, incoming_damage);
    const float reduced = std::max(0.0f, incoming - std::max(0.0f, resistance));
    if (damage_type != DamageType::BASH) return reduced;
    return std::max(
        incoming * std::clamp(bash_transmission, 0.0f, 1.0f),
        reduced);
}

inline bool armor_coverage_applies(float roll, float coverage) {
    return std::clamp(roll, 0.0f, 1.0f) <
        std::clamp(coverage, 0.0f, 1.0f);
}

inline float scaled_part_integrity(
    float base_integrity,
    float anatomy_scale,
    float part_scale = 1.0f
) {
    return std::max(0.0f, base_integrity) *
        std::max(0.01f, anatomy_scale) *
        std::max(0.01f, part_scale);
}

}

#endif // SPACETRAVELLER_COMBAT_MATH_H
