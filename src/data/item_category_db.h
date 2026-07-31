#ifndef SPACETRAVELLER_ITEM_CATEGORY_DB_H
#define SPACETRAVELLER_ITEM_CATEGORY_DB_H

#include "database.h"
#include <godot_cpp/classes/object.hpp>

namespace godot {

struct ItemCategoryInfo {
    String name;
    int sort_rank = 100;
};

class ItemCategoryDb :
    public Object,
    public DataBase<ItemCategoryInfo, ItemCategoryDb> {
    GDCLASS(ItemCategoryDb, Object)

protected:
    static void _bind_methods();
    ItemCategoryInfo _parse_row(const Dictionary& p_data) override;

public:
    ItemCategoryDb();
    ~ItemCategoryDb();

    void initialize_data() {
        DataBase::initialize_data("res://data/item_categories");
    }
    Array get_ids() const { return DataBase::get_ids(); }
    const ItemCategoryInfo* get_category_info(const String& p_id) const;
    String get_display_name(const String& p_id) const;
    int get_sort_rank(const String& p_id) const;
};

}

#endif
