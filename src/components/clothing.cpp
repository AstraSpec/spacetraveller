#include "clothing.h"
#include "anatomy.h"
#include "data/item_db.h"
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void Clothing::init(ClothingData& data) {
    data.equipped.clear();
}

bool Clothing::equip(ClothingData& data, const AnatomyData& anatomy, int part_index, const String& item_id, const String& layer) {
    if (!Anatomy::is_functional(anatomy, part_index)) return false;

    ItemDb* db = ItemDb::get_singleton();
    if (!db) return false;

    Dictionary item_data = db->get_clothing_data(item_id);
    if (item_data.is_empty()) return false;

    String part_type_needed = item_data.get("part", "");
    if (part_type_needed != "" && Anatomy::get_type_id(anatomy, part_index) != part_type_needed) {
        return false;
    }

    data.equipped[part_index][layer] = item_id;
    return true;
}

bool Clothing::unequip(ClothingData& data, const String& item_id) {
    for (auto& part_pair : data.equipped) {
        for (auto it = part_pair.second.begin(); it != part_pair.second.end(); ++it) {
            if (it->second == item_id) {
                part_pair.second.erase(it);
                return true;
            }
        }
    }
    return false;
}

bool Clothing::is_equipped(const ClothingData& data, const String& item_id) {
    for (const auto& part_pair : data.equipped) {
        for (const auto& layer_pair : part_pair.second) {
            if (layer_pair.second == item_id) return true;
        }
    }
    return false;
}

float Clothing::get_armor(const ClothingData& data, const AnatomyData& anatomy) {
    float total = 0.0f;
    ItemDb* db = ItemDb::get_singleton();
    if (!db) return 0.0f;

    for (const auto& part_pair : data.equipped) {
        if (!Anatomy::is_functional(anatomy, part_pair.first)) continue;

        for (const auto& layer_pair : part_pair.second) {
            Dictionary item_data = db->get_clothing_data(layer_pair.second);
            total += (float)item_data.get("armor", 0.0f);
        }
    }
    return total;
}

Dictionary Clothing::get_list(const ClothingData& data, const AnatomyData& anatomy) {
    Array list;
    for (const auto& part_pair : data.equipped) {
        for (const auto& layer_pair : part_pair.second) {
            Dictionary item;
            item["id"] = layer_pair.second;
            item["part_index"] = part_pair.first;
            item["part_name"] = Anatomy::get_name(anatomy, part_pair.first);
            item["layer"] = layer_pair.first;
            list.push_back(item);
        }
    }
    Dictionary result;
    result["items"] = list;
    return result;
}

Dictionary Clothing::get_at(const ClothingData& data, int part_index, const String& layer) {
    auto part_it = data.equipped.find(part_index);
    if (part_it != data.equipped.end()) {
        auto layer_it = part_it->second.find(layer);
        if (layer_it != part_it->second.end()) {
            Dictionary d;
            d["id"] = layer_it->second;
            return d;
        }
    }
    return Dictionary();
}

Dictionary Clothing::serialize(const ClothingData& data) {
    Dictionary result;
    Dictionary parts;
    for (const auto& part_pair : data.equipped) {
        Dictionary layers;
        for (const auto& layer_pair : part_pair.second) {
            layers[layer_pair.first] = layer_pair.second;
        }
        parts[part_pair.first] = layers;
    }
    result["equipped"] = parts;
    return result;
}

void Clothing::deserialize(ClothingData& data, const Dictionary& dict) {
    data.equipped.clear();
    Dictionary parts = dict.get("equipped", Dictionary());
    Array part_keys = parts.keys();
    for (int i = 0; i < part_keys.size(); i++) {
        Variant key_var = part_keys[i];
        int part_idx;
        if (key_var.get_type() == Variant::STRING) {
            part_idx = ((String)key_var).to_int();
        } else {
            part_idx = key_var;
        }
        Dictionary layers = parts[key_var];
        Array layer_keys = layers.keys();
        for (int j = 0; j < layer_keys.size(); j++) {
            String layer = layer_keys[j];
            String item_id = layers[layer_keys[j]];
            data.equipped[part_idx][layer] = item_id;
        }
    }
}

}