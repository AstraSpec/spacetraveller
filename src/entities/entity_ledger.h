#ifndef SPACETRAVELLER_ENTITY_LEDGER_H
#define SPACETRAVELLER_ENTITY_LEDGER_H

#include "entity_pool.h"
#include "components/anatomy.h"
#include "components/clothing.h"
#include "components/inventory.h"
#include "components/health.h"
#include "components/stamina.h"
#include "components/effects.h"
#include "components/equipment.h"
#include "components/locomotion.h"
#include "components/perception.h"
#include "components/ai_controller.h"

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/vector2i.hpp>

namespace godot {

class EntityLedger {
private:
    EntityPool entity_pool;

public:
    EntityPool& get_entity_pool() { return entity_pool; }
    const EntityPool& get_entity_pool() const { return entity_pool; }

    std::unordered_map<uint32_t, AnatomyData> anatomy_data;
    std::unordered_map<uint32_t, ClothingData> clothing_data;
    std::unordered_map<uint32_t, InventoryData> inventory_data;
    std::unordered_map<uint32_t, HealthData> health_data;
    std::unordered_map<uint32_t, StaminaData> stamina_data;
    std::unordered_map<uint32_t, EffectsData> effects_data;
    std::unordered_map<uint32_t, EquipmentData> equipment_data;
    std::unordered_map<uint32_t, LocomotionData> locomotion_data;
    std::unordered_map<uint32_t, PerceptionMemory> perception_memory;
    std::unordered_map<uint32_t, AIData> ai_data;
    std::unordered_map<uint32_t, String> combat_style;
    std::unordered_map<uint32_t, String> gender;
    std::unordered_map<uint32_t, String> entity_name;

    uint32_t spawn_entity(const Vector2i& pos, uint16_t atlas_x, uint16_t atlas_y, const String& race_id);
    uint32_t spawn_player(const Vector2i& pos, uint16_t atlas_x, uint16_t atlas_y);
    void destroy_entity(uint32_t id);

    int get_inventory_item_amount(uint32_t id, const String& item_id) const;

    Dictionary get_anatomy(uint32_t id) const;
    Dictionary get_clothing(uint32_t id) const;
    Dictionary get_inventory(uint32_t id) const;
    Dictionary get_health(uint32_t id) const;
    Dictionary get_stamina(uint32_t id) const;
    Dictionary get_effects(uint32_t id) const;
    float get_inventory_weight(uint32_t id) const;
    float get_inventory_volume(uint32_t id) const;
    float get_armor_rating(uint32_t id) const;
    String get_anatomy_part_name(uint32_t id, int part_index) const;

    bool add_inventory_item(uint32_t id, const String& item_id, int amount);
    bool remove_inventory_item(uint32_t id, const String& item_id, int amount);
    bool equip_clothing(uint32_t id, int part_index, const String& item_id, const String& layer);
    bool unequip_clothing(uint32_t id, const String& item_id);
    bool equip_clothing_by_string(uint32_t id, const String& item_id);
    bool unequip_clothing_by_string(uint32_t id, const String& item_id);

    Dictionary serialize() const;
    void deserialize(const Dictionary& data);

    Dictionary serialize_entity(uint32_t id) const;
    uint32_t deserialize_entity(const Dictionary& data);
};

}

#endif // ! SPACETRAVELLER_ENTITY_LEDGER_H