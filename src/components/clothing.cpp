#include "clothing.h"
#include "anatomy.h"
#include "data/item_db.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <vector>

namespace godot {

namespace {

struct ClothingAssignment {
    int part_index = -1;
    String layer;
    String item_id;
};

const ClothingSlotInfo* find_matching_slot(
    const std::vector<ClothingSlotInfo>* slots,
    const AnatomyData& anatomy,
    int part_index,
    const String& layer
) {
    if (!slots || part_index < 0 || part_index >= anatomy.parts.size()) return nullptr;

    String part_type = Anatomy::get_type_id(anatomy, part_index);
    for (const ClothingSlotInfo& slot : *slots) {
        if (slot.part == part_type && slot.layer == layer) {
            return &slot;
        }
    }
    return nullptr;
}

void add_matching_assignments(
    std::vector<ClothingAssignment>& assignments,
    const AnatomyData& anatomy,
    const String& item_id,
    const ClothingSlotInfo& slot
) {
    for (int i = 0; i < anatomy.parts.size(); i++) {
        if (anatomy.parts[i].type_id == slot.part && Anatomy::is_functional(anatomy, i)) {
            assignments.push_back({i, slot.layer, item_id});
        }
    }
}

bool assignment_slot_is_free(const ClothingData& data, const ClothingAssignment& assignment) {
    auto part_it = data.equipped.find(assignment.part_index);
    if (part_it == data.equipped.end()) return true;

    auto layer_it = part_it->second.find(assignment.layer);
    return layer_it == part_it->second.end();
}

}

void Clothing::init(ClothingData& data) {
    data.equipped.clear();
}

bool Clothing::equip(ClothingData& data, const AnatomyData& anatomy, int part_index, const String& item_id, const String& layer) {
    if (!Anatomy::is_functional(anatomy, part_index)) return false;

    ItemDb* db = ItemDb::get_singleton();
    if (!db) return false;

    const std::vector<ClothingSlotInfo>* slots = db->get_clothing_slots_info(item_id);
    if (!find_matching_slot(slots, anatomy, part_index, layer)) return false;

    ClothingAssignment assignment{part_index, layer, item_id};
    if (!assignment_slot_is_free(data, assignment)) return false;

    data.equipped[assignment.part_index][assignment.layer] = assignment.item_id;
    return true;
}

bool Clothing::equip_item(ClothingData& data, const AnatomyData& anatomy, const String& item_id) {
    ItemDb* db = ItemDb::get_singleton();
    if (!db) return false;

    const std::vector<ClothingSlotInfo>* slots = db->get_clothing_slots_info(item_id);
    if (!slots || slots->empty()) return false;

    std::vector<ClothingAssignment> assignments;
    for (const ClothingSlotInfo& slot : *slots) {
        add_matching_assignments(assignments, anatomy, item_id, slot);
    }

    if (assignments.empty()) return false;
    for (const ClothingAssignment& assignment : assignments) {
        if (!assignment_slot_is_free(data, assignment)) return false;
    }

    for (const ClothingAssignment& assignment : assignments) {
        data.equipped[assignment.part_index][assignment.layer] = assignment.item_id;
    }
    return true;
}

bool Clothing::unequip(ClothingData& data, const String& item_id) {
    bool removed = false;
    for (auto& part_pair : data.equipped) {
        for (auto it = part_pair.second.begin(); it != part_pair.second.end();) {
            if (it->second == item_id) {
                it = part_pair.second.erase(it);
                removed = true;
            } else {
                ++it;
            }
        }
    }
    return removed;
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
    for (const auto& part_pair : data.equipped) {
        if (!Anatomy::is_functional(anatomy, part_pair.first)) continue;
        total += get_armor_for_part(data, anatomy, part_pair.first);
    }
    return total;
}

float Clothing::get_armor_for_part(const ClothingData& data, const AnatomyData& anatomy, int part_index) {
    if (!Anatomy::is_functional(anatomy, part_index)) return 0.0f;

    ItemDb* db = ItemDb::get_singleton();
    if (!db) return 0.0f;

    auto part_it = data.equipped.find(part_index);
    if (part_it == data.equipped.end()) return 0.0f;

    float total = 0.0f;
    for (const auto& layer_pair : part_it->second) {
        const std::vector<ClothingSlotInfo>* slots = db->get_clothing_slots_info(layer_pair.second);
        const ClothingSlotInfo* slot = find_matching_slot(slots, anatomy, part_index, layer_pair.first);
        if (slot) {
            total += slot->armor;
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
