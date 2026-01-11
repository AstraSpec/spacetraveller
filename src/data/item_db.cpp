#include "item_db.h"
#include "id_registry.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

template<> ItemDb* DataBase<ItemInfo, ItemDb>::singleton = nullptr;

void ItemDb::_bind_methods() {
    ClassDB::bind_static_method("ItemDb", D_METHOD("get_singleton"), &ItemDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &ItemDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_atlas_coords", "id"), &ItemDb::get_atlas_coords);
    ClassDB::bind_method(D_METHOD("get_ids"), &ItemDb::get_ids);
}

ItemDb::ItemDb() {
}

ItemDb::~ItemDb() {
}

ItemInfo ItemDb::_parse_row(const Dictionary &p_data) {
    ItemInfo info;
    info.atlas = variant_to_vector2i(p_data.get("atlas", Array()));
    info.weight = p_data.get("weight", 0.0f);
    info.volume = p_data.get("volume", 0.0f);
    
    if (IdRegistry::get_singleton()) {
        uint16_t id = IdRegistry::get_singleton()->register_string(p_data["id"]);
        if (id >= fast_cache.size()) {
            fast_cache.resize(id + 1);
        }
        fast_cache[id] = info;
    }
    return info;
}

const ItemInfo* ItemDb::get_item_info(const String &p_id) const {
    return get_info(p_id);
}

const ItemInfo* ItemDb::get_item_info(uint16_t p_id) const {
    if (p_id < fast_cache.size()) {
        return &fast_cache[p_id];
    }
    return nullptr;
}

Vector2i ItemDb::get_atlas_coords(const String &p_id) const {
    const ItemInfo* info = get_item_info(p_id);
    if (info) return info->atlas;
    return Vector2i(-1, -1);
}

}
