#include "style_db.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

template<> StyleDb* DataBase<StyleInfo, StyleDb>::singleton = nullptr;

void StyleDb::_bind_methods() {
    ClassDB::bind_static_method("StyleDb", D_METHOD("get_singleton"), &StyleDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &StyleDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_ids"), &StyleDb::get_ids);
}

StyleDb::StyleDb() {}
StyleDb::~StyleDb() {}

static std::vector<String> parse_string_list(const Variant &p_var) {
    std::vector<String> result;
    if (p_var.get_type() != Variant::ARRAY) return result;
    Array arr = p_var;
    for (int i = 0; i < arr.size(); i++) {
        result.push_back(arr[i]);
    }
    return result;
}

StyleInfo StyleDb::_parse_row(const Dictionary &p_data) {
    StyleInfo info;
    info.name = p_data.get("name", "");
    info.damage_mult = static_cast<float>(static_cast<double>(p_data.get("damage_mult", 1.0)));
    info.accuracy_mod = static_cast<float>(static_cast<double>(p_data.get("accuracy_mod", 0.0)));
    info.requires_unarmed = p_data.get("requires_unarmed", false);
    info.target_heights = parse_string_list(p_data.get("target_heights", Array()));

    Array abilities = p_data.get("abilities", Array());
    for (int i = 0; i < abilities.size(); i++) {
        Dictionary a = abilities[i];
        StyleAbilityEntry entry;
        entry.ability_id = a.get("id", "");
        entry.weight = static_cast<float>(static_cast<double>(a.get("weight", 1.0)));
        if (!entry.ability_id.is_empty()) info.abilities.push_back(entry);
    }
    return info;
}

const StyleInfo* StyleDb::get_style_info(const String &p_id) const {
    return get_info(p_id);
}

}
