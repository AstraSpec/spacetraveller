#include "recipe_db.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

template<> RecipeDb* DataBase<RecipeInfo, RecipeDb>::singleton = nullptr;

void RecipeDb::_bind_methods() {
    ClassDB::bind_static_method("RecipeDb", D_METHOD("get_singleton"), &RecipeDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &RecipeDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_recipe_name", "id"), &RecipeDb::get_recipe_name);
    ClassDB::bind_method(D_METHOD("get_recipe_description", "id"), &RecipeDb::get_recipe_description);
    ClassDB::bind_method(D_METHOD("get_recipe_requirements", "id"), &RecipeDb::get_recipe_requirements);
    ClassDB::bind_method(D_METHOD("get_recipe_results", "id"), &RecipeDb::get_recipe_results);
    ClassDB::bind_method(D_METHOD("get_recipe_time", "id"), &RecipeDb::get_recipe_time);
    ClassDB::bind_method(D_METHOD("get_ids"), &RecipeDb::get_ids);
}

RecipeDb::RecipeDb() {}
RecipeDb::~RecipeDb() {}

RecipeInfo RecipeDb::_parse_row(const Dictionary &p_data) {
    RecipeInfo info;
    info.name = p_data.get("name", "");
    info.description = p_data.get("description", "");
    info.time_seconds = p_data.get("time", 0.0f);

    if (p_data.has("requirements")) {
        Array reqs = p_data["requirements"];
        for (int i = 0; i < reqs.size(); i++) {
            Dictionary req_data = reqs[i];
            RecipeRequirement req;
            req.item_id = req_data.get("id", "");
            req.amount = req_data.get("amount", 1);
            info.requirements.push_back(req);
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

Array RecipeDb::get_recipe_requirements(const String &p_id) const {
    Array list;
    const RecipeInfo* info = get_info(p_id);
    if (info) {
        for (const auto& req : info->requirements) {
            Dictionary d;
            d["id"] = req.item_id;
            d["amount"] = req.amount;
            list.push_back(d);
        }
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
