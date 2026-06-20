#include "job_db.h"
#include "core/id_registry.h"
#include "core/tag_registry.h"
#include <godot_cpp/core/class_db.hpp>
#include <algorithm>

namespace godot {

template<> JobDb* DataBase<JobInfo, JobDb>::singleton = nullptr;

static std::vector<String> parse_string_list(const Variant &p_var) {
    std::vector<String> result;
    if (p_var.get_type() != Variant::ARRAY) return result;
    Array arr = p_var;
    for (int i = 0; i < arr.size(); i++) {
        result.push_back(String(arr[i]));
    }
    return result;
}

static Array to_array(const std::vector<String> &p_values) {
    Array arr;
    for (const String &value : p_values) {
        arr.push_back(value);
    }
    return arr;
}

void JobDb::_bind_methods() {
    ClassDB::bind_static_method("JobDb", D_METHOD("get_singleton"), &JobDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &JobDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_ids"), &JobDb::get_ids);
    ClassDB::bind_method(D_METHOD("get_display_name", "id"), &JobDb::get_display_name);
    ClassDB::bind_method(D_METHOD("get_dialogue_profile", "id"), &JobDb::get_dialogue_profile);
    ClassDB::bind_method(D_METHOD("get_default_attitude", "id"), &JobDb::get_default_attitude);
    ClassDB::bind_method(D_METHOD("get_default_ai_state", "id"), &JobDb::get_default_ai_state);
    ClassDB::bind_method(D_METHOD("get_spawn_weight", "id"), &JobDb::get_spawn_weight);
    ClassDB::bind_method(D_METHOD("get_atlas_offset", "id"), &JobDb::get_atlas_offset);
    ClassDB::bind_method(D_METHOD("has_tag", "id", "tag"), &JobDb::has_tag);
    ClassDB::bind_method(D_METHOD("get_traits", "id"), &JobDb::get_traits);
    ClassDB::bind_method(D_METHOD("get_context_tags", "id"), &JobDb::get_context_tags);
    ClassDB::bind_method(D_METHOD("get_quest_kinds", "id"), &JobDb::get_quest_kinds);
    ClassDB::bind_method(D_METHOD("get_vendor_loot_table", "id"), &JobDb::get_vendor_loot_table);
    ClassDB::bind_method(D_METHOD("get_quest_loot_tables", "id"), &JobDb::get_quest_loot_tables);
}

JobDb::JobDb() {}
JobDb::~JobDb() {}

JobInfo JobDb::_parse_row(const Dictionary &p_data) {
    JobInfo info;
    info.id = String(p_data.get("id", ""));
    info.display_name = String(p_data.get("display_name", info.id.capitalize()));
    info.dialogue_profile = String(p_data.get("dialogue_profile", info.id));
    info.default_attitude = String(p_data.get("default_attitude", "")).to_lower();
    info.default_ai_state = String(p_data.get("default_ai_state", "")).to_lower();
    info.spawn_weight = int(p_data.get("spawn_weight", 1));
    info.atlas_offset = int(p_data.get("atlas_offset", 0));
    if (info.spawn_weight < 0) info.spawn_weight = 0;
    info.tags = _parse_tags(p_data.get("tags", Array()));
    info.traits = parse_string_list(p_data.get("traits", Array()));
    info.context_tags = parse_string_list(p_data.get("context_tags", Array()));
    info.quest_kinds = parse_string_list(p_data.get("quest_kinds", Array()));
    String vendor_loot_table = String(p_data.get("vendor_loot_table", ""));
    if (!vendor_loot_table.is_empty() && IdRegistry::get_singleton()) {
        info.vendor_loot_table = IdRegistry::get_singleton()->register_string(vendor_loot_table);
    }
    info.quest_loot_tables = p_data.get("quest_loot_tables", Dictionary());
    return info;
}

const JobInfo* JobDb::get_job_info(const String &p_id) const {
    return get_info(p_id);
}

const JobInfo* JobDb::pick_weighted_job(Rng::Seeded &p_rng) const {
    if (cache.empty()) return nullptr;

    std::vector<String> ids;
    ids.reserve(cache.size());
    for (const auto &pair : cache) {
        ids.push_back(pair.first);
    }
    std::sort(ids.begin(), ids.end(), [](const String &a, const String &b) {
        return a < b;
    });

    int total_weight = 0;
    for (const String &id : ids) {
        const JobInfo &info = cache.at(id);
        if (info.spawn_weight > 0) {
            total_weight += info.spawn_weight;
        }
    }

    if (total_weight <= 0) {
        return &cache.at(ids.front());
    }

    int roll = p_rng.range(1, total_weight);
    for (const String &id : ids) {
        const JobInfo &info = cache.at(id);
        int weight = info.spawn_weight;
        if (weight <= 0) continue;
        roll -= weight;
        if (roll <= 0) {
            return &info;
        }
    }

    return &cache.at(ids.front());
}

String JobDb::get_display_name(const String &p_id) const {
    const JobInfo* info = get_job_info(p_id);
    return info ? info->display_name : "";
}

String JobDb::get_dialogue_profile(const String &p_id) const {
    const JobInfo* info = get_job_info(p_id);
    return info ? info->dialogue_profile : "";
}

String JobDb::get_default_attitude(const String &p_id) const {
    const JobInfo* info = get_job_info(p_id);
    return info ? info->default_attitude : "";
}

String JobDb::get_default_ai_state(const String &p_id) const {
    const JobInfo* info = get_job_info(p_id);
    return info ? info->default_ai_state : "";
}

int JobDb::get_spawn_weight(const String &p_id) const {
    const JobInfo* info = get_job_info(p_id);
    return info ? info->spawn_weight : 0;
}

int JobDb::get_atlas_offset(const String &p_id) const {
    const JobInfo* info = get_job_info(p_id);
    return info ? info->atlas_offset : 0;
}

bool JobDb::has_tag(const String &p_id, const String &p_tag) const {
    const JobInfo* info = get_job_info(p_id);
    if (!info) return false;

    TagRegistry *reg = TagRegistry::get_singleton();
    if (!reg) return false;

    uint16_t tag_id = reg->get_tag_id(p_tag);
    return TagRegistry::has_tag(tag_id, info->tags);
}

Array JobDb::get_traits(const String &p_id) const {
    const JobInfo* info = get_job_info(p_id);
    return info ? to_array(info->traits) : Array();
}

Array JobDb::get_context_tags(const String &p_id) const {
    const JobInfo* info = get_job_info(p_id);
    return info ? to_array(info->context_tags) : Array();
}

Array JobDb::get_quest_kinds(const String &p_id) const {
    const JobInfo* info = get_job_info(p_id);
    return info ? to_array(info->quest_kinds) : Array();
}

String JobDb::get_vendor_loot_table(const String &p_id) const {
    const JobInfo* info = get_job_info(p_id);
    IdRegistry* reg = IdRegistry::get_singleton();
    return (info && reg && info->vendor_loot_table != 0) ? reg->get_string(info->vendor_loot_table) : String();
}

Dictionary JobDb::get_quest_loot_tables(const String &p_id) const {
    const JobInfo* info = get_job_info(p_id);
    return info ? info->quest_loot_tables : Dictionary();
}

}
