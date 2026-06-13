#include "feature_db.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

template<> FeatureDb* DataBase<FeaturePoolInfo, FeatureDb>::singleton = nullptr;

void FeatureDb::_bind_methods() {
    ClassDB::bind_static_method("FeatureDb", D_METHOD("get_singleton"), &FeatureDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &FeatureDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_ids"), &FeatureDb::get_ids);
}

FeatureDb::FeatureDb() {}
FeatureDb::~FeatureDb() {}

FeaturePoolInfo FeatureDb::_parse_row(const Dictionary &p_data) {
    FeaturePoolInfo info;
    info.id = String(p_data.get("id", ""));
    info.type = String(p_data.get("type", "structure_stamp"));

    Variant entries_var = p_data.get("entries", Array());
    if (entries_var.get_type() == Variant::ARRAY) {
        Array entries = entries_var;
        for (int i = 0; i < entries.size(); i++) {
            if (entries[i].get_type() != Variant::DICTIONARY) continue;

            Dictionary entry_data = entries[i];
            FeatureEntryInfo entry;
            entry.structure_id = String(entry_data.get("structure", entry_data.get("structure_id", "")));
            entry.tile_id = String(entry_data.get("tile", entry_data.get("tile_id", "")));
            entry.weight = static_cast<int>(entry_data.get("weight", Variant(1)));
            if (entry.weight < 0) entry.weight = 0;
            if (entry.weight <= 0) continue;
            if (entry.structure_id.is_empty() && entry.tile_id.is_empty()) continue;

            info.total_weight += entry.weight;
            info.entries.push_back(entry);
        }
    }

    return info;
}

const FeaturePoolInfo* FeatureDb::get_feature_pool(const String &p_id) const {
    return get_info(p_id);
}

const FeatureEntryInfo* FeatureDb::pick_weighted_entry(const FeaturePoolInfo& p_pool, Rng::Seeded &p_rng) const {
    if (p_pool.entries.empty()) return nullptr;
    if (p_pool.total_weight <= 0) return &p_pool.entries.front();

    int roll = p_rng.range(1, p_pool.total_weight);
    for (const FeatureEntryInfo& entry : p_pool.entries) {
        if (entry.weight <= 0) continue;
        roll -= entry.weight;
        if (roll <= 0) {
            return &entry;
        }
    }

    return &p_pool.entries.front();
}

}
