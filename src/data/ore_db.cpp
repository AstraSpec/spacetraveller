#include "ore_db.h"

#include "core/id_registry.h"
#include "data/tile_db.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>

namespace godot {

template<> OreDb* DataBase<OreFormationInfo, OreDb>::singleton = nullptr;

static OreIntRange parse_int_range(const Variant& p_value, int p_default_min, int p_default_max) {
    OreIntRange range{p_default_min, p_default_max};
    if (p_value.get_type() != Variant::ARRAY) return range;
    Array values = p_value;
    if (values.size() < 2) return range;
    range.min = static_cast<int>(values[0]);
    range.max = static_cast<int>(values[1]);
    return range;
}

static OreFloatRange parse_float_range(const Variant& p_value, float p_default_min, float p_default_max) {
    OreFloatRange range{p_default_min, p_default_max};
    if (p_value.get_type() != Variant::ARRAY) return range;
    Array values = p_value;
    if (values.size() < 2) return range;
    range.min = static_cast<float>(values[0]);
    range.max = static_cast<float>(values[1]);
    return range;
}

void OreDb::_bind_methods() {
    ClassDB::bind_static_method("OreDb", D_METHOD("get_singleton"), &OreDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &OreDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_ids"), &OreDb::get_ids);
}

OreFormationInfo OreDb::_parse_row(const Dictionary& p_data) {
    OreFormationInfo info;
    info.id = String(p_data.get("id", ""));
    info.type = String(p_data.get("type", ""));
    info.candidate_slots = static_cast<int>(p_data.get("candidate_slots", 1));

    info.province_key = String(p_data.get("province_key", info.id));
    info.province_scale_chunks = static_cast<int>(p_data.get("province_scale_chunks", 12));
    info.province_threshold = static_cast<float>(p_data.get("province_threshold", 0.5));
    info.outside_chance = static_cast<float>(p_data.get("outside_chance", 0.0));
    info.inside_chance = static_cast<float>(p_data.get("inside_chance", info.outside_chance));

    info.depth = parse_int_range(p_data.get("depth", Variant()), 1, 1);
    info.thickness = parse_int_range(p_data.get("thickness", Variant()), 1, 1);
    info.length = parse_int_range(p_data.get("length", Variant()), 1, 1);
    info.width = parse_int_range(p_data.get("width", Variant()), 1, 1);
    info.cells = parse_int_range(p_data.get("cells", Variant()), 1, 1);
    info.richness = parse_float_range(p_data.get("richness", Variant()), 1.0f, 1.0f);

    Array minerals = p_data.get("minerals", Array());
    for (int i = 0; i < minerals.size(); i++) {
        if (minerals[i].get_type() != Variant::DICTIONARY) continue;
        Dictionary mineral_data = minerals[i];
        OreMineralInfo mineral;
        mineral.tile = String(mineral_data.get("tile", ""));
        mineral.weight = static_cast<int>(mineral_data.get("weight", 1));
        info.minerals.push_back(mineral);
    }
    return info;
}

static bool valid_int_range(const OreIntRange& p_range, int p_minimum = 1) {
    return p_range.min >= p_minimum && p_range.max >= p_range.min;
}

bool OreDb::validate_formation(OreFormationInfo& r_info) const {
    auto reject = [&](const String& p_reason) {
        UtilityFunctions::push_error("[OreDb] Invalid formation ", r_info.id, ": ", p_reason);
        return false;
    };

    if (r_info.id.is_empty()) return reject("missing id");
    if (r_info.type != "layered_lens" && r_info.type != "random_walk") {
        return reject("type must be layered_lens or random_walk");
    }
    if (r_info.candidate_slots < 1 || r_info.province_scale_chunks < 1) {
        return reject("candidate_slots and province_scale_chunks must be positive");
    }
    if (r_info.province_threshold < 0.0f || r_info.province_threshold > 1.0f ||
        r_info.outside_chance < 0.0f || r_info.outside_chance > 1.0f ||
        r_info.inside_chance < 0.0f || r_info.inside_chance > 1.0f) {
        return reject("province threshold and spawn chances must be in [0, 1]");
    }
    if (r_info.richness.min < 0.0f || r_info.richness.max > 1.0f || r_info.richness.max < r_info.richness.min) {
        return reject("richness must be an ordered range in [0, 1]");
    }
    if (r_info.minerals.empty()) return reject("minerals cannot be empty");

    if (r_info.type == "layered_lens") {
        if (!valid_int_range(r_info.depth) || !valid_int_range(r_info.thickness) ||
            !valid_int_range(r_info.length) || !valid_int_range(r_info.width)) {
            return reject("large-body depth and dimensions must be ordered positive ranges");
        }
    } else if (!valid_int_range(r_info.depth) || !valid_int_range(r_info.cells, 2)) {
        return reject("small-cluster depth and cells must be ordered positive ranges with at least two cells");
    }

    TileDb* tile_db = TileDb::get_singleton();
    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (!tile_db || !id_reg) return reject("TileDb and IdRegistry must be initialized first");

    r_info.total_mineral_weight = 0;
    for (OreMineralInfo& mineral : r_info.minerals) {
        const TileInfo* tile = tile_db->get_tile_info(mineral.tile);
        mineral.tile_id = id_reg->get_id(mineral.tile);
        if (!tile || mineral.tile_id == 0) return reject(String("unknown mineral tile ") + mineral.tile);
        if (!tile->solid) return reject(String("mineral tile must be solid: ") + mineral.tile);
        if (mineral.weight <= 0) return reject("mineral weights must be positive");
        r_info.total_mineral_weight += mineral.weight;
        mineral.cumulative_weight = r_info.total_mineral_weight;
    }
    return true;
}

void OreDb::initialize_data() {
    valid_formations.clear();
    DataBase<OreFormationInfo, OreDb>::initialize_data("res://data/ores");
    valid_formations.reserve(cache.size());
    for (const auto& pair : cache) {
        OreFormationInfo info = pair.second;
        if (validate_formation(info)) valid_formations.push_back(std::move(info));
    }
    std::sort(valid_formations.begin(), valid_formations.end(), [](const OreFormationInfo& a, const OreFormationInfo& b) {
        return a.id < b.id;
    });
    UtilityFunctions::print("OreDb validated ", valid_formations.size(), " formations");
}

const OreFormationInfo* OreDb::get_formation(const String& p_id) const {
    for (const OreFormationInfo& formation : valid_formations) {
        if (formation.id == p_id) return &formation;
    }
    return nullptr;
}

Array OreDb::get_ids() const {
    Array ids;
    for (const OreFormationInfo& formation : valid_formations) ids.push_back(formation.id);
    return ids;
}

}
