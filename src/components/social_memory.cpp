#include "social_memory.h"
#include <godot_cpp/variant/variant.hpp>

using namespace godot;

void SocialMemory::init(SocialMemoryData& data) {
    data.cooldown_available_at = 0;
    data.conversation_state_json = "";
}

Dictionary SocialMemory::serialize(const SocialMemoryData& data) {
    Dictionary d;
    d["cooldown_available_at"] = data.cooldown_available_at;
    d["conversation_state_json"] = data.conversation_state_json;
    return d;
}

void SocialMemory::deserialize(SocialMemoryData& data, const Dictionary& dict) {
    data.cooldown_available_at = static_cast<int>(dict.get("cooldown_available_at", 0));
    data.conversation_state_json = dict.get("conversation_state_json", "");
}
