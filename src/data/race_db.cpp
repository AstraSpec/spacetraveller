#include "race_db.h"
#include "core/id_registry.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

template<> RaceDb* DataBase<RaceInfo, RaceDb>::singleton = nullptr;

void RaceDb::_bind_methods() {
    ClassDB::bind_static_method("RaceDb", D_METHOD("get_singleton"), &RaceDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &RaceDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_ids"), &RaceDb::get_ids);
}

RaceDb::RaceDb() {}
RaceDb::~RaceDb() {}

RaceInfo RaceDb::_parse_row(const Dictionary &p_data) {
    RaceInfo info;
    info.name = p_data.get("name", "");
    info.atlas = variant_to_vector2i(p_data.get("atlas", Array()));
    info.perception_tier = p_data.get("perception_tier", "raycast");

    Array parts = p_data.get("parts", Array());
    for (int i = 0; i < parts.size(); i++) {
        Dictionary p = parts[i];
        RacePartDefinition def;
        def.part_id = p.get("id", "");
        def.parent_part_id = p.get("parent", "");
        def.count = p.get("count", 1);
        info.parts.push_back(def);
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

const RaceInfo* RaceDb::get_race_info(const String &p_id) const {
    return get_info(p_id);
}

const RaceInfo* RaceDb::get_race_info(uint16_t p_id) const {
    if (p_id < fast_cache.size()) {
        return &fast_cache[p_id];
    }
    return nullptr;
}

Vector2i RaceDb::get_atlas_coords(const String &p_id) const {
    const RaceInfo* info = get_race_info(p_id);
    if (info) return info->atlas;
    return Vector2i(-1, -1);
}

Vector2i RaceDb::get_atlas_coords(uint16_t p_id) const {
    const RaceInfo* info = get_race_info(p_id);
    if (info) return info->atlas;
    return Vector2i(-1, -1);
}

}
