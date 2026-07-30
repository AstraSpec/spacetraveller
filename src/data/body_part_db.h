#ifndef SPACETRAVELLER_BODY_PART_DB_H
#define SPACETRAVELLER_BODY_PART_DB_H

#include <godot_cpp/classes/object.hpp>
#include "database.h"

namespace godot {

struct BodyPartInfo {
    String name;
    std::vector<uint16_t> tags;
    float hit_weight = 1.0f;
    float max_integrity = 10.0f;
    float consciousness_multiplier = 1.0f;
    float pain_multiplier = 1.0f;
};

class BodyPartDb : public Object, public DataBase<BodyPartInfo, BodyPartDb> {
    GDCLASS(BodyPartDb, Object)

protected:
    static void _bind_methods();
    virtual BodyPartInfo _parse_row(const Dictionary &p_data) override;

public:
    BodyPartDb();
    ~BodyPartDb();

    void initialize_data() { DataBase::initialize_data("res://data/body_parts"); }
    Array get_ids() const { return DataBase::get_ids(); }

    const BodyPartInfo* get_body_part_info(const String &p_id) const;
    String get_body_part_name(const String &p_id) const;
    float get_body_part_hit_weight(const String &p_id) const;
    float get_body_part_max_integrity(const String &p_id) const;
    float get_body_part_consciousness_multiplier(const String &p_id) const;
    float get_body_part_pain_multiplier(const String &p_id) const;
};

}

#endif // ! SPACETRAVELLER_BODY_PART_DB_H
