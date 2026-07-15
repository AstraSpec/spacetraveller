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
    ClassDB::bind_method(D_METHOD("is_city_structure_type", "structure_type"), &ChunkDb::is_city_structure_type);
    ClassDB::bind_method(D_METHOD("get_ids"), &ChunkDb::get_ids);
}

ChunkDb::ChunkDb() {
}

ChunkDb::~ChunkDb() {
}

void ChunkDb::initialize_data() {
    fast_cache.clear();
    city_spawn_chunks.clear();
    wilderness_spawn_chunks.clear();
    city_spawn_total_weight = 0;
    DataBase::initialize_data("res://data/chunks");

    for (uint16_t id = 0; id < fast_cache.size(); id++) {
        const ChunkInfo& info = fast_cache[id];
        if (info.city_spawn_weight > 0) {
            city_spawn_chunks.push_back({
                id,
                info.city_spawn_weight,
                info.city_zone_min,
                info.city_zone_max,
                info.city_min_count,
                info.city_max_count
            });
            city_spawn_total_weight += info.city_spawn_weight;
        }

        if (info.wilderness_spawn_chance > 0.0f) {
            wilderness_spawn_chunks.push_back({
                id,
                static_cast<int>(info.wilderness_spawn_chance * 1000000.0f),
                0.0f,
                1.0f,
                0,
                -1,
                info.wilderness_footprint
            });
        }
    }

    std::sort(city_spawn_chunks.begin(), city_spawn_chunks.end(), [](const CityChunkSpawnInfo& a, const CityChunkSpawnInfo& b) {
        return a.id < b.id;
    });
    std::sort(wilderness_spawn_chunks.begin(), wilderness_spawn_chunks.end(), [](const CityChunkSpawnInfo& a, const CityChunkSpawnInfo& b) {
        return a.id < b.id;
    });
}

ChunkInfo ChunkDb::_parse_row(const Dictionary &p_data) {
    ChunkInfo info;
    info.atlas = variant_to_vector2i(p_data.get("atlas", Array()));
    info.tags = _parse_tags(p_data.get("tags", Array()));
    info.city_spawn_weight = static_cast<int>(p_data.get("city_spawn_weight", Variant(0)));
    info.city_zone_min = CLAMP(static_cast<float>(p_data.get("city_zone_min", 0.0)), 0.0f, 1.0f);
    info.city_zone_max = CLAMP(static_cast<float>(p_data.get("city_zone_max", 1.0)), 0.0f, 1.0f);
    if (info.city_zone_min > info.city_zone_max) {
        info.city_zone_min = 0.0f;
        info.city_zone_max = 1.0f;
    }

    info.city_min_count = std::max(0, static_cast<int>(p_data.get("city_min_count", 0)));
    info.city_max_count = static_cast<int>(p_data.get("city_max_count", -1));
    
    info.wilderness_spawn_chance = static_cast<float>(static_cast<double>(p_data.get("wilderness_spawn_chance", 0.0)));
    if (info.wilderness_spawn_chance < 0.0f) info.wilderness_spawn_chance = 0.0f;
    if (info.wilderness_spawn_chance > 1.0f) info.wilderness_spawn_chance = 1.0f;
    info.wilderness_footprint = variant_to_vector2i(
        p_data.get("wilderness_footprint", Array()),
        Vector2i(1, 1)
    );
    if (info.wilderness_footprint.x <= 0 || info.wilderness_footprint.y <= 0) {
        info.wilderness_footprint = Vector2i(1, 1);
    }
    
    info.structure_type = String(p_data.get("structure_type", ""));
    info.dungeon_type = String(p_data.get("dungeon_type", ""));
    info.tile_group = String(p_data.get("tile_group", ""));
    
    info.ambient_entity_group = String(p_data.get("ambient_entity_group", ""));
    info.ambient_entity_chance = static_cast<float>(static_cast<double>(p_data.get("ambient_entity_chance", 0.0)));
    if (info.ambient_entity_chance < 0.0f) info.ambient_entity_chance = 0.0f;
    if (info.ambient_entity_chance > 1.0f) info.ambient_entity_chance = 1.0f;
    String ambient_loot_table = String(p_data.get("ambient_loot_table", ""));
    
    if (!ambient_loot_table.is_empty() && IdRegistry::get_singleton()) {
        info.ambient_loot_table = IdRegistry::get_singleton()->register_string(ambient_loot_table);
    }
    
    info.ambient_loot_chance = static_cast<float>(static_cast<double>(p_data.get("ambient_loot_chance", 0.0)));
    if (info.ambient_loot_chance < 0.0f) info.ambient_loot_chance = 0.0f;
    if (info.ambient_loot_chance > 1.0f) info.ambient_loot_chance = 1.0f;

    Variant feature_spawns_var = p_data.get("feature_spawns", Array());
    if (feature_spawns_var.get_type() == Variant::ARRAY) {
        Array feature_spawns = feature_spawns_var;
        for (int i = 0; i < feature_spawns.size(); i++) {
            if (feature_spawns[i].get_type() != Variant::DICTIONARY) continue;

            Dictionary spawn_data = feature_spawns[i];
            ChunkFeatureSpawnInfo spawn;
            spawn.pool = String(spawn_data.get("pool", ""));
            spawn.placement = String(spawn_data.get("placement", ""));
            spawn.scope = String(spawn_data.get("scope", "chunk"));
            if (spawn.scope.is_empty()) spawn.scope = "chunk";
            spawn.rotation_mode = String(spawn_data.get("rotation", "fixed"));
            spawn.chance = static_cast<float>(static_cast<double>(spawn_data.get("chance", 0.0)));
            spawn.candidates = static_cast<int>(spawn_data.get("candidates", Variant(1)));
            spawn.unique = static_cast<bool>(spawn_data.get("unique", false));

            Variant areas_var = spawn_data.get("areas", Array());
            if (areas_var.get_type() == Variant::ARRAY) {
                Array areas = areas_var;
                for (int area_idx = 0; area_idx < areas.size(); area_idx++) {
                    if (areas[area_idx].get_type() != Variant::DICTIONARY) continue;

                    Dictionary area_data = areas[area_idx];
                    ChunkFeatureAreaInfo area;
                    area.id = String(area_data.get("id", ""));
                    area.facing = String(area_data.get("facing", ""));
                    area.origin = variant_to_vector2i(area_data.get("origin", Array()));
                    area.size = variant_to_vector2i(area_data.get("size", Array()));
                    if (area.origin.x < 0 || area.origin.y < 0) continue;
                    if (area.size.x <= 0 || area.size.y <= 0) continue;
                    spawn.areas.push_back(area);
                }
            }

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

bool ChunkDb::is_city_structure_type(const String &p_structure_type) const {
    String structure_type = p_structure_type.strip_edges();
    if (structure_type.is_empty()) return false;

    for (const ChunkInfo& info : fast_cache) {
        if (info.city_spawn_weight > 0 && info.structure_type == structure_type) {
            return true;
        }
    }
    return false;
}

}
