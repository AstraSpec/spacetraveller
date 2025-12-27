#ifndef SPACETRAVELLER_TILE_DB_H
#define SPACETRAVELLER_TILE_DB_H

#include <godot_cpp/classes/object.hpp>
#include "database.h"

namespace godot {

struct TileInfo {
    Vector2i atlas;
    bool solid;
};

class TileDb : public Object, public DataBase<TileInfo, TileDb> {
    GDCLASS(TileDb, Object)

protected:
    static void _bind_methods();
    virtual TileInfo _parse_row(const Dictionary &p_data) override;

public:
    TileDb();
    ~TileDb();

    void initialize_data() { DataBase::initialize_data("res://data/tiles"); }
    Array get_ids() const { return DataBase::get_ids(); }

    // Fast C++ access
    const TileInfo* get_tile_info(const String &p_id) const;
    const TileInfo* get_tile_info(uint16_t p_id) const;

    // GDScript/Standard access
    Vector2i get_atlas_coords(const String &p_id) const;
    bool is_solid(const String &p_id) const;
};

}

#endif // ! SPACETRAVELLER_TILE_DB_H
