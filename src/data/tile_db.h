#ifndef SPACETRAVELLER_TILE_DB_H
#define SPACETRAVELLER_TILE_DB_H

#include <godot_cpp/classes/object.hpp>
#include "database.h"
#include <godot_cpp/variant/vector2i.hpp>
#include <unordered_map>

namespace godot {

struct TileInfo {
    Vector2i atlas;
    bool solid;
};

class TileDb : public Object {
    GDCLASS(TileDb, Object)

private:
    static TileDb *singleton;
    DataBaseHelper db_helper;
    std::unordered_map<String, TileInfo, StringHasher> cache;

protected:
    static void _bind_methods();

public:
    static TileDb *get_singleton() { return singleton; }
    static void create_singleton();
    static void delete_singleton();

    TileDb();
    ~TileDb();

    void initialize_data();

    // Fast C++ access
    const TileInfo* get_tile_info(const String &p_id) const;
    const TileInfo* get_tile_info(uint16_t p_id) const;

    // GDScript/Standard access
    Vector2i get_atlas_coords(const String &p_id) const;
    bool is_solid(const String &p_id) const;
    Dictionary get_full_data() const { return db_helper.get_full_data(); }
};

}

#endif // ! SPACETRAVELLER_TILE_DB_H
