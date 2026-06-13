#include "chunk_db.h"
#include "core/id_registry.h"
#include <godot_cpp/core/class_db.hpp>
#include <algorithm>

namespace godot {

template<> ChunkDb* DataBase<ChunkInfo, ChunkDb>::singleton = nullptr;

void ChunkDb::_bind_methods() {
    ClassDB::bind_static_method("ChunkDb", D_METHOD("get_singleton"), &ChunkDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &ChunkDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_atlas_coords", "id"), &ChunkDb::get_atlas_coords);
    ClassDB::bind_method(D_METHOD("has_tag", "id", "tag"), &ChunkDb::has_tag_gd);
    ClassDB::bind_method(D_METHOD("get_ids"), &ChunkDb::get_ids);
}

ChunkDb::ChunkDb() {
}

ChunkDb::~ChunkDb() {
}

void ChunkDb::initialize_data() {
    fast_cache.clear();
    city_spawn_chunks.clear();
    city_spawn_total_weight = 0;
    DataBase::initialize_data("res://data/chunks");

    for (uint16_t id = 0; id < fast_cache.size(); id++) {
        const ChunkInfo& info = fast_cache[id];
        if (info.city_spawn_weight <= 0) {
            continue;
        }
        city_spawn_chunks.push_back({ id, info.city_spawn_weight });
        city_spawn_total_weight += info.city_spawn_weight;
    }

    std::sort(city_spawn_chunks.begin(), city_spawn_chunks.end(), [](const CityChunkSpawnInfo& a, const CityChunkSpawnInfo& b) {
        return a.id < b.id;
    });
}

ChunkInfo ChunkDb::_parse_row(const Dictionary &p_data) {
    ChunkInfo info;
    info.atlas = variant_to_vector2i(p_data.get("atlas", Array()));
    info.tags = _parse_tags(p_data.get("tags", Array()));
    info.city_spawn_weight = static_cast<int>(p_data.get("city_spawn_weight", Variant(0)));
    info.structure_type = String(p_data.get("structure_type", ""));

    Variant feature_spawns_var = p_data.get("feature_spawns", Array());
    if (feature_spawns_var.get_type() == Variant::ARRAY) {
        Array feature_spawns = feature_spawns_var;
        for (int i = 0; i < feature_spawns.size(); i++) {
            if (feature_spawns[i].get_type() != Variant::DICTIONARY) continue;

            Dictionary spawn_data = feature_spawns[i];
            ChunkFeatureSpawnInfo spawn;
            spawn.pool = String(spawn_data.get("pool", ""));
            spawn.placement = String(spawn_data.get("placement", ""));
            spawn.chance = static_cast<float>(static_cast<double>(spawn_data.get("chance", 0.0)));
            spawn.candidates = static_cast<int>(spawn_data.get("candidates", Variant(1)));

            if (spawn.chance < 0.0f) spawn.chance = 0.0f;
            if (spawn.chance > 1.0f) spawn.chance = 1.0f;
            if (spawn.candidates < 0) spawn.candidates = 0;
            if (spawn.pool.is_empty() || spawn.placement.is_empty() || spawn.chance <= 0.0f || spawn.candidates <= 0) continue;

            info.feature_spawns.push_back(spawn);
        }
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

const ChunkInfo* ChunkDb::get_chunk_info(const String &p_id) const {
    return get_info(p_id);
}

const ChunkInfo* ChunkDb::get_chunk_info(uint16_t p_id) const {
    if (p_id < fast_cache.size()) {
        return &fast_cache[p_id];
    }
    return nullptr;
}

bool ChunkDb::has_tag(const String &p_id, const String &p_tag) const {
    const ChunkInfo* info = get_chunk_info(p_id);
    if (!info) return false;

    TagRegistry *reg = TagRegistry::get_singleton();
    if (!reg) return false;

    uint16_t tag_id = reg->get_tag_id(p_tag);
    return TagRegistry::has_tag(tag_id, info->tags);
}

bool ChunkDb::has_tag(uint16_t p_id, uint16_t p_tag_id) const {
    const ChunkInfo* info = get_chunk_info(p_id);
    if (!info) return false;
    return TagRegistry::has_tag(p_tag_id, info->tags);
}

Vector2i ChunkDb::get_atlas_coords(const String &p_id) const {
    const ChunkInfo* info = get_chunk_info(p_id);
    if (info) return info->atlas;
    return Vector2i(-1, -1);
}

}
