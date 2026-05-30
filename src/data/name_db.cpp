#include "name_db.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

template<> NameDb* DataBase<NameInfo, NameDb>::singleton = nullptr;

static void parse_string_list(const Variant &p_var, std::vector<String> &out) {
    if (p_var.get_type() != Variant::ARRAY) return;
    Array arr = p_var;
    for (int i = 0; i < arr.size(); i++) {
        out.push_back(arr[i]);
    }
}

void NameDb::_bind_methods() {
    ClassDB::bind_static_method("NameDb", D_METHOD("get_singleton"), &NameDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &NameDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_ids"), &NameDb::get_ids);
}

NameDb::NameDb() {}
NameDb::~NameDb() {}

NameInfo NameDb::_parse_row(const Dictionary &p_data) {
    NameInfo info;
    parse_string_list(p_data.get("male", Array()), info.male);
    parse_string_list(p_data.get("female", Array()), info.female);
    parse_string_list(p_data.get("surname", Array()), info.surname);
    return info;
}

const NameInfo* NameDb::get_name_info(const String &p_id) const {
    return get_info(p_id);
}

String NameDb::generate(const String &p_race_id, const String &p_gender, Rng::Seeded &p_rng) const {
    const NameInfo* info = get_name_info(p_race_id);
    if (!info) return "";

    const std::vector<String> &first_pool = (p_gender == "female") ? info->female : info->male;

    String full;
    if (!first_pool.empty()) {
        full = first_pool[p_rng.range(0, static_cast<int>(first_pool.size()) - 1)];
    }
    if (!info->surname.empty()) {
        String surname = info->surname[p_rng.range(0, static_cast<int>(info->surname.size()) - 1)];
        full = full.is_empty() ? surname : (full + " " + surname);
    }
    return full;
}

}
