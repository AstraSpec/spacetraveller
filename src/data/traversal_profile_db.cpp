#include "traversal_profile_db.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

template<> TraversalProfileDb* DataBase<TraversalProfileInfo, TraversalProfileDb>::singleton = nullptr;

void TraversalProfileDb::_bind_methods() {
    ClassDB::bind_static_method("TraversalProfileDb", D_METHOD("get_singleton"), &TraversalProfileDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &TraversalProfileDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_ids"), &TraversalProfileDb::get_ids);
}

TraversalProfileDb::TraversalProfileDb() {}
TraversalProfileDb::~TraversalProfileDb() {}

TraversalProfileInfo TraversalProfileDb::_parse_row(const Dictionary &p_data) {
    TraversalProfileInfo info;
    info.id = String(p_data.get("id", "")).to_lower();
    info.display_name = String(p_data.get("display_name", info.id.capitalize()));
    info.can_move = bool(p_data.get("can_move", true));
    info.can_enter_solid = bool(p_data.get("can_enter_solid", false));
    info.can_open_doors = bool(p_data.get("can_open_doors", false));
    info.allowed_tile_tags = _parse_tags(p_data.get("allowed_tile_tags", Array()));
    info.blocked_tile_tags = _parse_tags(p_data.get("blocked_tile_tags", Array()));
    info.requires_body_tag = String(p_data.get("requires_body_tag", ""));
    return info;
}

const TraversalProfileInfo* TraversalProfileDb::get_profile_info(const String &p_id) const {
    const TraversalProfileInfo* info = get_info(p_id.to_lower());
    return info ? info : get_info("walker");
}

}
