#include "tool_quality_db.h"

#include <godot_cpp/core/class_db.hpp>

namespace godot {

template<> ToolQualityDb*
DataBase<ToolQualityInfo, ToolQualityDb>::singleton = nullptr;

void ToolQualityDb::_bind_methods() {
    ClassDB::bind_static_method(
        "ToolQualityDb",
        D_METHOD("get_singleton"),
        &ToolQualityDb::get_singleton);
    ClassDB::bind_method(
        D_METHOD("initialize_data"),
        &ToolQualityDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_ids"), &ToolQualityDb::get_ids);
    ClassDB::bind_method(
        D_METHOD("get_display_name", "id"),
        &ToolQualityDb::get_display_name);
}

ToolQualityInfo ToolQualityDb::_parse_row(const Dictionary& p_data) {
    ToolQualityInfo info;
    info.name = String(p_data.get("name", ""));
    return info;
}

const ToolQualityInfo* ToolQualityDb::get_quality_info(
    const String& p_id
) const {
    return get_info(p_id);
}

String ToolQualityDb::get_display_name(const String& p_id) const {
    const ToolQualityInfo* info = get_quality_info(p_id);
    return info && !info->name.is_empty() ? info->name : p_id.capitalize();
}

}
