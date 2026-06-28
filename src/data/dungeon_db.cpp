#include "dungeon_db.h"
#include "core/id_registry.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

template<> DungeonDb* DataBase<DungeonInfo, DungeonDb>::singleton = nullptr;

void DungeonDb::_bind_methods() {
    ClassDB::bind_static_method("DungeonDb", D_METHOD("get_singleton"), &DungeonDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &DungeonDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_ids"), &DungeonDb::get_ids);
}

DungeonDb::DungeonDb() {}
DungeonDb::~DungeonDb() {}

static float clamp_unit_float(float p_value) {
    if (p_value < 0.0f) return 0.0f;
    if (p_value > 1.0f) return 1.0f;
    return p_value;
}

static void normalize_range(int& r_min, int& r_max, int p_floor = 0) {
    if (r_min < p_floor) r_min = p_floor;
    if (r_max < r_min) r_max = r_min;
}

DungeonInfo DungeonDb::_parse_row(const Dictionary &p_data) {
    DungeonInfo info;
    info.id = String(p_data.get("id", ""));
    info.generator = String(p_data.get("generator", "room_graph"));
    info.end_structure_type = String(p_data.get("end_structure_type", ""));
    IdRegistry* id_reg = IdRegistry::get_singleton();
    info.ambient_entity_group = String(p_data.get("ambient_entity_group", ""));
    info.ambient_entity_chance = static_cast<float>(static_cast<double>(p_data.get("ambient_entity_chance", 0.0)));
    if (info.ambient_entity_chance < 0.0f) info.ambient_entity_chance = 0.0f;
    if (info.ambient_entity_chance > 1.0f) info.ambient_entity_chance = 1.0f;
    String ambient_loot_table = String(p_data.get("ambient_loot_table", ""));
    if (id_reg && !ambient_loot_table.is_empty()) {
        info.ambient_loot_table = id_reg->register_string(ambient_loot_table);
    }
    info.ambient_loot_chance = static_cast<float>(static_cast<double>(p_data.get("ambient_loot_chance", 0.0)));
    if (info.ambient_loot_chance < 0.0f) info.ambient_loot_chance = 0.0f;
    if (info.ambient_loot_chance > 1.0f) info.ambient_loot_chance = 1.0f;
    String floor_tile = String(p_data.get("floor_tile", ""));
    String wall_tile = String(p_data.get("wall_tile", ""));
    if (id_reg && !floor_tile.is_empty()) {
        info.floor_tile = id_reg->register_string(floor_tile);
    }
    if (id_reg && !wall_tile.is_empty()) {
        info.wall_tile = id_reg->register_string(wall_tile);
    }
    info.start_z = static_cast<int>(p_data.get("start_z", Variant(-1)));
    info.depth_min = static_cast<int>(p_data.get("depth_min", Variant(1)));
    info.depth_max = static_cast<int>(p_data.get("depth_max", Variant(info.depth_min)));
    info.radius_chunks = static_cast<int>(p_data.get("radius_chunks", Variant(4)));
    info.room_count_min = static_cast<int>(p_data.get("room_count_min", Variant(12)));
    info.room_count_max = static_cast<int>(p_data.get("room_count_max", Variant(info.room_count_min)));
    info.corridor_width = static_cast<int>(p_data.get("corridor_width", Variant(1)));

    if (info.depth_min < 1) info.depth_min = 1;
    if (info.depth_max < info.depth_min) info.depth_max = info.depth_min;
    if (info.radius_chunks < 1) info.radius_chunks = 1;
    if (info.room_count_min < 1) info.room_count_min = 1;
    if (info.room_count_max < info.room_count_min) info.room_count_max = info.room_count_min;
    if (info.corridor_width < 1) info.corridor_width = 1;

    Variant dynamic_features_var = p_data.get("dynamic_features", Array());
    if (dynamic_features_var.get_type() == Variant::ARRAY) {
        Array dynamic_features = dynamic_features_var;
        for (int i = 0; i < dynamic_features.size(); i++) {
            if (dynamic_features[i].get_type() != Variant::DICTIONARY) continue;

            Dictionary feature_data = dynamic_features[i];
            DungeonDynamicFeatureInfo feature;
            feature.type = String(feature_data.get("type", ""));
            feature.placement = String(feature_data.get("placement", "room_random"));
            feature.chance = clamp_unit_float(static_cast<float>(static_cast<double>(feature_data.get("chance", 1.0))));
            feature.count_min = static_cast<int>(feature_data.get("count_min", Variant(feature.count_min)));
            feature.count_max = static_cast<int>(feature_data.get("count_max", Variant(feature.count_max)));
            feature.radius_min = static_cast<int>(feature_data.get("radius_min", Variant(feature.radius_min)));
            feature.radius_max = static_cast<int>(feature_data.get("radius_max", Variant(feature.radius_max)));
            feature.egg_count_min = static_cast<int>(feature_data.get("egg_count_min", Variant(feature.egg_count_min)));
            feature.egg_count_max = static_cast<int>(feature_data.get("egg_count_max", Variant(feature.egg_count_max)));

            normalize_range(feature.count_min, feature.count_max, 0);
            normalize_range(feature.radius_min, feature.radius_max, 1);
            normalize_range(feature.egg_count_min, feature.egg_count_max, 0);
            if (feature.type.is_empty()) continue;

            info.dynamic_features.push_back(feature);
        }
    }

    return info;
}

const DungeonInfo* DungeonDb::get_dungeon_info(const String &p_id) const {
    return get_info(p_id);
}

}
