#include "tile_db.h"
#include "id_registry.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

template<> TileDb* DataBase<TileInfo, TileDb>::singleton = nullptr;

void TileDb::_bind_methods() {
    ClassDB::bind_static_method("TileDb", D_METHOD("get_singleton"), &TileDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &TileDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_atlas_coords", "id"), &TileDb::get_atlas_coords);
    ClassDB::bind_method(D_METHOD("is_solid", "id"), &TileDb::is_solid);
    ClassDB::bind_method(D_METHOD("get_ids"), &TileDb::get_ids);
}

TileDb::TileDb() {
}

TileDb::~TileDb() {
}

TileInfo TileDb::_parse_row(const Dictionary &p_data) {
    TileInfo info;
    info.atlas = variant_to_vector2i(p_data.get("atlas", Array()));
    info.solid = p_data.get("solid", false);
    
    if (IdRegistry::get_singleton()) {
        IdRegistry::get_singleton()->register_string(p_data["id"]);
    }
    return info;
}

const TileInfo* TileDb::get_tile_info(const String &p_id) const {
    return get_info(p_id);
}

const TileInfo* TileDb::get_tile_info(uint16_t p_id) const {
    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (id_reg) {
        return get_info(id_reg->get_string(p_id));
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
