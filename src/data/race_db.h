#ifndef SPACETRAVELLER_RACE_DB_H
#define SPACETRAVELLER_RACE_DB_H

#include <godot_cpp/classes/object.hpp>
#include "database.h"

namespace godot {

struct RacePartDefinition {
    String part_id;
    String parent_part_id;
    int count;
};

struct RaceInfo {
    String name;
    std::vector<RacePartDefinition> parts;
};

class RaceDb : public Object, public DataBase<RaceInfo, RaceDb> {
    GDCLASS(RaceDb, Object)

protected:
    static void _bind_methods();
    virtual RaceInfo _parse_row(const Dictionary &p_data) override;

public:
    RaceDb();
    ~RaceDb();

    void initialize_data() { DataBase::initialize_data("res://data/races"); }
    Array get_ids() const { return DataBase::get_ids(); }

    const RaceInfo* get_race_info(const String &p_id) const;
};

}

#endif // ! SPACETRAVELLER_RACE_DB_H
