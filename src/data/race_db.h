#ifndef SPACETRAVELLER_RACE_DB_H
#define SPACETRAVELLER_RACE_DB_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include "database.h"
#include "light_emission.h"

namespace godot {

struct RacePartDefinition {
    String part_id;
    String parent_part_id;
    int count;
    String height = "MID";
    float integrity_scale = 1.0f;
};

struct NaturalArmorInfo {
    bool enabled = false;
    float coverage = 0.0f;
    float bash = 0.0f;
    float cut = 0.0f;
    float pierce = 0.0f;
    float bash_transmission = 1.0f;
};

struct RaceInfo {
    String name;
    Vector2i atlas;
    int downed_atlas_offset = -1;
    float anatomy_scale = 1.0f;
    float speed = 1.0f;
    float base_damage = 10.0f;
    float base_stamina = 100.0f;
    String corpse_item;
    uint16_t death_loot_table = 0;
    String combat_style = "default";
    String faction;
    String reaction_policy = "defensive";
    String downed_target_policy = "continue";
    int reaction_radius = 12;
    String traversal_profile = "walker";
    std::vector<uint16_t> tags;
    std::vector<RacePartDefinition> parts;
    NaturalArmorInfo natural_armor;
    LightEmissionInfo light;
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

    void initialize_data() { fast_cache.clear(); DataBase::initialize_data("res://data/races"); }
    Array get_ids() const { return DataBase::get_ids(); }

    const RaceInfo* get_race_info(const String &p_id) const;
    const RaceInfo* get_race_info(uint16_t p_id) const;

    bool has_tag(const String &p_id, const String &p_tag) const;

    Vector2i get_atlas_coords(const String &p_id) const;
    Vector2i get_atlas_coords(uint16_t p_id) const;
};

}

#endif // ! SPACETRAVELLER_RACE_DB_H
