#include "weapon_profile_db.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

template<>
WeaponProfileDb*
DataBase<WeaponProfileInfo, WeaponProfileDb>::singleton = nullptr;

namespace {

DamageType parse_damage_type(const String& value) {
    const String normalized = value.to_lower();
    if (normalized == "cut") return DamageType::CUT;
    if (normalized == "pierce") return DamageType::PIERCE;
    return DamageType::BASH;
}

std::vector<String> parse_string_list(const Variant& value) {
    std::vector<String> result;
    if (value.get_type() != Variant::ARRAY) return result;
    const Array array = value;
    for (int i = 0; i < array.size(); ++i) result.push_back(array[i]);
    return result;
}

}

void WeaponProfileDb::_bind_methods() {
    ClassDB::bind_static_method(
        "WeaponProfileDb",
        D_METHOD("get_singleton"),
        &WeaponProfileDb::get_singleton);
    ClassDB::bind_method(
        D_METHOD("initialize_data"),
        &WeaponProfileDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_ids"), &WeaponProfileDb::get_ids);
}

WeaponProfileDb::WeaponProfileDb() {}
WeaponProfileDb::~WeaponProfileDb() {}

WeaponProfileInfo WeaponProfileDb::_parse_row(const Dictionary& data) {
    WeaponProfileInfo profile;
    profile.name = data.get("name", "");
    const Array packets = data.get("packets", Array());
    for (int i = 0; i < packets.size(); ++i) {
        if (packets[i].get_type() != Variant::DICTIONARY) continue;
        const Dictionary row = packets[i];
        WeaponAttackPacket packet;
        packet.id = row.get("id", "");
        if (packet.id.is_empty()) continue;
        packet.name = row.get("name", packet.id.capitalize());
        packet.verb = row.get("verb", packet.id);
        packet.damage_type = parse_damage_type(row.get("damage_type", "bash"));
        packet.damage_mult = static_cast<float>(static_cast<double>(
            row.get("damage_mult", 1.0)));
        packet.accuracy = static_cast<float>(static_cast<double>(
            row.get("accuracy", 0.75)));
        packet.speed = static_cast<float>(static_cast<double>(
            row.get("speed", 1.0)));
        packet.stamina_cost = static_cast<float>(static_cast<double>(
            row.get("stamina_cost", 10.0)));
        packet.weight = static_cast<float>(static_cast<double>(
            row.get("weight", 1.0)));
        packet.allow_exhausted = row.get("allow_exhausted", false);
        packet.target_heights =
            parse_string_list(row.get("target_heights", Array()));
        if (row.has("effect")) {
            const Dictionary effect = row["effect"];
            packet.effect_type = effect.get("type", "");
            packet.effect_mode = effect.get("mode", "decay");
            packet.effect_magnitude = static_cast<float>(static_cast<double>(
                effect.get("magnitude", 0.0)));
            packet.effect_duration = static_cast<float>(static_cast<double>(
                effect.get("duration", 0.0)));
        }
        profile.packets.push_back(packet);
    }
    return profile;
}

const WeaponProfileInfo* WeaponProfileDb::get_profile_info(
    const String& id
) const {
    return get_info(id);
}

}
