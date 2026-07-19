#ifndef SPACETRAVELLER_DUNGEON_DB_H
#define SPACETRAVELLER_DUNGEON_DB_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/string.hpp>
#include <vector>
#include "database.h"

namespace godot {

struct DungeonDynamicFeatureInfo {
    String type;
    String placement = "room_random";
    float chance = 1.0f;
    int count_min = 1;
    int count_max = 1;
    int radius_min = 8;
    int radius_max = 10;
    int egg_count_min = 4;
    int egg_count_max = 6;
};

struct DungeonInfo {
    String id;
    String generator = "room_graph";
    String end_feature_pool;
    String end_structure_type;
    String room_feature_pool;
    String support_feature_pool;
    String ambient_entity_group;
    float ambient_entity_chance = 0.0f;
    uint16_t ambient_loot_table = 0;
    float ambient_loot_chance = 0.0f;
    uint16_t floor_tile = 0;
    uint16_t wall_tile = 0;
    int start_z = -1;
    int depth_min = 1;
    int depth_max = 1;
    int radius_chunks = 4;
    int room_count_min = 12;
    int room_count_max = 20;
    int corridor_width = 1;
    int segment_count_min = 10;
    int segment_count_max = 18;
    int segment_length_min = 12;
    int segment_length_max = 30;
    int main_width = 3;
    int branch_width = 2;
    float branch_chance = 0.35f;
    float loop_chance = 0.15f;
    int support_spacing_min = 5;
    int support_spacing_max = 8;
    int feature_room_count_min = 3;
    int feature_room_count_max = 6;
    std::vector<DungeonDynamicFeatureInfo> dynamic_features;
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
