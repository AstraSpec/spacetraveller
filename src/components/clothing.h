#ifndef SPACETRAVELLER_CLOTHING_H
#define SPACETRAVELLER_CLOTHING_H

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <map>

namespace godot {

struct AnatomyData;

struct ClothingData {
    // part_index -> layer -> item_id
    std::map<int, std::map<String, String>> equipped;
};

namespace Clothing {
    void init(ClothingData& data);
    bool equip(ClothingData& data, const class AnatomyData& anatomy, int part_index, const String& item_id, const String& layer);
    bool unequip(ClothingData& data, const String& item_id);
    bool is_equipped(const ClothingData& data, const String& item_id);
    float get_armor(const ClothingData& data, const class AnatomyData& anatomy);
    Dictionary get_list(const ClothingData& data, const class AnatomyData& anatomy);
    Dictionary get_at(const ClothingData& data, int part_index, const String& layer);
    Dictionary serialize(const ClothingData& data);
    void deserialize(ClothingData& data, const Dictionary& dict);
}

}

#endif // SPACETRAVELLER_CLOTHING_H