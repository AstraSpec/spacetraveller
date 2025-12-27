#ifndef SPACETRAVELLER_CHUNK_DB_H
#define SPACETRAVELLER_CHUNK_DB_H

#include <godot_cpp/classes/object.hpp>
#include "database.h"

namespace godot {

struct ChunkInfo {
    Vector2i atlas;
};

class ChunkDb : public Object, public DataBase<ChunkInfo, ChunkDb> {
    GDCLASS(ChunkDb, Object)

protected:
    static void _bind_methods();
    virtual ChunkInfo _parse_row(const Dictionary &p_data) override;

public:
    ChunkDb();
    ~ChunkDb();

    void initialize_data() { DataBase::initialize_data("res://data/chunks"); }
    Array get_ids() const { return DataBase::get_ids(); }

    // Fast C++ access
    const ChunkInfo* get_chunk_info(const String &p_id) const;

    // GDScript/Standard access
    Vector2i get_atlas_coords(const String &p_id) const;
};

}

#endif // ! SPACETRAVELLER_CHUNK_DB_H
