#ifndef SPACETRAVELLER_EFFECTS_H
#define SPACETRAVELLER_EFFECTS_H

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <vector>

namespace godot {

enum class EffectScope { LIMB, BODY };
enum class EffectMode { DECAY, TIMER };

struct Effect {
    String type;
    int scope = (int)EffectScope::BODY;
    int target_part = -1;
    float magnitude = 0.0f;
    int mode = (int)EffectMode::DECAY;
    float rate = 0.0f;  // DECAY
    float timer = 0.0f; // TIMER
};

struct EffectsData {
    std::vector<Effect> effects;
};

namespace EffectTuning {
    inline constexpr float BLEED_PER_HIT = 0.3f;
    inline constexpr float BLEED_BLOOD_PER_MAG = 0.125f;
}

namespace Effects {
    void add(EffectsData& data, const Effect& e);
    void tick(EffectsData& data, float dt, std::vector<int>* expired_bleed_parts = nullptr);
    float total_bleed(const EffectsData& data);
    float bleeding_pain_floor(const EffectsData& data);
    float bleed_blood_loss(const EffectsData& data, float elapsed);

    Effect make_bleed(int target_part, float magnitude);

    Dictionary serialize(const EffectsData& data);
    void deserialize(EffectsData& data, const Dictionary& dict);
}

}

#endif // SPACETRAVELLER_EFFECTS_H
