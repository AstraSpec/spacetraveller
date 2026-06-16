#ifndef SPACETRAVELLER_TILE_GROUP_DB_H
#define SPACETRAVELLER_TILE_GROUP_DB_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <vector>
#include "database.h"

namespace godot {

struct TileGroupEntryInfo {
    uint16_t tile_id = 0;
    int weight = 1;
};

struct TileGroupInfo {
    String id;
    std::vector<TileGroupEntryInfo> entries;
    int total_weight = 0;
};

class TileGroupDb : public Object, public DataBase<TileGroupInfo, TileGroupDb> {
    GDCLASS(TileGroupDb, Object)

protected:
    static void _bind_methods();
    virtual TileGroupInfo _parse_row(const Dictionary &p_data) override;

public:
    TileGroupDb();
    ~TileGroupDb();

    void initialize_data() { DataBase<TileGroupInfo, TileGroupDb>::initialize_data("res://data/tile_groups"); }
    Array get_ids() const { return DataBase<TileGroupInfo, TileGroupDb>::get_ids(); }

    const TileGroupInfo* get_tile_group(const String &p_id) const;
};

}

#endif // SPACETRAVELLER_TILE_GROUP_DB_H
