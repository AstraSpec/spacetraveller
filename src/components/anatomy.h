#ifndef SPACETRAVELLER_ANATOMY_H
#define SPACETRAVELLER_ANATOMY_H

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <vector>
#include <cstdint>

namespace godot {

struct BodyPart {
    String type_id;
    int parent_index = -1;
    float integrity = 1.0f;
    int local_index = 0;
    String height = "MID";
};

struct AnatomyData {
    String race_id;
    std::vector<BodyPart> parts;
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
    float min_required_integrity(const AnatomyData& data, const std::vector<String>& required);
    int pick_hit_location(const AnatomyData& data, const std::vector<String>& preferred_heights = std::vector<String>());
    Dictionary serialize(const AnatomyData& data);
    void deserialize(AnatomyData& data, const Dictionary& dict);
}

}

#endif // ! SPACETRAVELLER_ANATOMY_H