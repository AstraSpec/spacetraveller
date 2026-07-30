#ifndef SPACETRAVELLER_EQUIPMENT_H
#define SPACETRAVELLER_EQUIPMENT_H

#include <godot_cpp/variant/string.hpp>
#include <cstdint>
#include <map>
#include <vector>
#include <godot_cpp/variant/dictionary.hpp>

namespace godot {

struct EquipmentSlot {
    String item_id;
    int durability = -1;
};

struct EquipmentData {
    std::map<String, EquipmentSlot> slots;
};

struct WeaponHandling {
    String slot_name;
    String item_id;
    float load = 1.0f;
    float allocated = 0.0f;
    float ratio = 0.0f;
};

namespace Equipment {
    constexpr const char* MAIN_HAND_SLOT = "main_hand";
    constexpr const char* OFF_HAND_SLOT = "off_hand";

    void init(EquipmentData& data);
    bool equip(EquipmentData& data, const String& slot_name, const String& item_id);
    bool unequip(EquipmentData& data, const String& slot_name);
    bool is_slot_occupied(const EquipmentData& data, const String& slot_name);
    const EquipmentSlot* get_slot(const EquipmentData& data, const String& slot_name);
    std::vector<String> get_wielded_weapon_ids(const EquipmentData& data);
    int get_wielded_weapon_count(const EquipmentData& data);
    float get_weapon_load(const String& item_id);
    std::vector<WeaponHandling> get_weapon_handling(
        const EquipmentData& data,
        float manipulation_units);
    float get_handling_ratio(
        const EquipmentData& data,
        const String& slot_name,
        float manipulation_units);
    bool can_retain_all_weapons(
        const EquipmentData& data,
        float manipulation_units,
        float minimum_ratio = 0.25f);
    std::vector<WeaponHandling> reconcile_handling(
        EquipmentData& data,
        float manipulation_units,
        float minimum_ratio = 0.25f);
    float handling_accuracy_modifier(float ratio);
    float handling_speed_multiplier(float ratio);
    float handling_damage_multiplier(float ratio);
    float get_attack_power(const EquipmentData& data);
    Dictionary serialize(const EquipmentData& data);
    void deserialize(EquipmentData& data, const Dictionary& dict);
}

}

#endif // SPACETRAVELLER_EQUIPMENT_H
