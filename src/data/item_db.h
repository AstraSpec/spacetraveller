#ifndef SPACETRAVELLER_ITEM_DB_H
#define SPACETRAVELLER_ITEM_DB_H

#include <godot_cpp/classes/object.hpp>
#include "database.h"
#include "light_emission.h"

namespace godot {

struct ClothingSlotInfo {
    String part;
    String layer = "middle";
    float coverage = 1.0f;
    float bash = 0.0f;
    float cut = 0.0f;
    float pierce = 0.0f;
    float bash_transmission = 0.0f;
};

struct ItemInfo {
    String name;
    String description;
    Vector2i atlas;
    float weight = 0.0f;
    int price = 0;
    std::vector<uint16_t> tags;
    std::vector<ClothingSlotInfo> clothing_slots;
    Dictionary weapon_data;
    String type = "misc";
    LightEmissionInfo light;
};

class ItemDb : public Object, public DataBase<ItemInfo, ItemDb> {
    GDCLASS(ItemDb, Object)

protected:
    static void _bind_methods();
    virtual ItemInfo _parse_row(const Dictionary &p_data) override;

    std::vector<ItemInfo> fast_cache;

public:
    ItemDb();
    ~ItemDb();

    void initialize_data() { fast_cache.clear(); DataBase::initialize_data("res://data/items"); }
    Array get_ids() const { return DataBase::get_ids(); }

    // Fast C++ access
    const ItemInfo* get_item_info(const String &p_id) const;
    const ItemInfo* get_item_info(uint16_t p_id) const;

    // GDScript/Standard access
    Vector2i get_atlas_coords(const String &p_id) const;
    String get_item_name(const String &p_id) const;
    String get_item_description(const String &p_id) const;
    int get_item_price(const String &p_id) const;
    Dictionary get_item_modifiers(const String &p_id) const;
    bool has_tag(const String &p_id, const String &p_tag) const;
    Dictionary get_clothing_data(const String &p_id) const;
    Array get_clothing_slots(const String &p_id) const;
    const std::vector<ClothingSlotInfo>* get_clothing_slots_info(const String &p_id) const;
    Dictionary get_weapon_data(const String &p_id) const;
    String get_item_type(const String &p_id) const;
};

}

#endif // ! SPACETRAVELLER_ITEM_DB_H
