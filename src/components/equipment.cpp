#include "equipment.h"
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>

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

float Equipment::get_attack_power(const EquipmentData& data) {
    return 0.0f;
}

float Equipment::get_armor_rating(const EquipmentData& data) {
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
