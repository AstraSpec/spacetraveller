#ifndef SPACETRAVELLER_SOCIAL_PROFILE_H
#define SPACETRAVELLER_SOCIAL_PROFILE_H

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

struct SocialProfileData {
    String job = "drifter";
    String dialogue_id = "";
    Array traits;
    Array context_tags;
};

namespace SocialProfile {
    void init(SocialProfileData& data);
    Dictionary serialize(const SocialProfileData& data);
    void deserialize(SocialProfileData& data, const Dictionary& dict);
}

}

#endif // SPACETRAVELLER_SOCIAL_PROFILE_H
