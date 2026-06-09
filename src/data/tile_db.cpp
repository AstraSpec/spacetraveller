#include "tile_db.h"
#include "core/id_registry.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

template<> TileDb* DataBase<TileInfo, TileDb>::singleton = nullptr;

void TileDb::_bind_methods() {
    ClassDB::bind_static_method("TileDb", D_METHOD("get_singleton"), &TileDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &TileDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_atlas_coords", "id"), &TileDb::get_atlas_coords);
    ClassDB::bind_method(D_METHOD("is_solid", "id"), &TileDb::is_solid);
    ClassDB::bind_method(D_METHOD("has_tag", "id", "tag"), &TileDb::has_tag);
    ClassDB::bind_method(D_METHOD("get_tile_name", "id"), &TileDb::get_tile_name);
    ClassDB::bind_method(D_METHOD("get_smash_loot_table", "id"), &TileDb::get_smash_loot_table);
    ClassDB::bind_method(D_METHOD("get_ids"), &TileDb::get_ids);
}

TileDb::TileDb() {
}

TileDb::~TileDb() {
}

TileInfo TileDb::_parse_row(const Dictionary &p_data) {
    TileInfo info;

    info.name = p_data.get("name", "");

    Variant atlas_data = p_data.get("atlas", Array());
    if (atlas_data.get_type() == Variant::ARRAY) {
        Array arr = atlas_data;
        if (arr.size() > 0 && arr[0].get_type() == Variant::ARRAY) {
            // Multiple atlases for tile
            for (int i = 0; i < arr.size(); i++) {
                info.atlas_variants.push_back(variant_to_vector2i(arr[i]));
            }
        } else {
            // Single variation
            info.atlas_variants.push_back(variant_to_vector2i(arr));
        }
    }
    
    info.solid = p_data.get("solid", false);
    info.tags = _parse_tags(p_data.get("tags", Array()));
    TagRegistry* tag_reg = TagRegistry::get_singleton();
    if (tag_reg) {
        uint16_t hidden_items_tag = tag_reg->get_tag_id("HIDDEN_ITEMS");
        info.hides_items = TagRegistry::has_tag(hidden_items_tag, info.tags);
    }
    String smash_loot_table = String(p_data.get("smash_loot_table", ""));
    if (!smash_loot_table.is_empty() && IdRegistry::get_singleton()) {
        info.smash_loot_table = IdRegistry::get_singleton()->register_string(smash_loot_table);
    }
    
    if (IdRegistry::get_singleton()) {
        uint16_t id = IdRegistry::get_singleton()->register_string(p_data["id"]);
        if (id >= fast_cache.size()) {
            fast_cache.resize(id + 1);
        }
        fast_cache[id] = info;
    }
    return info;
}

const TileInfo* TileDb::get_tile_info(const String &p_id) const {
    return get_info(p_id);
}

const TileInfo* TileDb::get_tile_info(uint16_t p_id) const {
    if (p_id < fast_cache.size()) {
        return &fast_cache[p_id];
    }
    return nullptr;
}

Vector2i TileDb::get_atlas_coords(const String &p_id) const {
    const TileInfo* info = get_tile_info(p_id);
    if (info && !info->atlas_variants.empty()) return info->atlas_variants[0];
    return Vector2i(-1, -1);
}

bool TileDb::is_solid(const String &p_id) const {
    const TileInfo* info = get_tile_info(p_id);
    if (info) return info->solid;
    return false;
}

bool TileDb::has_tag(const String &p_id, const String &p_tag) const {
    const TileInfo* info = get_tile_info(p_id);
    if (!info) return false;
    
    TagRegistry *reg = TagRegistry::get_singleton();
    if (!reg) return false;
    
    uint16_t tag_id = reg->get_tag_id(p_tag);
    return TagRegistry::has_tag(tag_id, info->tags);
}

bool TileDb::hides_items_at(uint16_t p_id) const {
    const TileInfo* info = get_tile_info(p_id);
    return info ? info->hides_items : false;
}

String TileDb::get_tile_name(const String &p_id) const {
    const TileInfo* info = get_tile_info(p_id);
    return (info && !info->name.is_empty()) ? info->name : p_id;
}

String TileDb::get_smash_loot_table(const String &p_id) const {
    const TileInfo* info = get_tile_info(p_id);
    IdRegistry* reg = IdRegistry::get_singleton();
    return (info && reg && info->smash_loot_table != 0) ? reg->get_string(info->smash_loot_table) : String();
}

}
