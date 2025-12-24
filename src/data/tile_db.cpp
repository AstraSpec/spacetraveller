#include "tile_db.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

TileDb *TileDb::singleton = nullptr;

void TileDb::_bind_methods() {
    ClassDB::bind_method(D_METHOD("initialize_data"), &TileDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_atlas_coords", "id"), &TileDb::get_atlas_coords);
    ClassDB::bind_method(D_METHOD("is_solid", "id"), &TileDb::is_solid);
    ClassDB::bind_method(D_METHOD("get_full_data"), &TileDb::get_full_data);
}

void TileDb::create_singleton() {
    singleton = memnew(TileDb);
}

void TileDb::delete_singleton() {
    if (singleton) {
        memdelete(singleton);
        singleton = nullptr;
    }
}

TileDb::TileDb() {
}

TileDb::~TileDb() {
}

void TileDb::initialize_data() {
    db_helper.load_directory("res://data/tiles");
    
    // Populate the fast C++ cache
    cache.clear();
    for (const auto& pair : db_helper.get_all_rows()) {
        const String& id = pair.first;
        const Dictionary& d = pair.second;
        
        TileInfo info;
        info.atlas = db_helper.variant_to_vector2i(d.get("atlas", Array()));
        info.solid = d.get("solid", false);
        cache[id] = info;
    }
}

const TileInfo* TileDb::get_tile_info(const String &p_id) const {
    auto it = cache.find(p_id);
    if (it != cache.end()) {
        return &it->second;
    }
    return nullptr;
}

Vector2i TileDb::get_atlas_coords(const String &p_id) const {
    const TileInfo* info = get_tile_info(p_id);
    if (info) return info->atlas;
    return Vector2i(-1, -1);
}

bool TileDb::is_solid(const String &p_id) const {
    const TileInfo* info = get_tile_info(p_id);
    if (info) return info->solid;
    return false;
}

}
