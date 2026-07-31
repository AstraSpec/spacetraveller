#include "item_category_db.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

template<>
ItemCategoryDb*
DataBase<ItemCategoryInfo, ItemCategoryDb>::singleton = nullptr;

void ItemCategoryDb::_bind_methods() {
    ClassDB::bind_static_method(
        "ItemCategoryDb",
        D_METHOD("get_singleton"),
        &ItemCategoryDb::get_singleton);
    ClassDB::bind_method(
        D_METHOD("initialize_data"),
        &ItemCategoryDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_ids"), &ItemCategoryDb::get_ids);
    ClassDB::bind_method(
        D_METHOD("get_display_name", "id"),
        &ItemCategoryDb::get_display_name);
    ClassDB::bind_method(
        D_METHOD("get_sort_rank", "id"),
        &ItemCategoryDb::get_sort_rank);
}

ItemCategoryDb::ItemCategoryDb() {}
ItemCategoryDb::~ItemCategoryDb() {}

ItemCategoryInfo ItemCategoryDb::_parse_row(const Dictionary& p_data) {
    ItemCategoryInfo info;
    info.name = String(
        p_data.get("name", p_data.get("id", ""))
    ).strip_edges();
    info.sort_rank = static_cast<int>(p_data.get("sort_rank", 100));
    return info;
}

const ItemCategoryInfo* ItemCategoryDb::get_category_info(
    const String& p_id
) const {
    return get_info(p_id);
}

String ItemCategoryDb::get_display_name(const String& p_id) const {
    const ItemCategoryInfo* info = get_category_info(p_id);
    return info ? info->name : "";
}

int ItemCategoryDb::get_sort_rank(const String& p_id) const {
    const ItemCategoryInfo* info = get_category_info(p_id);
    return info ? info->sort_rank : 100;
}

}
