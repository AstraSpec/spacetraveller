#ifndef SPACETRAVELLER_JOB_DB_H
#define SPACETRAVELLER_JOB_DB_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <vector>
#include "database.h"
#include "core/rng.h"

namespace godot {

struct JobInfo {
    String id;
    String display_name;
    String dialogue_profile = "default";
    int spawn_weight = 1;
    std::vector<String> traits;
    std::vector<String> context_tags;
    std::vector<String> quest_kinds;
    uint16_t vendor_loot_table = 0;
    Dictionary quest_loot_tables;
};

class JobDb : public Object, public DataBase<JobInfo, JobDb> {
    GDCLASS(JobDb, Object)

protected:
    static void _bind_methods();
    virtual JobInfo _parse_row(const Dictionary &p_data) override;

public:
    JobDb();
    ~JobDb();

    void initialize_data() { DataBase<JobInfo, JobDb>::initialize_data("res://data/jobs"); }
    Array get_ids() const { return DataBase<JobInfo, JobDb>::get_ids(); }

    const JobInfo* get_job_info(const String &p_id) const;
    const JobInfo* pick_weighted_job(Rng::Seeded &p_rng) const;

    String get_display_name(const String &p_id) const;
    String get_dialogue_profile(const String &p_id) const;
    int get_spawn_weight(const String &p_id) const;
    Array get_traits(const String &p_id) const;
    Array get_context_tags(const String &p_id) const;
    Array get_quest_kinds(const String &p_id) const;
    String get_vendor_loot_table(const String &p_id) const;
    Dictionary get_quest_loot_tables(const String &p_id) const;
};

}

#endif // SPACETRAVELLER_JOB_DB_H
