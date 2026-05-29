#ifndef SPACETRAVELLER_ANATOMY_H
#define SPACETRAVELLER_ANATOMY_H

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <vector>

namespace godot {

struct BodyPart {
    String type_id;
    int parent_index = -1;
    float integrity = 1.0f;
    int local_index = 0;
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
    inline int get_parent(const AnatomyData& data, int index);
    inline float get_integrity(const AnatomyData& data, int index);
    inline void set_integrity(AnatomyData& data, int index, float integrity);
    inline int get_count(const AnatomyData& data);
    Dictionary get_functional_list(const AnatomyData& data);
    Dictionary serialize(const AnatomyData& data);
    void deserialize(AnatomyData& data, const Dictionary& dict);
}

}

#endif // ! SPACETRAVELLER_ANATOMY_H