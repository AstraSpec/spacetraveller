#ifndef SPACETRAVELLER_TRAVERSAL_PROFILE_DB_H
#define SPACETRAVELLER_TRAVERSAL_PROFILE_DB_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/string.hpp>
#include "database.h"
#include <vector>

namespace godot {

struct TraversalProfileInfo {
    String id;
    String display_name;
    bool can_move = true;
    bool can_enter_solid = false;
    bool can_open_doors = false;
    std::vector<uint16_t> allowed_tile_tags;
    std::vector<uint16_t> blocked_tile_tags;
    String requires_body_tag;
};

class TraversalProfileDb : public Object, public DataBase<TraversalProfileInfo, TraversalProfileDb> {
    GDCLASS(TraversalProfileDb, Object)

protected:
    static void _bind_methods();
    virtual TraversalProfileInfo _parse_row(const Dictionary &p_data) override;

public:
    TraversalProfileDb();
    ~TraversalProfileDb();

    void initialize_data() { DataBase<TraversalProfileInfo, TraversalProfileDb>::initialize_data("res://data/traversal_profiles"); }
    Array get_ids() const { return DataBase<TraversalProfileInfo, TraversalProfileDb>::get_ids(); }

    const TraversalProfileInfo* get_profile_info(const String &p_id) const;
};

}

#endif // SPACETRAVELLER_TRAVERSAL_PROFILE_DB_H
