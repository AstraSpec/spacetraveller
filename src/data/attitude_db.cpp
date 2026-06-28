#include "attitude_db.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

template<> AttitudeDb* DataBase<AttitudeInfo, AttitudeDb>::singleton = nullptr;

void AttitudeDb::_bind_methods() {
    ClassDB::bind_static_method("AttitudeDb", D_METHOD("get_singleton"), &AttitudeDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &AttitudeDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_ids"), &AttitudeDb::get_ids);
    ClassDB::bind_method(D_METHOD("get_hostility_mode", "id"), &AttitudeDb::get_hostility_mode);
}

AttitudeDb::AttitudeDb() {}
AttitudeDb::~AttitudeDb() {}

AttitudeInfo AttitudeDb::_parse_row(const Dictionary &p_data) {
    AttitudeInfo info;
    info.id = String(p_data.get("id", "")).to_lower();
    info.display_name = String(p_data.get("display_name", info.id.capitalize()));
    info.hostility_mode = String(p_data.get("hostility_mode", "faction")).to_lower();
    if (info.hostility_mode != "never") {
        info.hostility_mode = "faction";
    }
    return info;
}

const AttitudeInfo* AttitudeDb::get_attitude_info(const String &p_id) const {
    return get_info(p_id.to_lower());
}

String AttitudeDb::get_hostility_mode(const String &p_id) const {
    const AttitudeInfo* info = get_attitude_info(p_id);
    return info ? info->hostility_mode : String("faction");
}

}
