#include "body_part_db.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

template<> BodyPartDb* DataBase<BodyPartInfo, BodyPartDb>::singleton = nullptr;

void BodyPartDb::_bind_methods() {
    ClassDB::bind_static_method("BodyPartDb", D_METHOD("get_singleton"), &BodyPartDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &BodyPartDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_body_part_name", "id"), &BodyPartDb::get_body_part_name);
    ClassDB::bind_method(D_METHOD("get_ids"), &BodyPartDb::get_ids);
}

BodyPartDb::BodyPartDb() {}
BodyPartDb::~BodyPartDb() {}

BodyPartInfo BodyPartDb::_parse_row(const Dictionary &p_data) {
    BodyPartInfo info;
    info.name = p_data.get("name", "");
    info.tags = _parse_tags(p_data.get("tags", Array()));
    return info;
}

const BodyPartInfo* BodyPartDb::get_body_part_info(const String &p_id) const {
    return get_info(p_id);
}

String BodyPartDb::get_body_part_name(const String &p_id) const {
    const BodyPartInfo* info = get_body_part_info(p_id);
    return info ? info->name : p_id;
}

}
