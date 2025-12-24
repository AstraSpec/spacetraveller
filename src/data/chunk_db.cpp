#include "chunk_db.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

ChunkDb *ChunkDb::singleton = nullptr;

void ChunkDb::_bind_methods() {
    ClassDB::bind_method(D_METHOD("initialize_data"), &ChunkDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_atlas_coords", "id"), &ChunkDb::get_atlas_coords);
    ClassDB::bind_method(D_METHOD("get_full_data"), &ChunkDb::get_full_data);
}

void ChunkDb::create_singleton() {
    singleton = memnew(ChunkDb);
}

void ChunkDb::delete_singleton() {
    if (singleton) {
        memdelete(singleton);
        singleton = nullptr;
    }
}

ChunkDb::ChunkDb() {
}

ChunkDb::~ChunkDb() {
}

void ChunkDb::initialize_data() {
    db_helper.load_directory("res://data/chunks");
    
    // Populate fast C++ cache
    cache.clear();
    for (const auto& pair : db_helper.get_all_rows()) {
        const String& id = pair.first;
        const Dictionary& d = pair.second;
        
        ChunkInfo info;
        info.atlas = db_helper.variant_to_vector2i(d.get("atlas", Array()));
        cache[id] = info;
    }
}

const ChunkInfo* ChunkDb::get_chunk_info(const String &p_id) const {
    auto it = cache.find(p_id);
    if (it != cache.end()) {
        return &it->second;
    }
    return nullptr;
}

Vector2i ChunkDb::get_atlas_coords(const String &p_id) const {
    const ChunkInfo* info = get_chunk_info(p_id);
    if (info) return info->atlas;
    return Vector2i(-1, -1);
}

}
