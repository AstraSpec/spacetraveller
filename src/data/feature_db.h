#ifndef SPACETRAVELLER_FEATURE_DB_H
#define SPACETRAVELLER_FEATURE_DB_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/string.hpp>
#include <vector>
#include "database.h"
#include "core/rng.h"

namespace godot {

struct FeatureEntryInfo {
    String structure_id;
    String tile_id;
    int weight = 1;
};

struct FeaturePoolInfo {
    String id;
    String type = "structure_stamp";
    std::vector<FeatureEntryInfo> entries;
    int total_weight = 0;
};

class FeatureDb : public Object, public DataBase<FeaturePoolInfo, FeatureDb> {
    GDCLASS(FeatureDb, Object)

protected:
    static void _bind_methods();
    virtual FeaturePoolInfo _parse_row(const Dictionary &p_data) override;

public:
    FeatureDb();
    ~FeatureDb();

    void initialize_data() { DataBase<FeaturePoolInfo, FeatureDb>::initialize_data("res://data/features"); }
    Array get_ids() const { return DataBase<FeaturePoolInfo, FeatureDb>::get_ids(); }

    const FeaturePoolInfo* get_feature_pool(const String &p_id) const;
    const FeatureEntryInfo* pick_weighted_entry(const FeaturePoolInfo& p_pool, Rng::Seeded &p_rng) const;
};

}

#endif // SPACETRAVELLER_FEATURE_DB_H
