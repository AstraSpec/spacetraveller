#include "dungeon_db.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

template<> DungeonDb* DataBase<DungeonInfo, DungeonDb>::singleton = nullptr;

void DungeonDb::_bind_methods() {
    ClassDB::bind_static_method("DungeonDb", D_METHOD("get_singleton"), &DungeonDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &DungeonDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_ids"), &DungeonDb::get_ids);
}

DungeonDb::DungeonDb() {}
DungeonDb::~DungeonDb() {}

DungeonInfo DungeonDb::_parse_row(const Dictionary &p_data) {
    DungeonInfo info;
    info.id = String(p_data.get("id", ""));
    info.generator = String(p_data.get("generator", "room_graph"));
    info.start_z = static_cast<int>(p_data.get("start_z", Variant(-1)));
    info.depth_min = static_cast<int>(p_data.get("depth_min", Variant(1)));
    info.depth_max = static_cast<int>(p_data.get("depth_max", Variant(info.depth_min)));
    info.radius_chunks = static_cast<int>(p_data.get("radius_chunks", Variant(4)));
    info.room_count_min = static_cast<int>(p_data.get("room_count_min", Variant(12)));
    info.room_count_max = static_cast<int>(p_data.get("room_count_max", Variant(info.room_count_min)));
    info.corridor_width = static_cast<int>(p_data.get("corridor_width", Variant(1)));

    if (info.depth_min < 1) info.depth_min = 1;
    if (info.depth_max < info.depth_min) info.depth_max = info.depth_min;
    if (info.radius_chunks < 1) info.radius_chunks = 1;
    if (info.room_count_min < 1) info.room_count_min = 1;
    if (info.room_count_max < info.room_count_min) info.room_count_max = info.room_count_min;
    if (info.corridor_width < 1) info.corridor_width = 1;

    return info;
}

const DungeonInfo* DungeonDb::get_dungeon_info(const String &p_id) const {
    return get_info(p_id);
}

}
