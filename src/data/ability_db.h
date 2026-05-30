#ifndef SPACETRAVELLER_ABILITY_DB_H
#define SPACETRAVELLER_ABILITY_DB_H

#include <godot_cpp/classes/object.hpp>
#include "database.h"

namespace godot {

struct AbilityInfo {
    String name;
    String verb;
    std::vector<String> required_limbs;
    float damage_mult = 1.0f;
    float accuracy = 0.8f;
    float speed = 1.0f;
    float stamina_cost = 10.0f;
};

class AbilityDb : public Object, public DataBase<AbilityInfo, AbilityDb> {
    GDCLASS(AbilityDb, Object)

protected:
    static void _bind_methods();
    virtual AbilityInfo _parse_row(const Dictionary &p_data) override;

public:
    AbilityDb();
    ~AbilityDb();

    void initialize_data() { DataBase::initialize_data("res://data/abilities"); }
    Array get_ids() const { return DataBase::get_ids(); }

    const AbilityInfo* get_ability_info(const String &p_id) const;
};

}

#endif // ! SPACETRAVELLER_ABILITY_DB_H
