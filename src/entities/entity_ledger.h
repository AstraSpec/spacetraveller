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
#include "components/ai_controller.h"
#include "components/relationship_tuning.h"
#include "components/social_memory.h"
#include "components/social_profile.h"
#include "components/allegiance.h"

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <unordered_map>

namespace godot {

struct VendorState {
    static constexpr int DEFAULT_FUNDS = 1000;

    int funds = DEFAULT_FUNDS;
    int credit = 0;
    std::unordered_map<uint16_t, int> stock;
};

class EntityLedger {
private:
    EntityPool entity_pool;

    template<typename Map>
    static typename Map::mapped_type* try_get_ptr(Map& p_map, uint32_t id) {
        auto it = p_map.find(id);
        return it != p_map.end() ? &it->second : nullptr;
    }

    template<typename Map>
    static const typename Map::mapped_type* try_get_ptr(const Map& p_map, uint32_t id) {
        auto it = p_map.find(id);
        return it != p_map.end() ? &it->second : nullptr;
    }

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
    std::unordered_map<uint32_t, AIData> ai_data;
    std::unordered_map<uint32_t, String> combat_style;
    std::unordered_map<uint32_t, String> gender;
    std::unordered_map<uint32_t, String> entity_name;
    std::unordered_map<uint32_t, int> friendship;
    std::unordered_map<uint32_t, int> romance;
    std::unordered_map<uint32_t, SocialMemoryData> social_data;
    std::unordered_map<uint32_t, SocialProfileData> social_profiles;
    std::unordered_map<uint32_t, AllegianceData> allegiance_data;
    std::unordered_map<uint32_t, VendorState> vendor_state;

    static constexpr int RELATIONSHIP_SENTINEL = -1;

    uint32_t spawn_entity(const Vector2i& pos, uint16_t atlas_x, uint16_t atlas_y, const String& race_id);
    uint32_t spawn_entity(const Vector2i& pos, int z, uint16_t atlas_x, uint16_t atlas_y, const String& race_id);
    uint32_t spawn_player(const Vector2i& pos, uint16_t atlas_x, uint16_t atlas_y);
    void destroy_entity(uint32_t id);

    AnatomyData* try_get_anatomy(uint32_t id) { return try_get_ptr(anatomy_data, id); }
    const AnatomyData* try_get_anatomy(uint32_t id) const { return try_get_ptr(anatomy_data, id); }
    HealthData* try_get_health(uint32_t id) { return try_get_ptr(health_data, id); }
    const HealthData* try_get_health(uint32_t id) const { return try_get_ptr(health_data, id); }
    StaminaData* try_get_stamina(uint32_t id) { return try_get_ptr(stamina_data, id); }
    const StaminaData* try_get_stamina(uint32_t id) const { return try_get_ptr(stamina_data, id); }
    EquipmentData* try_get_equipment(uint32_t id) { return try_get_ptr(equipment_data, id); }
    const EquipmentData* try_get_equipment(uint32_t id) const { return try_get_ptr(equipment_data, id); }
    LocomotionData* try_get_locomotion(uint32_t id) { return try_get_ptr(locomotion_data, id); }
    const LocomotionData* try_get_locomotion(uint32_t id) const { return try_get_ptr(locomotion_data, id); }
    ClothingData* try_get_clothing(uint32_t id) { return try_get_ptr(clothing_data, id); }
    const ClothingData* try_get_clothing(uint32_t id) const { return try_get_ptr(clothing_data, id); }
    InventoryData* try_get_inventory(uint32_t id) { return try_get_ptr(inventory_data, id); }
    const InventoryData* try_get_inventory(uint32_t id) const { return try_get_ptr(inventory_data, id); }
    EffectsData* try_get_effects(uint32_t id) { return try_get_ptr(effects_data, id); }
    const EffectsData* try_get_effects(uint32_t id) const { return try_get_ptr(effects_data, id); }
    AIData* try_get_ai(uint32_t id) { return try_get_ptr(ai_data, id); }
    const AIData* try_get_ai(uint32_t id) const { return try_get_ptr(ai_data, id); }
    String* try_get_combat_style(uint32_t id) { return try_get_ptr(combat_style, id); }
    const String* try_get_combat_style(uint32_t id) const { return try_get_ptr(combat_style, id); }
    String* try_get_gender(uint32_t id) { return try_get_ptr(gender, id); }
    const String* try_get_gender(uint32_t id) const { return try_get_ptr(gender, id); }
    String* try_get_name(uint32_t id) { return try_get_ptr(entity_name, id); }
    const String* try_get_name(uint32_t id) const { return try_get_ptr(entity_name, id); }
    SocialMemoryData* try_get_social_memory(uint32_t id) { return try_get_ptr(social_data, id); }
    const SocialMemoryData* try_get_social_memory(uint32_t id) const { return try_get_ptr(social_data, id); }
    SocialProfileData* try_get_social_profile(uint32_t id) { return try_get_ptr(social_profiles, id); }
    const SocialProfileData* try_get_social_profile(uint32_t id) const { return try_get_ptr(social_profiles, id); }
    AllegianceData* try_get_allegiance(uint32_t id) { return try_get_ptr(allegiance_data, id); }
    const AllegianceData* try_get_allegiance(uint32_t id) const { return try_get_ptr(allegiance_data, id); }
    VendorState* try_get_vendor_state(uint32_t id) { return try_get_ptr(vendor_state, id); }
    const VendorState* try_get_vendor_state(uint32_t id) const { return try_get_ptr(vendor_state, id); }

