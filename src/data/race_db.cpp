#include "race_db.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

template<> RaceDb* DataBase<RaceInfo, RaceDb>::singleton = nullptr;

void RaceDb::_bind_methods() {
    ClassDB::bind_static_method("RaceDb", D_METHOD("get_singleton"), &RaceDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &RaceDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_ids"), &RaceDb::get_ids);
}

RaceDb::RaceDb() {}
RaceDb::~RaceDb() {}

RaceInfo RaceDb::_parse_row(const Dictionary &p_data) {
    RaceInfo info;
    info.name = p_data.get("name", "");
    
    Array parts = p_data.get("parts", Array());
    for (int i = 0; i < parts.size(); i++) {
        Dictionary p = parts[i];
        RacePartDefinition def;
        def.part_id = p.get("id", "");
        def.parent_part_id = p.get("parent", "");
        def.count = p.get("count", 1);
        info.parts.push_back(def);
    }
    
    return info;
}

const RaceInfo* RaceDb::get_race_info(const String &p_id) const {
    return get_info(p_id);
}

}
