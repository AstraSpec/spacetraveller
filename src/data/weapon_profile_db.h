#ifndef SPACETRAVELLER_WEAPON_PROFILE_DB_H
#define SPACETRAVELLER_WEAPON_PROFILE_DB_H

#include "components/damage.h"
#include "database.h"
#include <godot_cpp/classes/object.hpp>

namespace godot {

struct WeaponAttackPacket {
    String id;
    String name;
    String verb;
    DamageType damage_type = DamageType::BASH;
    float damage_mult = 1.0f;
    float accuracy = 0.75f;
    float speed = 1.0f;
    float stamina_cost = 10.0f;
    float weight = 1.0f;
    bool allow_exhausted = false;
    std::vector<String> target_heights;
    String effect_type;
    String effect_mode;
    float effect_magnitude = 0.0f;
    float effect_duration = 0.0f;
};

struct WeaponProfileInfo {
    String name;
    std::vector<WeaponAttackPacket> packets;
};

class WeaponProfileDb :
    public Object,
    public DataBase<WeaponProfileInfo, WeaponProfileDb> {
    GDCLASS(WeaponProfileDb, Object)

protected:
    static void _bind_methods();
    WeaponProfileInfo _parse_row(const Dictionary& data) override;

public:
    WeaponProfileDb();
    ~WeaponProfileDb();

    void initialize_data() {
        DataBase::initialize_data("res://data/weapon_profiles");
    }
    Array get_ids() const { return DataBase::get_ids(); }
    const WeaponProfileInfo* get_profile_info(const String& id) const;
};

}

#endif
