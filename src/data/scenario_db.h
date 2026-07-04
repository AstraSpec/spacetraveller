#ifndef SPACETRAVELLER_SCENARIO_DB_H
#define SPACETRAVELLER_SCENARIO_DB_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include "database.h"

namespace godot {

struct ScenarioInfo {
    String id;
    String display_name;
    String description;
    bool is_default = false;
    Dictionary location;
    Array items;
    Array equipment;
};

class ScenarioDb : public Object, public DataBase<ScenarioInfo, ScenarioDb> {
    GDCLASS(ScenarioDb, Object)

protected:
    static void _bind_methods();
    virtual ScenarioInfo _parse_row(const Dictionary &p_data) override;

public:
    ScenarioDb();
    ~ScenarioDb();

    void initialize_data() { DataBase<ScenarioInfo, ScenarioDb>::initialize_data("res://data/scenarios"); }
    Array get_ids() const;

    bool has_scenario(const String &p_id) const;
    String get_default_scenario_id() const;
    Dictionary get_scenario(const String &p_id) const;
    String get_display_name(const String &p_id) const;
    String get_description(const String &p_id) const;
    Dictionary get_location(const String &p_id) const;
    Array get_items(const String &p_id) const;
    Array get_equipment(const String &p_id) const;
};

}

#endif // SPACETRAVELLER_SCENARIO_DB_H
