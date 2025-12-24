#ifndef SPACETRAVELLER_CHUNK_DB_H
#define SPACETRAVELLER_CHUNK_DB_H

#include <godot_cpp/classes/object.hpp>
#include "database.h"
#include <godot_cpp/variant/vector2i.hpp>
#include <unordered_map>

namespace godot {

struct ChunkInfo {
    Vector2i atlas;
};

class ChunkDb : public Object {
    GDCLASS(ChunkDb, Object)

private:
    static ChunkDb *singleton;
    DataBaseHelper db_helper;
    std::unordered_map<String, ChunkInfo, StringHasher> cache;

protected:
    static void _bind_methods();

public:
    static ChunkDb *get_singleton() { return singleton; }
    static void create_singleton();
    static void delete_singleton();

    ChunkDb();
    ~ChunkDb();

    void initialize_data();

    // Fast C++ access
    const ChunkInfo* get_chunk_info(const String &p_id) const;

    // GDScript/Standard access
    Vector2i get_atlas_coords(const String &p_id) const;
    Dictionary get_full_data() const { return db_helper.get_full_data(); }
};

}

#endif // ! SPACETRAVELLER_CHUNK_DB_H
