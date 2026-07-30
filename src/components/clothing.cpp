#include "clothing.h"
#include "anatomy.h"
#include "combat_math.h"
#include "data/item_db.h"
#include <algorithm>
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

int protection_layer_order(const String& layer) {
    if (layer == "outer") return 0;
    if (layer == "armor") return 1;
    if (layer == "middle") return 2;
    if (layer == "under") return 3;
    return 4;
}

ProtectionLayer to_protection_layer(
    const String& item_id,
    const ClothingSlotInfo& slot
) {
    ProtectionLayer layer;
    layer.item_id = item_id;
    layer.layer = slot.layer;
    layer.coverage = slot.coverage;
    layer.bash = slot.bash;
    layer.cut = slot.cut;
    layer.pierce = slot.pierce;
    layer.bash_transmission = slot.bash_transmission;
    return layer;
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

std::vector<ProtectionLayer> Clothing::get_protection_layers_for_part(
    const ClothingData& data,
    const AnatomyData& anatomy,
    int part_index
) {
    std::vector<ProtectionLayer> layers;
    if (!Anatomy::is_functional(anatomy, part_index)) return layers;

    ItemDb* db = ItemDb::get_singleton();
    if (!db) return layers;

    auto part_it = data.equipped.find(part_index);
    if (part_it == data.equipped.end()) return layers;

    for (const auto& layer_pair : part_it->second) {
        const std::vector<ClothingSlotInfo>* slots =
            db->get_clothing_slots_info(layer_pair.second);
        const ClothingSlotInfo* slot =
            find_matching_slot(slots, anatomy, part_index, layer_pair.first);
        if (slot) {
            layers.push_back(to_protection_layer(layer_pair.second, *slot));
        }
    }

    std::stable_sort(
        layers.begin(),
        layers.end(),
        [](const ProtectionLayer& lhs, const ProtectionLayer& rhs) {
            const int lhs_order = protection_layer_order(lhs.layer);
            const int rhs_order = protection_layer_order(rhs.layer);
            if (lhs_order != rhs_order) return lhs_order < rhs_order;
            return lhs.layer < rhs.layer;
        }
    );
    return layers;
}

float Clothing::get_resistance(const ProtectionLayer& layer, DamageType damage_type) {
    switch (damage_type) {
        case DamageType::BASH:
            return std::max(0.0f, layer.bash);
        case DamageType::CUT:
            return std::max(0.0f, layer.cut);
        case DamageType::PIERCE:
            return std::max(0.0f, layer.pierce);
    }
    return 0.0f;
}

float Clothing::apply_covered_layer(
    float incoming_damage,
    DamageType damage_type,
    const ProtectionLayer& layer
) {
    return CombatMath::armor_damage_after_covered_layer(
        incoming_damage,
        damage_type,
        get_resistance(layer, damage_type),
        layer.bash_transmission);
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
