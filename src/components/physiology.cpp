#include "physiology.h"

#include "combat_math.h"
#include <godot_cpp/variant/variant.hpp>

namespace godot {

namespace {

PhysiologyUpdate merge_updates(
    const PhysiologyUpdate& first,
    const PhysiologyUpdate& second
) {
    PhysiologyUpdate result;
    result.consciousness_lost =
        first.consciousness_lost + second.consciousness_lost;
    result.pain_added = first.pain_added + second.pain_added;
    result.newly_downed = first.newly_downed || second.newly_downed;
    result.recovered = first.recovered || second.recovered;
    result.reached_zero = first.reached_zero || second.reached_zero;
    return result;
}

}

void Physiology::init(PhysiologyData& data, float max_consciousness) {
    data.max_consciousness = MAX(1.0f, max_consciousness);
    data.current_consciousness = data.max_consciousness;
    data.pain = 0.0f;
    data.downed = false;
}

float Physiology::get_consciousness_percent(const PhysiologyData& data) {
    if (data.max_consciousness <= 0.0f) return 0.0f;
    return CLAMP(
        data.current_consciousness / data.max_consciousness,
        0.0f,
        1.0f);
}

float Physiology::get_consciousness_ceiling(
    const PhysiologyData& data,
    float blood_percent,
    float pain_floor
) {
    const float effective_pain = MAX(
        CLAMP(data.pain, 0.0f, 1.0f),
        CLAMP(pain_floor, 0.0f, 0.75f));
    return data.max_consciousness *
        CombatMath::consciousness_ceiling_fraction(
            blood_percent,
            effective_pain);
}

PhysiologyUpdate Physiology::reconcile(
    PhysiologyData& data,
    float blood_percent,
    float pain_floor
) {
    PhysiologyUpdate result;
    data.max_consciousness = MAX(1.0f, data.max_consciousness);
    data.pain = MAX(
        CLAMP(data.pain, 0.0f, 1.0f),
        CLAMP(pain_floor, 0.0f, 0.75f));
    const float before = data.current_consciousness;
    data.current_consciousness = CLAMP(
        data.current_consciousness,
        0.0f,
        get_consciousness_ceiling(data, blood_percent, pain_floor));
    result.consciousness_lost =
        MAX(0.0f, before - data.current_consciousness);
    result.reached_zero = data.current_consciousness <= 0.0f;

    const float fraction = get_consciousness_percent(data);
    const bool was_downed = data.downed;
    data.downed = CombatMath::should_be_downed(fraction, data.downed);
    if (!was_downed && data.downed) {
        result.newly_downed = true;
    } else if (was_downed && !data.downed) {
        result.recovered = true;
    }
    return result;
}

PhysiologyUpdate Physiology::apply_trauma(
    PhysiologyData& data,
    float consciousness_loss,
    float pain_gain,
    float blood_percent,
    float pain_floor
) {
    PhysiologyUpdate direct;
    const float old_consciousness = data.current_consciousness;
    const float old_pain = data.pain;
    data.current_consciousness = MAX(
        0.0f,
        data.current_consciousness - MAX(0.0f, consciousness_loss));
    data.pain = CLAMP(data.pain + MAX(0.0f, pain_gain), 0.0f, 1.0f);
    direct.consciousness_lost =
        old_consciousness - data.current_consciousness;
    direct.pain_added = data.pain - old_pain;
    return merge_updates(
        direct,
        reconcile(data, blood_percent, pain_floor));
}

PhysiologyUpdate Physiology::advance(
    PhysiologyData& data,
    float blood_percent,
    float elapsed,
    float action_recovery,
    float pain_floor
) {
    const float dt = MAX(0.0f, elapsed);
    const float recovery = CLAMP(action_recovery, 0.0f, 1.0f);
    data.pain = CombatMath::pain_after_elapsed(
        data.pain,
        dt,
        pain_floor);

    PhysiologyUpdate result =
        reconcile(data, blood_percent, pain_floor);
    if (result.reached_zero) return result;

    const float ceiling =
        get_consciousness_ceiling(data, blood_percent, pain_floor);
    data.current_consciousness = MIN(
        ceiling,
        data.current_consciousness +
            CombatMath::consciousness_recovery(
                blood_percent,
                data.pain,
                dt,
                recovery));
    return merge_updates(
        result,
        reconcile(data, blood_percent, pain_floor));
}

Dictionary Physiology::serialize(const PhysiologyData& data) {
    Dictionary result;
    result["current_consciousness"] = data.current_consciousness;
    result["max_consciousness"] = data.max_consciousness;
    result["pain"] = data.pain;
    result["downed"] = data.downed;
    return result;
}

void Physiology::deserialize(
    PhysiologyData& data,
    const Dictionary& dict
) {
    data.max_consciousness = MAX(
        1.0f,
        static_cast<float>(static_cast<double>(
            dict.get("max_consciousness", 100.0))));
    data.current_consciousness = CLAMP(
        static_cast<float>(static_cast<double>(
            dict.get("current_consciousness", data.max_consciousness))),
        0.0f,
        data.max_consciousness);
    data.pain = CLAMP(
        static_cast<float>(static_cast<double>(dict.get("pain", 0.0))),
        0.0f,
        1.0f);
    data.downed = dict.get("downed", false);
}

}
