#include "clothing.h"
#include "data/item_db.h"
#include "anatomy.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void Clothing::_bind_methods() {
    ClassDB::bind_method(D_METHOD("equip_item", "item_id", "part_index"), &Clothing::equip_item);
    ClassDB::bind_method(D_METHOD("unequip_item", "item_id"), &Clothing::unequip_item);
    ClassDB::bind_method(D_METHOD("is_equipped", "item_id"), &Clothing::is_equipped);
    ClassDB::bind_method(D_METHOD("get_total_armor"), &Clothing::get_total_armor);
    ClassDB::bind_method(D_METHOD("get_equipped_items_list"), &Clothing::get_equipped_items_list);
    ClassDB::bind_method(D_METHOD("get_equipped_at", "part_index", "layer"), &Clothing::get_equipped_at);

    ClassDB::bind_method(D_METHOD("get_save_data"), &Clothing::get_save_data);
    ClassDB::bind_method(D_METHOD("load_save_data", "data"), &Clothing::load_save_data);
}

Clothing::Clothing() {}
Clothing::~Clothing() {}

bool Clothing::equip_item(const String &p_item_id, int p_part_index) {
    ItemDb *db = ItemDb::get_singleton();
    if (!db) return false;

    Dictionary data = db->get_clothing_data(p_item_id);
    if (data.is_empty()) return false;

    // 1. Check if the part index is valid and functional
    Anatomy *anatomy = nullptr;
    Node *parent = get_parent();
    if (parent) {
        anatomy = parent->get_node<Anatomy>("Anatomy");
    }

    if (!anatomy) return false;
    if (!anatomy->is_part_functional(p_part_index)) return false;

    // 2. Check if the item supports this part type
    String part_type_needed = data.get("part", "");
    if (part_type_needed != "" && anatomy->get_part_type_id(p_part_index) != part_type_needed) {
        return false;
    }

    String layer = data.get("layer", "middle");

    // 3. Equip!
    equipped_items[p_part_index][layer] = p_item_id;
    return true;
}

bool Clothing::unequip_item(const String &p_item_id) {
    for (auto &part_pair : equipped_items) {
        for (auto it = part_pair.second.begin(); it != part_pair.second.end(); ++it) {
            if (it->second == p_item_id) {
                part_pair.second.erase(it);
                return true;
            }
        }
    }
    return false;
}

bool Clothing::is_equipped(const String &p_item_id) const {
    for (const auto &part_pair : equipped_items) {
        for (const auto &layer_pair : part_pair.second) {
            if (layer_pair.second == p_item_id) {
                return true;
            }
        }
    }
    return false;
}

float Clothing::get_total_armor() const {
    float total = 0.0f;
    ItemDb *db = ItemDb::get_singleton();
    if (!db) return 0.0f;

    Anatomy *anatomy = nullptr;
    Node *parent = get_parent();
    if (parent) {
        anatomy = parent->get_node<Anatomy>("Anatomy");
    }

    for (const auto &part_pair : equipped_items) {
        // Only sum armor from functional parts
        if (anatomy && !anatomy->is_part_functional(part_pair.first)) {
            continue;
        }

        for (const auto &layer_pair : part_pair.second) {
            Dictionary data = db->get_clothing_data(layer_pair.second);
            total += (float)data.get("armor", 0.0f);
        }
    }
    return total;
}

Array Clothing::get_equipped_items_list() const {
    Array list;
    Anatomy *anatomy = nullptr;
    Node *parent = get_parent();
    if (parent) {
        anatomy = parent->get_node<Anatomy>("Anatomy");
    }

    for (const auto &part_pair : equipped_items) {
        for (const auto &layer_pair : part_pair.second) {
            Dictionary item;
            item["id"] = layer_pair.second;
            item["part_index"] = part_pair.first;
            item["part_name"] = anatomy ? anatomy->get_part_name(part_pair.first) : "Unknown Part";
            item["layer"] = layer_pair.first;
            list.push_back(item);
        }
    }
    return list;
}

Dictionary Clothing::get_equipped_at(int p_part_index, const String &p_layer) const {
    auto part_it = equipped_items.find(p_part_index);
    if (part_it != equipped_items.end()) {
        auto layer_it = part_it->second.find(p_layer);
        if (layer_it != part_it->second.end()) {
            Dictionary d;
            d["id"] = layer_it->second;
            return d;
        }
    }
    return Dictionary();
}

Dictionary Clothing::get_save_data() const {
    Dictionary data;
    Dictionary parts;
    for (const auto& part_pair : equipped_items) {
        Dictionary layers;
        for (const auto& layer_pair : part_pair.second) {
            layers[layer_pair.first] = layer_pair.second;
        }
        parts[part_pair.first] = layers;
    }
    data["equipped"] = parts;
    return data;
}

void Clothing::load_save_data(const Dictionary &p_data) {
    equipped_items.clear();
    Dictionary parts = p_data.get("equipped", Dictionary());
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
            equipped_items[part_idx][layer] = item_id;
        }
    }
}

}
