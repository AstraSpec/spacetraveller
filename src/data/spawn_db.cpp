#include "spawn_db.h"
#include "core/id_registry.h"
#include <godot_cpp/core/class_db.hpp>
#include <algorithm>

namespace godot {

template<> SpawnDb* DataBase<SpawnRuleInfo, SpawnDb>::singleton = nullptr;

static std::vector<uint16_t> parse_id_list(const Variant &p_var) {
    std::vector<uint16_t> result;
    if (p_var.get_type() != Variant::ARRAY) return result;
    IdRegistry *reg = IdRegistry::get_singleton();
    if (!reg) return result;
    Array arr = p_var;
    for (int i = 0; i < arr.size(); i++) {
        result.push_back(reg->register_string(String(arr[i])));
    }
    std::sort(result.begin(), result.end());
    return result;
}

void SpawnDb::_bind_methods() {
    ClassDB::bind_static_method("SpawnDb", D_METHOD("get_singleton"), &SpawnDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &SpawnDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_ids"), &SpawnDb::get_ids);
}

SpawnDb::SpawnDb() {}
SpawnDb::~SpawnDb() {}

SpawnRuleInfo SpawnDb::_parse_row(const Dictionary &p_data) {
    SpawnRuleInfo info;
    info.id = String(p_data.get("id", ""));
    info.chunk_ids = parse_id_list(p_data.get("chunk_ids", Array()));
    info.race_id = String(p_data.get("race_id", ""));
    info.spawn_weight = int(p_data.get("spawn_weight", 1));
    if (info.spawn_weight < 0) info.spawn_weight = 0;
    info.chance = static_cast<float>(static_cast<double>(p_data.get("chance", 1.0)));
    if (info.chance < 0.0f) info.chance = 0.0f;
    if (info.chance > 1.0f) info.chance = 1.0f;
    info.spawn_mode = String(p_data.get("spawn_mode", "free_cell"));
    info.tile_tags = _parse_tags(p_data.get("tile_tags", Array()));
    info.job = String(p_data.get("job", ""));
    info.dialogue_profile = String(p_data.get("dialogue_profile", ""));
    info.attitude = String(p_data.get("attitude", ""));
    info.role = String(p_data.get("role", ""));
    info.ai_state = String(p_data.get("ai_state", ""));
    return info;
}

const SpawnRuleInfo* SpawnDb::get_spawn_rule(const String &p_id) const {
    return get_info(p_id);
}

void SpawnDb::get_matching_rules(uint16_t p_chunk_id, const String &p_spawn_mode, std::vector<const SpawnRuleInfo*> &r_out) const {
    r_out.clear();
    for (const auto &pair : cache) {
        const SpawnRuleInfo &info = pair.second;
        if (info.spawn_weight <= 0) continue;
        if (info.race_id.is_empty()) continue;
        if (info.spawn_mode != p_spawn_mode) continue;
        if (!std::binary_search(info.chunk_ids.begin(), info.chunk_ids.end(), p_chunk_id)) continue;
        r_out.push_back(&info);
    }
}

const SpawnRuleInfo* SpawnDb::pick_weighted_rule(const std::vector<const SpawnRuleInfo*> &p_rules, Rng::Seeded &p_rng) const {
    if (p_rules.empty()) return nullptr;

    int total_weight = 0;
    for (const SpawnRuleInfo *rule : p_rules) {
        total_weight += rule ? rule->spawn_weight : 0;
    }
    if (total_weight <= 0) return p_rules.front();

    int roll = p_rng.range(1, total_weight);
    for (const SpawnRuleInfo *rule : p_rules) {
        if (!rule || rule->spawn_weight <= 0) continue;
        roll -= rule->spawn_weight;
        if (roll <= 0) return rule;
    }
    return p_rules.front();
}

}
