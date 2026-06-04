#ifndef SPACETRAVELLER_QUEST_DB_H
#define SPACETRAVELLER_QUEST_DB_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include "database.h"
#include "core/string_hasher.h"

namespace godot {

struct QuestTier {
    std::vector<uint16_t> item_pool;
    std::vector<int>      amount_range;
    int                   friendship = 0;
    int                   romance    = 0;
};

struct QuestTemplate {
    String kind;                // "gather" | "kill" | "reach"
    std::vector<String> item_type_filter; // gather
    std::vector<String> race_exclude;     // kill
    std::vector<int>    target_range;
    String label_template;
    String description_template;
    std::unordered_map<String, QuestTier, StringHasher> tiers; // tier name -> QuestTier
};

class QuestDb : public Object, public DataBase<QuestTemplate, QuestDb> {
    GDCLASS(QuestDb, Object)

protected:
    static void _bind_methods();
    virtual QuestTemplate _parse_row(const Dictionary &p_data) override;

    std::vector<QuestTemplate> fast_cache;

public:
    QuestDb();
    ~QuestDb();

    void initialize_data() { DataBase<QuestTemplate, QuestDb>::initialize_data("res://data/quests"); }
    Array get_ids() const { return DataBase<QuestTemplate, QuestDb>::get_ids(); }

    Array get_kinds() const;

    String get_label_template(const String &p_kind) const;
    String get_description_template(const String &p_kind) const;
    Array  get_target_range(const String &p_kind) const;
    Array  get_item_type_filter(const String &p_kind) const;
    Array  get_race_exclude(const String &p_kind) const;
    Array  get_tier_names(const String &p_kind) const;

    Array  get_tier_item_pool(const String &p_kind, const String &p_tier) const;
    Array  get_tier_amount_range(const String &p_kind, const String &p_tier) const;
    int    get_tier_friendship(const String &p_kind, const String &p_tier) const;
    int    get_tier_romance(const String &p_kind, const String &p_tier) const;

    bool get_tier_item_pool_vec(const String &p_kind, const String &p_tier, std::vector<uint16_t> &r_out) const;
    bool get_tier_amount_range_vec(const String &p_kind, const String &p_tier, int &r_min, int &r_max) const;
};

}

#endif // ! SPACETRAVELLER_QUEST_DB_H
