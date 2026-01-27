#ifndef SPACETRAVELLER_ITEM_DB_H
#define SPACETRAVELLER_ITEM_DB_H

#include <godot_cpp/classes/object.hpp>
#include "database.h"

namespace godot {

struct ItemInfo {
    String name;
    String description;
    Vector2i atlas;
    float weight = 0.0f;
    float volume = 0.0f;
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

    void initialize_data() { DataBase::initialize_data("res://data/items"); }
    Array get_ids() const { return DataBase::get_ids(); }

    // Fast C++ access
    const ItemInfo* get_item_info(const String &p_id) const;
    const ItemInfo* get_item_info(uint16_t p_id) const;

    // GDScript/Standard access
    Vector2i get_atlas_coords(const String &p_id) const;
    String get_item_name(const String &p_id) const;
    String get_item_description(const String &p_id) const;
    Dictionary get_item_modifiers(const String &p_id) const;
};

}

#endif // ! SPACETRAVELLER_ITEM_DB_H
