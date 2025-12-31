#ifndef SPACETRAVELLER_STRUCTURE_DB_H
#define SPACETRAVELLER_STRUCTURE_DB_H

#include <godot_cpp/classes/object.hpp>
#include "database.h"
#include <vector>

namespace godot {

struct StructureInfo {
    std::vector<uint16_t> data;
};

class StructureDb : public Object, public DataBase<StructureInfo, StructureDb> {
    GDCLASS(StructureDb, Object)

private:
    static const int CHUNK_SIZE;

protected:
    static void _bind_methods();
    virtual StructureInfo _parse_row(const Dictionary &p_data) override;

public:
    StructureDb();
    ~StructureDb();

    void initialize_data() { DataBase::initialize_data("res://data/structures"); }
    Array get_ids() const { return DataBase::get_ids(); }

    // Fast C++ access
    uint16_t get_tile_at(const String &p_structure_id, int p_x, int p_y) const;
};

}

#endif // ! SPACETRAVELLER_STRUCTURE_DB_H
