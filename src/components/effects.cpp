#include "effects.h"
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/array.hpp>
#include <algorithm>

using namespace godot;

void Effects::add(EffectsData& data, const Effect& e) {
    // Bleed stacks per limb; stun stacks into a single body effect of the same mode.
    for (auto& existing : data.effects) {
        if (existing.type == e.type && existing.scope == e.scope &&
            existing.target_part == e.target_part && existing.mode == e.mode) {
            existing.magnitude = std::min(1.0f, existing.magnitude + e.magnitude);
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
            it->magnitude -= it->rate * dt;
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

float Effects::get_stun(const EffectsData& data) {
    float total = 0.0f;
    for (const auto& e : data.effects) {
        if (e.type == "stun") total += e.magnitude;
    }
    return std::min(1.0f, total);
}

float Effects::total_bleed(const EffectsData& data) {
    float total = 0.0f;
    for (const auto& e : data.effects) {
        if (e.type == "bleed") total += e.magnitude;
    }
    return total;
}

bool Effects::is_stunned(const EffectsData& data) {
    return get_stun(data) > EffectTuning::STUN_FREEZE_THRESHOLD;
}

Effect Effects::make_bleed(int target_part, float magnitude) {
    Effect e;
    e.type = "bleed";
    e.scope = (int)EffectScope::LIMB;
    e.target_part = target_part;
    e.magnitude = magnitude;
    e.mode = (int)EffectMode::DECAY;
    e.rate = EffectTuning::BLEED_DECAY_RATE;
    return e;
}

Effect Effects::make_stun_decay(float magnitude) {
    Effect e;
    e.type = "stun";
    e.scope = (int)EffectScope::BODY;
    e.target_part = -1;
    e.magnitude = magnitude;
    e.mode = (int)EffectMode::DECAY;
    e.rate = EffectTuning::STUN_DECAY_RATE;
    return e;
}

Effect Effects::make_stun_timer(float duration, float magnitude) {
    Effect e;
    e.type = "stun";
    e.scope = (int)EffectScope::BODY;
    e.target_part = -1;
    e.magnitude = magnitude;
    e.mode = (int)EffectMode::TIMER;
    e.timer = duration;
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
        e.scope = ed.get("scope", (int)EffectScope::BODY);
        e.target_part = ed.get("target_part", -1);
        e.magnitude = static_cast<float>(static_cast<double>(ed.get("magnitude", 0.0)));
        e.mode = ed.get("mode", (int)EffectMode::DECAY);
        e.rate = static_cast<float>(static_cast<double>(ed.get("rate", 0.0)));
        e.timer = static_cast<float>(static_cast<double>(ed.get("timer", 0.0)));
        data.effects.push_back(e);
    }
}
