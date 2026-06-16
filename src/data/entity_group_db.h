#ifndef SPACETRAVELLER_ENTITY_GROUP_DB_H
#define SPACETRAVELLER_ENTITY_GROUP_DB_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <vector>
#include "database.h"
#include "core/rng.h"

namespace godot {

struct EntityGroupEntry {
    String entity;
    bool none = false;
    int weight = 1;
    int cumulative_weight = 0;
    String job;
    String dialogue_profile;
    String attitude;
    String ai_state;
};

struct EntityGroupInfo {
    String id;
    int total_weight = 0;
    std::vector<EntityGroupEntry> entries;
};

class EntityGroupDb : public Object, public DataBase<EntityGroupInfo, EntityGroupDb> {
    GDCLASS(EntityGroupDb, Object)

protected:
    static void _bind_methods();
    virtual EntityGroupInfo _parse_row(const Dictionary &p_data) override;

public:
    EntityGroupDb();
    ~EntityGroupDb();

    void initialize_data() { DataBase<EntityGroupInfo, EntityGroupDb>::initialize_data("res://data/entity_groups"); }
    Array get_ids() const { return DataBase<EntityGroupInfo, EntityGroupDb>::get_ids(); }
    Vector2i get_atlas_coords(const String &p_id) const;

    const EntityGroupInfo* get_entity_group(const String &p_id) const;
    const EntityGroupEntry* pick_weighted_entry(const String &p_id, Rng::Seeded &p_rng) const;
};

}

#endif // SPACETRAVELLER_ENTITY_GROUP_DB_H
