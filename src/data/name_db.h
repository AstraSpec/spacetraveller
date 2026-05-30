#ifndef SPACETRAVELLER_NAME_DB_H
#define SPACETRAVELLER_NAME_DB_H

#include <godot_cpp/classes/object.hpp>
#include "database.h"
#include "core/rng.h"

namespace godot {

struct NameInfo {
    std::vector<String> male;
    std::vector<String> female;
    std::vector<String> surname;
};

class NameDb : public Object, public DataBase<NameInfo, NameDb> {
    GDCLASS(NameDb, Object)

protected:
    static void _bind_methods();
    virtual NameInfo _parse_row(const Dictionary &p_data) override;

public:
    NameDb();
    ~NameDb();

    void initialize_data() { DataBase::initialize_data("res://data/names"); }
    Array get_ids() const { return DataBase::get_ids(); }

    const NameInfo* get_name_info(const String &p_id) const;

    // Draws a first name (by gender) and surname from the same rng stream.
    String generate(const String &p_race_id, const String &p_gender, Rng::Seeded &p_rng) const;
};

}

#endif // ! SPACETRAVELLER_NAME_DB_H
