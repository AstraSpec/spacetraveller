#ifndef SPACETRAVELLER_DUNGEON_DB_H
#define SPACETRAVELLER_DUNGEON_DB_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/string.hpp>
#include "database.h"

namespace godot {

struct DungeonInfo {
    String id;
    String generator = "room_graph";
    int start_z = -1;
    int depth_min = 1;
    int depth_max = 1;
    int radius_chunks = 4;
    int room_count_min = 12;
    int room_count_max = 20;
    int corridor_width = 1;
};

class DungeonDb : public Object, public DataBase<DungeonInfo, DungeonDb> {
    GDCLASS(DungeonDb, Object)

protected:
    static void _bind_methods();
    virtual DungeonInfo _parse_row(const Dictionary &p_data) override;

public:
    DungeonDb();
    ~DungeonDb();

    void initialize_data() { DataBase<DungeonInfo, DungeonDb>::initialize_data("res://data/dungeons"); }
    Array get_ids() const { return DataBase<DungeonInfo, DungeonDb>::get_ids(); }

    const DungeonInfo* get_dungeon_info(const String &p_id) const;
};

}

#endif // SPACETRAVELLER_DUNGEON_DB_H
