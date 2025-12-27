#ifndef SPACETRAVELLER_STRUCTURE_DB_H
#define SPACETRAVELLER_STRUCTURE_DB_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <unordered_map>
#include <vector>
#include "database.h"

namespace godot {

struct StructureInfo {
    std::vector<uint16_t> data;
};

class StructureDb : public Object {
    GDCLASS(StructureDb, Object)

private:
    static StructureDb *singleton;
    DataBaseHelper db_helper;
    std::unordered_map<String, StructureInfo, StringHasher> cache;

protected:
    static void _bind_methods();

public:
    static StructureDb *get_singleton() { return singleton; }
    static void create_singleton();
    static void delete_singleton();

    StructureDb();
    ~StructureDb();

    void initialize_data();

    // Fast C++ access
    uint16_t get_tile_at(const String &p_structure_id, int p_x, int p_y) const;

    // GDScript access
    Dictionary get_full_data() const { return db_helper.get_full_data(); }
};

}

#endif // ! SPACETRAVELLER_STRUCTURE_DB_H