#ifndef SPACETRAVELLER_LOOT_DB_H
#define SPACETRAVELLER_LOOT_DB_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <vector>
#include <cstdint>
#include "database.h"
#include "core/rng.h"

namespace godot {

struct LootStack {
    uint16_t item_id = 0;
    int amount = 0;
};

struct LootEntry {
    uint16_t item_id = 0;
    uint16_t table_id = 0;
    int weight = 1;
    int cumulative_weight = 0;
    int amount_min = 1;
    int amount_max = 1;
};

struct LootTableInfo {
    String id;
    uint16_t numeric_id = 0;
    int rolls_min = 1;
    int rolls_max = 1;
    bool allow_duplicates = true;
    int total_weight = 0;
    std::vector<LootEntry> entries;
};

class LootDb : public Object, public DataBase<LootTableInfo, LootDb> {
    GDCLASS(LootDb, Object)

protected:
    static void _bind_methods();
    virtual LootTableInfo _parse_row(const Dictionary &p_data) override;

    std::vector<LootTableInfo> fast_cache;

    bool _roll_table_internal(uint16_t p_table_id, Rng::Seeded &p_rng, std::vector<LootStack> &r_out, int p_depth) const;

public:
    static constexpr int MAX_NESTED_DEPTH = 8;

    LootDb();
    ~LootDb();

    void initialize_data();
    Array get_ids() const { return DataBase<LootTableInfo, LootDb>::get_ids(); }

    const LootTableInfo* get_loot_table(const String &p_id) const;
    const LootTableInfo* get_loot_table(uint16_t p_id) const;
    uint16_t get_loot_table_id(const String &p_id) const;

    bool roll_table(uint16_t p_table_id, Rng::Seeded &p_rng, std::vector<LootStack> &r_out) const;
    Array roll_table_for_position(const String &p_table_id, int p_world_seed, const Vector2i &p_pos, int p_stream) const;
};

}

#endif // SPACETRAVELLER_LOOT_DB_H
