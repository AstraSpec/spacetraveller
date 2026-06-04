#ifndef SPACETRAVELLER_SOCIAL_MEMORY_H
#define SPACETRAVELLER_SOCIAL_MEMORY_H

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace godot {

struct SocialMemoryData {
    int cooldown_available_at = 0;   // 0 = no cooldown
    String conversation_state_json;  // JSON-encoded GDScript dict
};

namespace SocialMemory {
    void init(SocialMemoryData& data);
    Dictionary serialize(const SocialMemoryData& data);
    void deserialize(SocialMemoryData& data, const Dictionary& dict);
}

}

#endif // SPACETRAVELLER_SOCIAL_MEMORY_H