    InventoryData& ensure_inventory(uint32_t id);
    EffectsData& ensure_effects(uint32_t id);
    SocialMemoryData& ensure_social_memory(uint32_t id);
    VendorState& ensure_vendor_state(uint32_t id);

    bool has_inventory(uint32_t id) const;
    bool is_alive(uint32_t id) const;
    bool is_player(uint32_t id) const;
    bool is_actor(uint32_t id) const;
    bool is_npc(uint32_t id) const;
    bool is_combatant(uint32_t id) const;
    bool is_sapient(uint32_t id) const;
    bool has_core_components(uint32_t id) const;
    bool is_schedulable_actor(uint32_t id) const;
    bool validate_player(uint32_t id) const;
    bool validate_npc_actor(uint32_t id) const;
    bool validate_combatant(uint32_t id) const;

    int get_inventory_item_amount(uint32_t id, const String& item_id) const;

    void init_relationship(uint32_t id);
    void init_social_profile(uint32_t id, const String& job = "drifter", const String& dialogue_id = "");
    bool has_relationship(uint32_t id) const;
    int get_friendship(uint32_t id) const;
    int get_romance(uint32_t id) const;
    void set_friendship(uint32_t id, int value);
    void set_romance(uint32_t id, int value);

    int get_social_cooldown(uint32_t id) const;
    void set_social_cooldown(uint32_t id, int turn);
    String get_social_state_json(uint32_t id) const;
    void set_social_state_json(uint32_t id, const String& json);
    void clear_social_state(uint32_t id);

    Dictionary get_anatomy(uint32_t id) const;
    Dictionary get_clothing(uint32_t id) const;
    Dictionary get_equipment(uint32_t id) const;
    Dictionary get_inventory(uint32_t id) const;
    Dictionary get_health(uint32_t id) const;
    Dictionary get_stamina(uint32_t id) const;
    Dictionary get_effects(uint32_t id) const;
    Dictionary get_social_profile(uint32_t id) const;
    float get_inventory_weight(uint32_t id) const;
    float get_armor_rating(uint32_t id) const;
    String get_anatomy_part_name(uint32_t id, int part_index) const;

    bool add_inventory_item(uint32_t id, const String& item_id, int amount);
    bool remove_inventory_item(uint32_t id, const String& item_id, int amount);
    bool equip_clothing(uint32_t id, int part_index, const String& item_id, const String& layer);
    bool unequip_clothing(uint32_t id, const String& item_id);
    bool equip_clothing_by_string(uint32_t id, const String& item_id);
    bool unequip_clothing_by_string(uint32_t id, const String& item_id);
    bool wield_weapon(uint32_t id, const String& slot_name, const String& item_id);
    bool unwield_weapon(uint32_t id, const String& slot_name);
    bool wield_weapon_by_string(uint32_t id, const String& item_id);

    Dictionary serialize() const;
    void deserialize(const Dictionary& data);

    Dictionary serialize_entity(uint32_t id) const;
    uint32_t deserialize_entity(const Dictionary& data);
};

}

#endif // ! SPACETRAVELLER_ENTITY_LEDGER_H
