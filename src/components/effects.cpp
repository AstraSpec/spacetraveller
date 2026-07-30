#include "effects.h"
#include "combat_math.h"
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/array.hpp>
#include <algorithm>

using namespace godot;

void Effects::add(EffectsData& data, const Effect& e) {
    // Effects of the same type and target stack into one record.
    for (auto& existing : data.effects) {
        if (existing.type == e.type && existing.scope == e.scope &&
            existing.target_part == e.target_part && existing.mode == e.mode) {
            existing.magnitude = std::min(1.0f, existing.magnitude + e.magnitude);
            if (existing.type == "bleed") {
                existing.rate =
                    CombatMath::bleed_decay_rate(existing.magnitude);
            }
            if (e.mode == (int)EffectMode::TIMER) {
                existing.timer = std::max(existing.timer, e.timer);
            }
            return;
        }
    }
    data.effects.push_back(e);
}

void Effects::tick(EffectsData& data, float dt, std::vector<int>* expired_bleed_parts) {
    for (auto it = data.effects.begin(); it != data.effects.end(); ) {
        bool expired = false;
        if (it->mode == (int)EffectMode::TIMER) {
            it->timer -= dt;
            if (it->timer <= 0.0f) expired = true;
        } else {
            if (it->type == "bleed") {
                const CombatMath::BleedAdvance advanced =
                    CombatMath::advance_bleed(it->magnitude, dt);
                it->magnitude = advanced.magnitude;
                it->rate =
                    CombatMath::bleed_decay_rate(it->magnitude);
            } else {
                it->magnitude -= it->rate * dt;
            }
            if (it->magnitude <= 0.0f) expired = true;
        }
        if (expired) {
            if (it->type == "bleed" && expired_bleed_parts) {
                expired_bleed_parts->push_back(it->target_part);
            }
            it = data.effects.erase(it);
            continue;
        }
        ++it;
    }
}

float Effects::total_bleed(const EffectsData& data) {
    float total = 0.0f;
    for (const auto& e : data.effects) {
        if (e.type == "bleed") total += e.magnitude;
    }
    return total;
}

float Effects::bleeding_pain_floor(const EffectsData& data) {
    return CombatMath::bleeding_pain_floor(total_bleed(data));
}

float Effects::bleed_blood_loss(
    const EffectsData& data,
    float elapsed
) {
    float magnitude_time = 0.0f;
    for (const Effect& effect : data.effects) {
        if (effect.type != "bleed") continue;
        magnitude_time += CombatMath::advance_bleed(
            effect.magnitude,
            elapsed).magnitude_time;
    }
    return magnitude_time * EffectTuning::BLEED_BLOOD_PER_MAG;
}

Effect Effects::make_bleed(int target_part, float magnitude) {
    Effect e;
    e.type = "bleed";
    e.scope = (int)EffectScope::LIMB;
    e.target_part = target_part;
    e.magnitude = std::clamp(magnitude, 0.0f, 1.0f);
    e.mode = (int)EffectMode::DECAY;
    e.rate = CombatMath::bleed_decay_rate(e.magnitude);
    return e;
}

Dictionary Effects::serialize(const EffectsData& data) {
    Dictionary d;
    Array arr;
    for (const auto& e : data.effects) {
        Dictionary ed;
        ed["type"] = e.type;
        ed["scope"] = e.scope;
        ed["target_part"] = e.target_part;
        ed["magnitude"] = e.magnitude;
        ed["mode"] = e.mode;
        ed["rate"] = e.rate;
        ed["timer"] = e.timer;
        arr.push_back(ed);
    }
    d["effects"] = arr;
    return d;
}

void Effects::deserialize(EffectsData& data, const Dictionary& dict) {
    data.effects.clear();
    Array arr = dict.get("effects", Array());
    for (int i = 0; i < arr.size(); i++) {
        Dictionary ed = arr[i];
        Effect e;
        e.type = ed.get("type", "");
        if (e.type == "stun") continue;
        e.scope = ed.get("scope", (int)EffectScope::BODY);
        e.target_part = ed.get("target_part", -1);
        e.magnitude = static_cast<float>(static_cast<double>(ed.get("magnitude", 0.0)));
        e.mode = ed.get("mode", (int)EffectMode::DECAY);
        e.rate = static_cast<float>(static_cast<double>(ed.get("rate", 0.0)));
        e.timer = static_cast<float>(static_cast<double>(ed.get("timer", 0.0)));
        if (e.type == "bleed") {
            e.magnitude = std::clamp(e.magnitude, 0.0f, 1.0f);
            e.rate = CombatMath::bleed_decay_rate(e.magnitude);
        }
        data.effects.push_back(e);
    }
}
