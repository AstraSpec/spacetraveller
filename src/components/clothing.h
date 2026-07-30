#ifndef SPACETRAVELLER_CLOTHING_H
#define SPACETRAVELLER_CLOTHING_H

#include "damage.h"
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <map>
#include <vector>

namespace godot {

struct AnatomyData;

struct ClothingData {
    // part_index -> layer -> item_id
    std::map<int, std::map<String, String>> equipped;
};

struct ProtectionLayer {
    String item_id;
    String layer;
    float coverage = 1.0f;
    float bash = 0.0f;
    float cut = 0.0f;
    float pierce = 0.0f;
    float bash_transmission = 0.0f;
};

namespace Clothing {
    void init(ClothingData& data);
    bool equip(ClothingData& data, const class AnatomyData& anatomy, int part_index, const String& item_id, const String& layer);
    bool equip_item(ClothingData& data, const class AnatomyData& anatomy, const String& item_id);
    bool unequip(ClothingData& data, const String& item_id);
    bool is_equipped(const ClothingData& data, const String& item_id);
    std::vector<ProtectionLayer> get_protection_layers_for_part(
        const ClothingData& data,
        const class AnatomyData& anatomy,
        int part_index
    );
    float get_resistance(const ProtectionLayer& layer, DamageType damage_type);
    float apply_covered_layer(float incoming_damage, DamageType damage_type, const ProtectionLayer& layer);
    Dictionary get_list(const ClothingData& data, const class AnatomyData& anatomy);
    Dictionary get_at(const ClothingData& data, int part_index, const String& layer);
    Dictionary serialize(const ClothingData& data);
    void deserialize(ClothingData& data, const Dictionary& dict);
}

}

#endif // SPACETRAVELLER_CLOTHING_H
