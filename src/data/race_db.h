#ifndef SPACETRAVELLER_RACE_DB_H
#define SPACETRAVELLER_RACE_DB_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include "database.h"

namespace godot {

struct RacePartDefinition {
    String part_id;
    String parent_part_id;
    int count;
};

struct RaceInfo {
    String name;
    Vector2i atlas;
    String perception_tier;
    std::vector<RacePartDefinition> parts;
};

class RaceDb : public Object, public DataBase<RaceInfo, RaceDb> {
    GDCLASS(RaceDb, Object)

protected:
    static void _bind_methods();
    virtual RaceInfo _parse_row(const Dictionary &p_data) override;

    std::vector<RaceInfo> fast_cache;

public:
    RaceDb();
    ~RaceDb();

    void initialize_data() { DataBase::initialize_data("res://data/races"); }
    Array get_ids() const { return DataBase::get_ids(); }

    const RaceInfo* get_race_info(const String &p_id) const;
    const RaceInfo* get_race_info(uint16_t p_id) const;

    Vector2i get_atlas_coords(const String &p_id) const;
    Vector2i get_atlas_coords(uint16_t p_id) const;
};

}

#endif // ! SPACETRAVELLER_RACE_DB_H
