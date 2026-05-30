#include "ability_db.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

template<> AbilityDb* DataBase<AbilityInfo, AbilityDb>::singleton = nullptr;

void AbilityDb::_bind_methods() {
    ClassDB::bind_static_method("AbilityDb", D_METHOD("get_singleton"), &AbilityDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &AbilityDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_ids"), &AbilityDb::get_ids);
}

AbilityDb::AbilityDb() {}
AbilityDb::~AbilityDb() {}

AbilityInfo AbilityDb::_parse_row(const Dictionary &p_data) {
    AbilityInfo info;
    info.name = p_data.get("name", "");
    info.verb = p_data.get("verb", "strike");
    info.damage_mult = static_cast<float>(static_cast<double>(p_data.get("damage_mult", 1.0)));
    info.accuracy = static_cast<float>(static_cast<double>(p_data.get("accuracy", 0.8)));
    info.speed = static_cast<float>(static_cast<double>(p_data.get("speed", 1.0)));
    info.stamina_cost = static_cast<float>(static_cast<double>(p_data.get("stamina_cost", 10.0)));

    Array limbs = p_data.get("required_limbs", Array());
    for (int i = 0; i < limbs.size(); i++) {
        info.required_limbs.push_back(limbs[i]);
    }

    if (p_data.has("effect")) {
        Dictionary fx = p_data["effect"];
        info.effect_type = fx.get("type", "");
        info.effect_mode = fx.get("mode", "decay");
        info.effect_magnitude = static_cast<float>(static_cast<double>(fx.get("magnitude", 0.0)));
        info.effect_duration = static_cast<float>(static_cast<double>(fx.get("duration", 0.0)));
    }
    return info;
}

const AbilityInfo* AbilityDb::get_ability_info(const String &p_id) const {
    return get_info(p_id);
}

}
