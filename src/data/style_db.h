#ifndef SPACETRAVELLER_STYLE_DB_H
#define SPACETRAVELLER_STYLE_DB_H

#include <godot_cpp/classes/object.hpp>
#include "database.h"

namespace godot {

struct StyleAbilityEntry {
    String ability_id;
    float weight = 1.0f;
};

struct StyleInfo {
    String name;
    float damage_mult = 1.0f;
    float accuracy_mod = 0.0f;
    bool requires_unarmed = false;
    std::vector<String> target_heights;
    std::vector<StyleAbilityEntry> abilities;
};

class StyleDb : public Object, public DataBase<StyleInfo, StyleDb> {
    GDCLASS(StyleDb, Object)

protected:
    static void _bind_methods();
    virtual StyleInfo _parse_row(const Dictionary &p_data) override;

public:
    StyleDb();
    ~StyleDb();

    void initialize_data() { DataBase::initialize_data("res://data/styles"); }
    Array get_ids() const { return DataBase::get_ids(); }

    const StyleInfo* get_style_info(const String &p_id) const;
};

}

#endif // ! SPACETRAVELLER_STYLE_DB_H
