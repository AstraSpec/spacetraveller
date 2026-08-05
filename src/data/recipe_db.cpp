#include "recipe_db.h"
#include "core/id_registry.h"
#include "item_db.h"
#include "tool_quality_db.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

template<> RecipeDb* DataBase<RecipeInfo, RecipeDb>::singleton = nullptr;

void RecipeDb::_bind_methods() {
    ClassDB::bind_static_method("RecipeDb", D_METHOD("get_singleton"), &RecipeDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &RecipeDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_recipe_name", "id"), &RecipeDb::get_recipe_name);
    ClassDB::bind_method(D_METHOD("get_recipe_description", "id"), &RecipeDb::get_recipe_description);
    ClassDB::bind_method(D_METHOD("get_recipe_components", "id"), &RecipeDb::get_recipe_components);
    ClassDB::bind_method(D_METHOD("get_recipe_tool_requirements", "id"), &RecipeDb::get_recipe_tool_requirements);
    ClassDB::bind_method(D_METHOD("get_recipe_results", "id"), &RecipeDb::get_recipe_results);
    ClassDB::bind_method(D_METHOD("get_recipe_time", "id"), &RecipeDb::get_recipe_time);
    ClassDB::bind_method(D_METHOD("get_ids"), &RecipeDb::get_ids);
}

RecipeDb::RecipeDb() {}
RecipeDb::~RecipeDb() {}

bool RecipeDb::_validate_row(
    const Dictionary& p_data,
    String& r_error
) const {
    if (p_data.has("requirements")) {
        r_error = "field 'requirements' is unsupported; use 'components'";
        return false;
    }
    if (!p_data.has("components") ||
        p_data["components"].get_type() != Variant::ARRAY) {
        r_error = "field 'components' must be an array";
        return false;
    }
    ItemDb* items = ItemDb::get_singleton();
    const Array components = p_data["components"];
    for (int i = 0; i < components.size(); ++i) {
        if (components[i].get_type() != Variant::DICTIONARY) {
            r_error = "field 'components' entries must be objects";
            return false;
        }
        const Dictionary component = components[i];
        const String item_id = String(component.get("id", "")).strip_edges();
        const int amount = static_cast<int>(component.get("amount", 0));
        if (item_id.is_empty() || amount <= 0 || !items ||
            !items->get_item_info(item_id)) {
            r_error = "field 'components' contains an invalid item or amount";
            return false;
        }
    }
    if (p_data.has("tool_requirements")) {
        if (p_data["tool_requirements"].get_type() != Variant::ARRAY) {
            r_error = "field 'tool_requirements' must be an array";
            return false;
        }
        ToolQualityDb* qualities = ToolQualityDb::get_singleton();
        const Array requirements = p_data["tool_requirements"];
        for (int i = 0; i < requirements.size(); ++i) {
            if (requirements[i].get_type() != Variant::DICTIONARY) {
                r_error = "field 'tool_requirements' entries must be objects";
                return false;
            }
            const Dictionary requirement = requirements[i];
            const String quality =
                String(requirement.get("quality", "")).strip_edges();
            RarityTier rarity;
            if (quality.is_empty() || !qualities ||
                !qualities->get_quality_info(quality)) {
                r_error = "field 'tool_requirements' references unknown quality '" +
                    quality + "'";
                return false;
            }
            if (!requirement.has("minimum_rarity") ||
                requirement["minimum_rarity"].get_type() != Variant::STRING ||
                !rarity_from_string(
                    String(requirement["minimum_rarity"]), rarity)) {
                r_error = "field 'tool_requirements.minimum_rarity' is invalid";
                return false;
            }
        }
    }
    if (!p_data.has("results") ||
        p_data["results"].get_type() != Variant::ARRAY ||
        Array(p_data["results"]).is_empty()) {
        r_error = "field 'results' must be a non-empty array";
        return false;
    }
    const Array results = p_data["results"];
    for (int i = 0; i < results.size(); ++i) {
        if (results[i].get_type() != Variant::DICTIONARY) {
            r_error = "field 'results' entries must be objects";
            return false;
        }
        const Dictionary result = results[i];
        const String item_id = String(result.get("id", "")).strip_edges();
        const int amount = static_cast<int>(result.get("amount", 0));
        if (item_id.is_empty() || amount <= 0 || !items ||
            !items->get_item_info(item_id)) {
            r_error = "field 'results' contains an invalid item or amount";
            return false;
        }
    }
    const double time = static_cast<double>(p_data.get("time", 0.0));
    if (time <= 0.0) {
        r_error = "field 'time' must be positive";
        return false;
    }
    return true;
}

RecipeInfo RecipeDb::_parse_row(const Dictionary &p_data) {
    RecipeInfo info;
    info.name = p_data.get("name", "");
    info.description = p_data.get("description", "");
    info.time_seconds = p_data.get("time", 0.0f);

    if (p_data.has("components")) {
        Array reqs = p_data["components"];
        for (int i = 0; i < reqs.size(); i++) {
            Dictionary req_data = reqs[i];
            RecipeComponent req;
            req.item_id = req_data.get("id", "");
            req.amount = req_data.get("amount", 1);
            info.components.push_back(req);
        }
    }

    if (p_data.has("tool_requirements")) {
        Array reqs = p_data["tool_requirements"];
        IdRegistry* registry = IdRegistry::get_singleton();
        for (int i = 0; i < reqs.size(); i++) {
            Dictionary req_data = reqs[i];
            RecipeToolRequirement req;
            req.quality_id = registry
                ? registry->register_string(String(req_data["quality"]))
                : 0;
            rarity_from_string(
                String(req_data["minimum_rarity"]), req.minimum_rarity);
            info.tool_requirements.push_back(req);
        }
    }

    if (p_data.has("results")) {
        Array res = p_data["results"];
        for (int i = 0; i < res.size(); i++) {
            Dictionary res_data = res[i];
            RecipeResult result;
            result.item_id = res_data.get("id", "");
            result.amount = res_data.get("amount", 1);
            info.results.push_back(result);
        }
    }

    return info;
}

String RecipeDb::get_recipe_name(const String &p_id) const {
    const RecipeInfo* info = get_info(p_id);
    return info ? info->name : "";
}

String RecipeDb::get_recipe_description(const String &p_id) const {
    const RecipeInfo* info = get_info(p_id);
    return info ? info->description : "";
}

Array RecipeDb::get_recipe_components(const String &p_id) const {
    Array list;
    const RecipeInfo* info = get_info(p_id);
    if (info) {
        for (const auto& req : info->components) {
            Dictionary d;
            d["id"] = req.item_id;
            d["amount"] = req.amount;
            list.push_back(d);
        }
    }
    return list;
}

Array RecipeDb::get_recipe_tool_requirements(const String& p_id) const {
    Array list;
    const RecipeInfo* info = get_info(p_id);
    IdRegistry* registry = IdRegistry::get_singleton();
    ToolQualityDb* qualities = ToolQualityDb::get_singleton();
    if (!info || !registry) return list;
    for (const RecipeToolRequirement& req : info->tool_requirements) {
        const String quality = registry->get_string(req.quality_id);
        Dictionary d;
        d["quality"] = quality;
        d["quality_name"] = qualities
            ? qualities->get_display_name(quality)
            : quality.capitalize();
        d["minimum_rarity"] = rarity_to_string(req.minimum_rarity);
        list.push_back(d);
    }
    return list;
}

Array RecipeDb::get_recipe_results(const String &p_id) const {
    Array list;
    const RecipeInfo* info = get_info(p_id);
    if (info) {
        for (const auto& res : info->results) {
            Dictionary d;
            d["id"] = res.item_id;
            d["amount"] = res.amount;
            list.push_back(d);
        }
    }
    return list;
}

float RecipeDb::get_recipe_time(const String &p_id) const {
    const RecipeInfo* info = get_info(p_id);
    return info ? info->time_seconds : 0.0f;
}

}
