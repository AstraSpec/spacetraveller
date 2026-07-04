#include "scenario_db.h"
#include <godot_cpp/core/class_db.hpp>
#include <vector>
#include <algorithm>

namespace godot {

template<> ScenarioDb* DataBase<ScenarioInfo, ScenarioDb>::singleton = nullptr;

namespace {

bool scenario_less(const ScenarioInfo* a, const ScenarioInfo* b) {
    if (a->is_default != b->is_default) {
        return a->is_default;
    }
    int name_cmp = a->display_name.nocasecmp_to(b->display_name);
    if (name_cmp != 0) {
        return name_cmp < 0;
    }
    return a->id.nocasecmp_to(b->id) < 0;
}

}

void ScenarioDb::_bind_methods() {
    ClassDB::bind_static_method("ScenarioDb", D_METHOD("get_singleton"), &ScenarioDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &ScenarioDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_ids"), &ScenarioDb::get_ids);
    ClassDB::bind_method(D_METHOD("has_scenario", "id"), &ScenarioDb::has_scenario);
    ClassDB::bind_method(D_METHOD("get_default_scenario_id"), &ScenarioDb::get_default_scenario_id);
    ClassDB::bind_method(D_METHOD("get_scenario", "id"), &ScenarioDb::get_scenario);
    ClassDB::bind_method(D_METHOD("get_display_name", "id"), &ScenarioDb::get_display_name);
    ClassDB::bind_method(D_METHOD("get_description", "id"), &ScenarioDb::get_description);
    ClassDB::bind_method(D_METHOD("get_location", "id"), &ScenarioDb::get_location);
    ClassDB::bind_method(D_METHOD("get_items", "id"), &ScenarioDb::get_items);
    ClassDB::bind_method(D_METHOD("get_equipment", "id"), &ScenarioDb::get_equipment);
}

ScenarioDb::ScenarioDb() {}
ScenarioDb::~ScenarioDb() {}

ScenarioInfo ScenarioDb::_parse_row(const Dictionary &p_data) {
    ScenarioInfo info;
    info.id = String(p_data.get("id", "")).to_lower();
    info.display_name = String(p_data.get("display_name", info.id.capitalize()));
    info.description = String(p_data.get("description", ""));
    info.is_default = bool(p_data.get("default", false));
    info.location = p_data.get("location", Dictionary());
    info.items = p_data.get("items", Array());
    info.equipment = p_data.get("equipment", Array());
    return info;
}

Array ScenarioDb::get_ids() const {
    std::vector<const ScenarioInfo*> sorted;
    sorted.reserve(cache.size());
    for (const auto& pair : cache) {
        sorted.push_back(&pair.second);
    }
    std::sort(sorted.begin(), sorted.end(), scenario_less);

    Array ids;
    for (const ScenarioInfo* info : sorted) {
        ids.push_back(info->id);
    }
    return ids;
}

bool ScenarioDb::has_scenario(const String &p_id) const {
    return get_info(p_id.to_lower()) != nullptr;
}

String ScenarioDb::get_default_scenario_id() const {
    std::vector<const ScenarioInfo*> defaults;
    for (const auto& pair : cache) {
        if (pair.second.is_default) {
            defaults.push_back(&pair.second);
        }
    }
    if (defaults.empty()) {
        return String();
    }
    std::sort(defaults.begin(), defaults.end(), scenario_less);
    return defaults[0]->id;
}

Dictionary ScenarioDb::get_scenario(const String &p_id) const {
    Dictionary data;
    const ScenarioInfo* info = get_info(p_id.to_lower());
    if (!info) {
        return data;
    }

    data["id"] = info->id;
    data["display_name"] = info->display_name;
    data["description"] = info->description;
    data["location"] = info->location;
    data["items"] = info->items;
    data["equipment"] = info->equipment;
    return data;
}

String ScenarioDb::get_display_name(const String &p_id) const {
    const ScenarioInfo* info = get_info(p_id.to_lower());
    return info ? info->display_name : String();
}

String ScenarioDb::get_description(const String &p_id) const {
    const ScenarioInfo* info = get_info(p_id.to_lower());
    return info ? info->description : String();
}

Dictionary ScenarioDb::get_location(const String &p_id) const {
    const ScenarioInfo* info = get_info(p_id.to_lower());
    return info ? info->location : Dictionary();
}

Array ScenarioDb::get_items(const String &p_id) const {
    const ScenarioInfo* info = get_info(p_id.to_lower());
    return info ? info->items : Array();
}

Array ScenarioDb::get_equipment(const String &p_id) const {
    const ScenarioInfo* info = get_info(p_id.to_lower());
    return info ? info->equipment : Array();
}

}
