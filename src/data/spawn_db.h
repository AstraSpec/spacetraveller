#ifndef SPACETRAVELLER_SPAWN_DB_H
#define SPACETRAVELLER_SPAWN_DB_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <vector>
#include "database.h"
#include "core/rng.h"

namespace godot {

struct SpawnRuleInfo {
    String id;
    std::vector<uint16_t> chunk_ids;
    String race_id;
    int spawn_weight = 1;
    float chance = 1.0f;
    String spawn_mode = "free_cell";
    std::vector<uint16_t> tile_tags;
    String job;
    String dialogue_id;
    String attitude;
    String ai_state;
};

class SpawnDb : public Object, public DataBase<SpawnRuleInfo, SpawnDb> {
    GDCLASS(SpawnDb, Object)

protected:
    static void _bind_methods();
    virtual SpawnRuleInfo _parse_row(const Dictionary &p_data) override;

public:
    SpawnDb();
    ~SpawnDb();

    void initialize_data() { DataBase<SpawnRuleInfo, SpawnDb>::initialize_data("res://data/spawns"); }
    Array get_ids() const { return DataBase<SpawnRuleInfo, SpawnDb>::get_ids(); }

    const SpawnRuleInfo* get_spawn_rule(const String &p_id) const;
    void get_matching_rules(uint16_t p_chunk_id, const String &p_spawn_mode, std::vector<const SpawnRuleInfo*> &r_out) const;
    const SpawnRuleInfo* pick_weighted_rule(const std::vector<const SpawnRuleInfo*> &p_rules, Rng::Seeded &p_rng) const;
};

}

#endif // SPACETRAVELLER_SPAWN_DB_H
