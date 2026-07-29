#ifndef SPACETRAVELLER_ANATOMY_H
#define SPACETRAVELLER_ANATOMY_H

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <vector>
#include <cstdint>
#include "core/rng.h"

namespace godot {

struct BodyPart {
    String type_id;
    int parent_index = -1;
    float integrity = 1.0f;
    int local_index = 0;
    String height = "MID";
};

struct BodyCapabilities {
    float moving = 1.0f;
    float manipulation = 1.0f;
    float perception = 1.0f;
    float speech = 1.0f;
};

struct AnatomyData {
    String race_id;
    std::vector<BodyPart> parts;
    BodyCapabilities capabilities;
};

namespace Anatomy {
    void init(AnatomyData& data, const String& race_id);
    int find_part(const AnatomyData& data, const String& type_id, int skip = 0);
    bool is_functional(const AnatomyData& data, int index);
    String get_type_id(const AnatomyData& data, int index);
    String get_name(const AnatomyData& data, int index);
    int get_parent(const AnatomyData& data, int index);
    float get_integrity(const AnatomyData& data, int index);
    void set_integrity(AnatomyData& data, int index, float integrity);
    int get_count(const AnatomyData& data);
    Dictionary get_functional_list(const AnatomyData& data);
    bool has_functional_limbs(const AnatomyData& data, const std::vector<String>& required);
    int count_functional_parts_with_tag(const AnatomyData& data, const String& tag);
    float min_required_integrity(const AnatomyData& data, const std::vector<String>& required);
    void refresh_capabilities(AnatomyData& data);
    float get_moving_capacity(const AnatomyData& data);
    float get_manipulation_capacity(const AnatomyData& data);
    float get_perception_capacity(const AnatomyData& data);
    float get_speech_capacity(const AnatomyData& data);
    Dictionary get_capabilities(const AnatomyData& data);
    int pick_hit_location(const AnatomyData& data, const std::vector<String>& preferred_heights = std::vector<String>());
    int pick_hit_location(const AnatomyData& data, const std::vector<String>& preferred_heights, Rng::Seeded& rng);
    Dictionary serialize(const AnatomyData& data);
    void deserialize(AnatomyData& data, const Dictionary& dict);
}

}

#endif // ! SPACETRAVELLER_ANATOMY_H
