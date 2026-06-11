#include "npc_role_db.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

template<> NpcRoleDb* DataBase<NpcRoleInfo, NpcRoleDb>::singleton = nullptr;

void NpcRoleDb::_bind_methods() {
    ClassDB::bind_static_method("NpcRoleDb", D_METHOD("get_singleton"), &NpcRoleDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &NpcRoleDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_ids"), &NpcRoleDb::get_ids);
    ClassDB::bind_method(D_METHOD("get_default_attitude", "id"), &NpcRoleDb::get_default_attitude);
    ClassDB::bind_method(D_METHOD("get_default_ai_state", "id"), &NpcRoleDb::get_default_ai_state);
    ClassDB::bind_method(D_METHOD("get_default_dialogue_profile", "id"), &NpcRoleDb::get_default_dialogue_profile);
}

NpcRoleDb::NpcRoleDb() {}
NpcRoleDb::~NpcRoleDb() {}

NpcRoleInfo NpcRoleDb::_parse_row(const Dictionary &p_data) {
    NpcRoleInfo info;
    info.id = String(p_data.get("id", "")).to_lower();
    info.display_name = String(p_data.get("display_name", info.id.capitalize()));
    info.default_attitude = String(p_data.get("default_attitude", "")).to_lower();
    info.default_ai_state = String(p_data.get("default_ai_state", "")).to_lower();
    info.default_dialogue_profile = String(p_data.get("default_dialogue_profile", ""));
    return info;
}

const NpcRoleInfo* NpcRoleDb::get_role_info(const String &p_id) const {
    return get_info(p_id.to_lower());
}

String NpcRoleDb::get_default_attitude(const String &p_id) const {
    const NpcRoleInfo* info = get_role_info(p_id);
    return info ? info->default_attitude : String();
}

String NpcRoleDb::get_default_ai_state(const String &p_id) const {
    const NpcRoleInfo* info = get_role_info(p_id);
    return info ? info->default_ai_state : String();
}

String NpcRoleDb::get_default_dialogue_profile(const String &p_id) const {
    const NpcRoleInfo* info = get_role_info(p_id);
    return info ? info->default_dialogue_profile : String();
}

}
