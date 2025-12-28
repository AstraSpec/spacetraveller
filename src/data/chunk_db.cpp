#include "chunk_db.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

template<> ChunkDb* DataBase<ChunkInfo, ChunkDb>::singleton = nullptr;

void ChunkDb::_bind_methods() {
    ClassDB::bind_static_method("ChunkDb", D_METHOD("get_singleton"), &ChunkDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &ChunkDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_atlas_coords", "id"), &ChunkDb::get_atlas_coords);
    ClassDB::bind_method(D_METHOD("get_ids"), &ChunkDb::get_ids);
}

ChunkDb::ChunkDb() {
}

ChunkDb::~ChunkDb() {
}

ChunkInfo ChunkDb::_parse_row(const Dictionary &p_data) {
        ChunkInfo info;
    info.atlas = variant_to_vector2i(p_data.get("atlas", Array()));
    return info;
}

const ChunkInfo* ChunkDb::get_chunk_info(const String &p_id) const {
    return get_info(p_id);
}

Vector2i ChunkDb::get_atlas_coords(const String &p_id) const {
    const ChunkInfo* info = get_chunk_info(p_id);
    if (info) return info->atlas;
    return Vector2i(-1, -1);
}

}
