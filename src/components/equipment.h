#ifndef SPACETRAVELLER_EQUIPMENT_H
#define SPACETRAVELLER_EQUIPMENT_H

#include <godot_cpp/variant/string.hpp>
#include <cstdint>
#include <map>
#include <godot_cpp/variant/dictionary.hpp>

namespace godot {

struct EquipmentSlot {
    String item_id;
    int durability = -1;
};

struct EquipmentData {
    std::map<String, EquipmentSlot> slots;
};

namespace Equipment {
    void init(EquipmentData& data);
    bool equip(EquipmentData& data, const String& slot_name, const String& item_id);
    bool unequip(EquipmentData& data, const String& slot_name);
    bool is_slot_occupied(const EquipmentData& data, const String& slot_name);
    const EquipmentSlot* get_slot(const EquipmentData& data, const String& slot_name);
    float get_attack_power(const EquipmentData& data);
    float get_armor_rating(const EquipmentData& data);
    Dictionary serialize(const EquipmentData& data);
    void deserialize(EquipmentData& data, const Dictionary& dict);
}

}

#endif // SPACETRAVELLER_EQUIPMENT_H
