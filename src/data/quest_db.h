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
    uint16_t              reward_loot_table = 0;
    std::vector<int>      amount_range;
    int                   friendship = 0;
    int                   romance    = 0;
};

struct QuestTemplate {
    String kind;                // "gather" | "kill" | "reach"
    String objective_kind;      // story quests use this to select the objective implementation
    bool story = false;         // authored, deterministic quest rather than a random offer
    String failure_policy = "permanent";
    int failure_cooldown_turns = 0;
    int time_limit_turns = 0;   // active quest deadline; zero means no deadline
    String prerequisite_quest;  // stable story quest kind that must be completed first
    String next_quest;          // stable story quest kind unlocked after completion
    String next_giver;          // authored handoff text / character identifier
    String giver_dialogue_id;   // optional dialogue identity allowed to offer this story quest
    std::vector<uint16_t> target_item_pool; // gather target items
    std::vector<uint16_t> target_item_tags; // gather target item tags
    uint16_t target_loot_table = 0;
    std::vector<String> giver_jobs;
    std::vector<String> target_races;        // kill: one race is selected from this pool
    std::vector<String> target_jobs;         // kill: one job is selected from this pool
    String location_context;                // optional kill location: forest or dungeon
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

    bool is_story_kind(const String &p_kind) const;
    String get_failure_policy(const String &p_kind) const;
    int get_failure_cooldown_turns(const String &p_kind) const;
    int get_time_limit_turns(const String &p_kind) const;
    String get_objective_kind(const String &p_kind) const;
    String get_prerequisite_quest(const String &p_kind) const;
    String get_next_quest(const String &p_kind) const;
    String get_next_giver(const String &p_kind) const;
    String get_giver_dialogue_id(const String &p_kind) const;

    String get_label_template(const String &p_kind) const;
    String get_description_template(const String &p_kind) const;
    Array  get_target_range(const String &p_kind) const;
    Array  get_target_item_pool(const String &p_kind) const;
    Array  get_target_item_tags(const String &p_kind) const;
    String get_target_loot_table(const String &p_kind) const;
    Array  get_giver_jobs(const String &p_kind) const;
    Array  get_target_races(const String &p_kind) const;
    Array  get_target_jobs(const String &p_kind) const;
    String get_location_context(const String &p_kind) const;
    Array  get_race_exclude(const String &p_kind) const;
    Array  get_tier_names(const String &p_kind) const;

    Array  get_tier_item_pool(const String &p_kind, const String &p_tier) const;
    String get_tier_reward_loot_table(const String &p_kind, const String &p_tier) const;
    Array  get_tier_amount_range(const String &p_kind, const String &p_tier) const;
    int    get_tier_friendship(const String &p_kind, const String &p_tier) const;
    int    get_tier_romance(const String &p_kind, const String &p_tier) const;

    bool get_tier_item_pool_vec(const String &p_kind, const String &p_tier, std::vector<uint16_t> &r_out) const;
    bool get_tier_amount_range_vec(const String &p_kind, const String &p_tier, int &r_min, int &r_max) const;
    bool get_target_item_pool_vec(const String &p_kind, std::vector<uint16_t> &r_out) const;
    bool get_target_item_tags_vec(const String &p_kind, std::vector<uint16_t> &r_out) const;
    bool get_target_races_vec(const String &p_kind, std::vector<String> &r_out) const;
    bool get_target_jobs_vec(const String &p_kind, std::vector<String> &r_out) const;
    uint16_t get_target_loot_table_id(const String &p_kind) const;
    uint16_t get_tier_reward_loot_table_id(const String &p_kind, const String &p_tier) const;
    bool get_giver_jobs_vec(const String &p_kind, std::vector<String> &r_out) const;
};

}

#endif // ! SPACETRAVELLER_QUEST_DB_H
