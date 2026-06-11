#ifndef SPACETRAVELLER_TILE_DB_H
#define SPACETRAVELLER_TILE_DB_H

#include <godot_cpp/classes/object.hpp>
#include "database.h"

namespace godot {

struct TileInfo {
    String name;
    std::vector<Vector2i> atlas_variants;
    bool solid;
    std::vector<uint16_t> tags;
    uint16_t smash_loot_table = 0;
    uint16_t spawn_loot_table = 0;
    uint16_t opens_to = 0;
    uint16_t closes_to = 0;
    bool hides_items = false;
    bool transparent = false;
};

class TileDb : public Object, public DataBase<TileInfo, TileDb> {
    GDCLASS(TileDb, Object)

protected:
    static void _bind_methods();
    virtual TileInfo _parse_row(const Dictionary &p_data) override;

    std::vector<TileInfo> fast_cache;

public:
    TileDb();
    ~TileDb();

    void initialize_data() { fast_cache.clear(); DataBase::initialize_data("res://data/tiles"); }
    Array get_ids() const { return DataBase::get_ids(); }

    // Fast C++ access
    const TileInfo* get_tile_info(const String &p_id) const;
    const TileInfo* get_tile_info(uint16_t p_id) const;

    // GDScript/Standard access
    Vector2i get_atlas_coords(const String &p_id) const;
    bool is_solid(const String &p_id) const;
    bool has_tag(const String &p_id, const String &p_tag) const;
    bool has_tag(uint16_t p_id, uint16_t p_tag) const;
    bool hides_items_at(uint16_t p_id) const;
    String get_tile_name(const String &p_id) const;
    String get_smash_loot_table(const String &p_id) const;
    String get_spawn_loot_table(const String &p_id) const;
};

}

#endif // ! SPACETRAVELLER_TILE_DB_H
