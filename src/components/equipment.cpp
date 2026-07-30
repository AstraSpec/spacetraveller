#include "equipment.h"
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>
#include "data/item_db.h"
#include "combat_math.h"
#include <algorithm>

using namespace godot;

void Equipment::init(EquipmentData& data) {
    data.slots.clear();
}

bool Equipment::equip(EquipmentData& data, const String& slot_name, const String& item_id) {
    data.slots[slot_name] = {item_id, -1};
    return true;
}

bool Equipment::unequip(EquipmentData& data, const String& slot_name) {
    auto it = data.slots.find(slot_name);
    if (it == data.slots.end()) return false;
    data.slots.erase(it);
    return true;
}

bool Equipment::is_slot_occupied(const EquipmentData& data, const String& slot_name) {
    return data.slots.find(slot_name) != data.slots.end();
}

const EquipmentSlot* Equipment::get_slot(const EquipmentData& data, const String& slot_name) {
    auto it = data.slots.find(slot_name);
    if (it != data.slots.end()) return &it->second;
    return nullptr;
}

std::vector<String> Equipment::get_wielded_weapon_ids(const EquipmentData& data) {
    std::vector<String> result;
    const EquipmentSlot* main = get_slot(data, MAIN_HAND_SLOT);
    if (main && !main->item_id.is_empty()) result.push_back(main->item_id);

    const EquipmentSlot* off = get_slot(data, OFF_HAND_SLOT);
    if (off && !off->item_id.is_empty()) result.push_back(off->item_id);
    return result;
}

int Equipment::get_wielded_weapon_count(const EquipmentData& data) {
    return static_cast<int>(get_wielded_weapon_ids(data).size());
}

float Equipment::get_weapon_load(const String& item_id) {
    ItemDb* db = ItemDb::get_singleton();
    const Dictionary weapon = db ? db->get_weapon_data(item_id) : Dictionary();
    if (weapon.is_empty()) return 1.0f;
    const float legacy = static_cast<float>(static_cast<double>(
        weapon.get("grasp_required", 1.0)));
    return std::max(0.01f, static_cast<float>(static_cast<double>(
        weapon.get("manipulation_load", legacy))));
}

std::vector<WeaponHandling> Equipment::get_weapon_handling(
    const EquipmentData& data,
    float manipulation_units
) {
    std::vector<WeaponHandling> result;
    float remaining = std::max(0.0f, manipulation_units);
    const String ordered_slots[] = {MAIN_HAND_SLOT, OFF_HAND_SLOT};
    for (const String& slot_name : ordered_slots) {
        const EquipmentSlot* slot = get_slot(data, slot_name);
        if (!slot || slot->item_id.is_empty()) continue;
        WeaponHandling handling;
        handling.slot_name = slot_name;
        handling.item_id = slot->item_id;
        handling.load = get_weapon_load(slot->item_id);
        handling.allocated = std::min(remaining, handling.load);
        handling.ratio = handling.allocated / handling.load;
        remaining -= handling.allocated;
        result.push_back(handling);
    }
    return result;
}

float Equipment::get_handling_ratio(
    const EquipmentData& data,
    const String& slot_name,
    float manipulation_units
) {
    for (const WeaponHandling& handling :
        get_weapon_handling(data, manipulation_units)) {
        if (handling.slot_name == slot_name) return handling.ratio;
    }
    return 0.0f;
}

bool Equipment::can_retain_all_weapons(
    const EquipmentData& data,
    float manipulation_units,
    float minimum_ratio
) {
    for (const WeaponHandling& handling :
        get_weapon_handling(data, manipulation_units)) {
        if (handling.ratio < minimum_ratio) return false;
    }
    return true;
}

std::vector<WeaponHandling> Equipment::reconcile_handling(
    EquipmentData& data,
    float manipulation_units,
    float minimum_ratio
) {
    std::vector<WeaponHandling> dropped;
    while (true) {
        const std::vector<WeaponHandling> handling =
            get_weapon_handling(data, manipulation_units);
        auto invalid = std::find_if(
            handling.begin(),
            handling.end(),
            [minimum_ratio](const WeaponHandling& entry) {
                return entry.ratio < minimum_ratio;
            });
        if (invalid == handling.end()) break;
        dropped.push_back(*invalid);
        unequip(data, invalid->slot_name);
    }
    return dropped;
}

float Equipment::handling_accuracy_modifier(float ratio) {
    return CombatMath::handling_accuracy_modifier(ratio);
}

float Equipment::handling_speed_multiplier(float ratio) {
    return CombatMath::handling_speed_multiplier(ratio);
}

float Equipment::handling_damage_multiplier(float ratio) {
    return CombatMath::handling_damage_multiplier(ratio);
}

float Equipment::get_attack_power(const EquipmentData& data) {
    return 0.0f;
}

Dictionary Equipment::serialize(const EquipmentData& data) {
    Dictionary d;
    for (const auto& [slot_name, slot] : data.slots) {
        Dictionary slot_d;
        slot_d[String("item_id")] = slot.item_id;
        slot_d[String("durability")] = slot.durability;
        d[slot_name] = slot_d;
    }
    return d;
}

void Equipment::deserialize(EquipmentData& data, const Dictionary& dict) {
    data.slots.clear();
    Array keys = dict.keys();
    for (int i = 0; i < keys.size(); i++) {
        String slot_name = keys[i];
        Dictionary slot_d = dict[slot_name];
        EquipmentSlot slot;
        slot.item_id = slot_d.get(String("item_id"), String(""));
        slot.durability = static_cast<int>(slot_d.get(String("durability"), -1));
        data.slots[slot_name] = slot;
    }
}
