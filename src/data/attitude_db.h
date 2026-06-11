#ifndef SPACETRAVELLER_ATTITUDE_DB_H
#define SPACETRAVELLER_ATTITUDE_DB_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/string.hpp>
#include "database.h"

namespace godot {

struct AttitudeInfo {
    String id;
    String display_name;
    String hostility_mode = "faction";
};

class AttitudeDb : public Object, public DataBase<AttitudeInfo, AttitudeDb> {
    GDCLASS(AttitudeDb, Object)

protected:
    static void _bind_methods();
    virtual AttitudeInfo _parse_row(const Dictionary &p_data) override;

public:
    AttitudeDb();
    ~AttitudeDb();

    void initialize_data() { DataBase<AttitudeInfo, AttitudeDb>::initialize_data("res://data/attitudes"); }
    Array get_ids() const { return DataBase<AttitudeInfo, AttitudeDb>::get_ids(); }

    const AttitudeInfo* get_attitude_info(const String &p_id) const;
    String get_hostility_mode(const String &p_id) const;
};

}

#endif // SPACETRAVELLER_ATTITUDE_DB_H
