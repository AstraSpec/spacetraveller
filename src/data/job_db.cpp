#include "job_db.h"
#include "core/id_registry.h"
#include <godot_cpp/core/class_db.hpp>

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
    ClassDB::bind_method(D_METHOD("get_dialogues", "id"), &JobDb::get_dialogues);
    ClassDB::bind_method(D_METHOD("get_reaction_policy", "id"), &JobDb::get_reaction_policy);
    ClassDB::bind_method(D_METHOD("get_reaction_radius", "id"), &JobDb::get_reaction_radius);
    ClassDB::bind_method(D_METHOD("get_default_ai_state", "id"), &JobDb::get_default_ai_state);
    ClassDB::bind_method(D_METHOD("get_faction", "id"), &JobDb::get_faction);
    ClassDB::bind_method(D_METHOD("get_atlas_offset", "id"), &JobDb::get_atlas_offset);
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
    info.dialogues = parse_string_list(p_data.get("dialogues", Array()));
    info.reaction_policy = String(p_data.get("reaction_policy", "")).to_lower();
    info.reaction_radius = static_cast<int>(p_data.get("reaction_radius", 0));
    info.default_ai_state = String(p_data.get("default_ai_state", "")).to_lower();
    info.faction = String(p_data.get("faction", "")).to_lower();
    info.atlas_offset = int(p_data.get("atlas_offset", 0));
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

String JobDb::get_display_name(const String &p_id) const {
    const JobInfo* info = get_job_info(p_id);
    return info ? info->display_name : "";
}

Array JobDb::get_dialogues(const String &p_id) const {
    const JobInfo* info = get_job_info(p_id);
    return info ? to_array(info->dialogues) : Array();
}

String JobDb::get_reaction_policy(const String &p_id) const {
    const JobInfo* info = get_job_info(p_id);
    return info ? info->reaction_policy : "";
}

int JobDb::get_reaction_radius(const String &p_id) const {
    const JobInfo* info = get_job_info(p_id);
    return info ? info->reaction_radius : 0;
}

String JobDb::get_default_ai_state(const String &p_id) const {
    const JobInfo* info = get_job_info(p_id);
    return info ? info->default_ai_state : "";
}

String JobDb::get_faction(const String &p_id) const {
    const JobInfo* info = get_job_info(p_id);
    return info ? info->faction : "";
}

int JobDb::get_atlas_offset(const String &p_id) const {
    const JobInfo* info = get_job_info(p_id);
    return info ? info->atlas_offset : 0;
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
