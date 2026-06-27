#include "social_profile.h"
#include <godot_cpp/variant/variant.hpp>

using namespace godot;

void SocialProfile::init(SocialProfileData& data) {
    data.job = "drifter";
    data.dialogue_id = "";
    data.faction = "";
    data.traits.clear();
    data.context_tags.clear();
}

Dictionary SocialProfile::serialize(const SocialProfileData& data) {
    Dictionary d;
    d["job"] = data.job;
    if (!data.dialogue_id.is_empty()) {
        d["dialogue_id"] = data.dialogue_id;
    }
    d["faction"] = data.faction;
    d["traits"] = data.traits;
    d["context_tags"] = data.context_tags;
    return d;
}

void SocialProfile::deserialize(SocialProfileData& data, const Dictionary& dict) {
    data.job = String(dict.get("job", "drifter"));
    data.dialogue_id = String(dict.get("dialogue_id", ""));
    data.faction = String(dict.get("faction", ""));
    data.traits = dict.get("traits", Array());
    data.context_tags = dict.get("context_tags", Array());
}
