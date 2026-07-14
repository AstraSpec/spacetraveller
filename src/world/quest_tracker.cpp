#include "quest_tracker.h"
#include "entities/entity_ledger.h"
#include "data/quest_db.h"
#include "data/item_db.h"
#include "data/race_db.h"
#include "data/job_db.h"
#include "data/tile_db.h"
#include "data/loot_db.h"
#include "core/id_registry.h"
#include "core/rng.h"
#include "core/tag_registry.h"
#include "entities/entity.h"
#include "world/world_generator.h"
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <cmath>

namespace godot {

static inline int _randi_index(int p_size) {
    if (p_size <= 0) return 0;
    return (int)((uint32_t)UtilityFunctions::randi() % (uint32_t)p_size);
}

static String quest_race_name(const String& p_race_id, int p_count) {
    String name = p_race_id.replace("_", " ").capitalize();
    if (p_count == 1) return name;
    if (p_race_id == "mouse") return "Mice";
    if (!name.ends_with("s")) name += "s";
    return name;
}

static String quest_job_name(const String& p_job_id) {
    JobDb* job_db = JobDb::get_singleton();
    if (job_db) {
        String display_name = job_db->get_display_name(p_job_id);
        if (!display_name.is_empty()) return display_name;
    }
    return p_job_id.replace("_", " ").capitalize();
}

void QuestTracker::configure(
    EntityLedger* p_ledger,
    QuestDb* p_db,
    uint32_t p_player_id,
    const int* p_world_seed,
    WorldGenerator* p_generator
) {
    ledger = p_ledger;
    db = p_db;
    generator = p_generator;
    player_entity_id = p_player_id;
    world_seed = p_world_seed;
}

Rng::Seeded QuestTracker::_quest_rng(const String& p_kind, uint32_t p_giver_entity_id) const {
    Vector2i pos;
    if (ledger) {
        const Entity* giver = ledger->get_entity_pool().get_entity(p_giver_entity_id);
        if (giver) pos = Vector2i(giver->x, giver->y);
    }
    uint32_t seed = world_seed ? static_cast<uint32_t>(*world_seed) : 0;
    uint64_t salt = (static_cast<uint64_t>(p_giver_entity_id) << 32) ^ static_cast<uint32_t>(p_kind.hash());
    return Rng::at(seed, pos, Rng::QUEST_LOOT, salt);
}

void QuestTracker::set_emit_callback(EmitFn p_emit, void* p_userdata) {
    emit_quest_updated = p_emit;
    emit_userdata = p_userdata;
}

float QuestTracker::_current_turn() const {
    if (!ledger) return 0.0f;
    const Entity* player = ledger->get_entity_pool().get_entity(player_entity_id);
    return player ? player->next_turn_time : 0.0f;
}

String QuestTracker::_failure_key(uint32_t p_giver_entity_id, const String& p_kind) const {
    return String::num_int64(static_cast<int64_t>(p_giver_entity_id)) + ":" + p_kind;
}

const QuestFailureHistory* QuestTracker::_get_failure_history(
    uint32_t p_giver_entity_id,
    const String& p_kind
) const {
    const auto it = failure_history.find(_failure_key(p_giver_entity_id, p_kind));
    return it == failure_history.end() ? nullptr : &it->second;
}

bool QuestTracker::_failure_allows_offer(uint32_t p_giver_entity_id, const String& p_kind) const {
    const QuestFailureHistory* history = _get_failure_history(p_giver_entity_id, p_kind);
    if (!history) return true;

    if (!history->ever_failed) return true;
    if (!db || db->get_failure_policy(p_kind) != "cooldown") return false;
    const int cooldown = db->get_failure_cooldown_turns(p_kind);
    if (cooldown <= 0) return false;
    return _current_turn() >= history->last_failed_turn + static_cast<float>(cooldown);
}

void QuestTracker::_record_failure(const QuestInstance& p_q) {
    if (p_q.template_kind.is_empty()) return;
    const String key = _failure_key(p_q.giver_entity_id, p_q.template_kind);
    QuestFailureHistory& history = failure_history[key];
    history.template_kind = p_q.template_kind;
    history.giver_entity_id = p_q.giver_entity_id;
    history.ever_failed = true;
    history.last_failed_turn = _current_turn();
    history.failure_count += 1;
}

void QuestTracker::_emit(const String& p_quest_id) {
    if (emit_quest_updated) emit_quest_updated(emit_userdata, p_quest_id);
}

bool QuestTracker::_is_gather_kind(const String& p_kind) const {
    const String objective_kind = db ? db->get_objective_kind(p_kind) : p_kind;
    return objective_kind == "gather" || objective_kind.ends_with("_gather");
}

bool QuestTracker::_location_matches(const String& p_kind, const Vector2i& p_position, int p_z) const {
    if (!db) return false;
    const String context = db->get_location_context(p_kind);
    if (context.is_empty()) return true;
    if (!generator || !world_seed) return false;

    const int seed = *world_seed;
    if (context == "dungeon") {
        return !generator->get_dungeon_type_for_cell(p_position.x, p_position.y, p_z, seed).is_empty();
    }
    if (context == "forest") {
        IdRegistry* reg = IdRegistry::get_singleton();
        return reg && generator->get_biome_id_for_cell(p_position.x, p_position.y, p_z, seed) == reg->get_id("forest");
    }
    return false;
}

void QuestTracker::on_game_event(const GameEvent& p_event) {
    _expire_due_quests();
    switch (p_event.type) {
        case GameEventType::ENTITY_KILLED: {
            fail_for_dead_giver(p_event.target_id);
            if (p_event.subject_id != player_entity_id) return;
            if (!ledger) return;
            Dictionary anatomy = ledger->get_anatomy(p_event.target_id);
            String race_id = String(anatomy.get("race_id", ""));
            Dictionary profile = ledger->get_social_profile(p_event.target_id);
            String job_id = String(profile.get("job", ""));
            if (race_id.is_empty() && job_id.is_empty()) return;
            const Entity* victim = ledger->get_entity_pool().get_entity(p_event.target_id);
            const int victim_z = victim ? victim->z : 0;
            for (auto& pair : instances) {
                QuestInstance& q = pair.second;
                if (q.status != "active") continue;
                if ((db ? db->get_objective_kind(q.template_kind) : q.template_kind) != "kill") continue;
                if (!_location_matches(q.template_kind, p_event.position, victim_z)) continue;
                const String target_race = String(q.params.get("race_id", ""));
                const String target_job = String(q.params.get("target_job", ""));
                if (!target_race.is_empty() && target_race != race_id) continue;
                if (!target_job.is_empty() && target_job != job_id) continue;
                _advance(pair.first, 1);
            }
            break;
        }
        case GameEventType::ITEM_PICKED_UP: {
            if (!db) return;
            if (p_event.item_id == 0) return;
            IdRegistry* reg = IdRegistry::get_singleton();
            if (!reg) return;
            String item_id_str = reg->get_string(p_event.item_id);
            if (item_id_str.is_empty()) return;
            for (auto& pair : instances) {
                QuestInstance& q = pair.second;
                if (q.status != "active") continue;
                if (!_is_gather_kind(q.template_kind)) continue;
                if (String(q.params.get("item_id", "")) != item_id_str) continue;
                _advance(pair.first, p_event.amount);
            }
            break;
        }
        case GameEventType::ENTITY_MOVED: {
            if (p_event.subject_id != player_entity_id) return;
            if (!ledger) return;
            for (auto& pair : instances) {
                QuestInstance& q = pair.second;
                if (q.status != "active") continue;
                if ((db ? db->get_objective_kind(q.template_kind) : q.template_kind) != "reach") continue;
                if (q.target <= 0) continue;
                _advance(pair.first, q.target); // reach objectives are one-shot
                break;
            }
            break;
        }
    }
}

QuestInstance QuestTracker::_sample_one(const String& p_kind, uint32_t p_giver_entity_id) {
    QuestInstance inst;
    Rng::Seeded quest_rng = _quest_rng(p_kind, p_giver_entity_id);

    uint64_t ticks = 0;
    Time* time_singleton = Time::get_singleton();
    if (time_singleton) {
        ticks = time_singleton->get_ticks_usec();
    }
    inst.id = String(p_kind) + String("_") + String::num_int64((int64_t)ticks) + String("_") + String::num_int64((int64_t)UtilityFunctions::randi());
    inst.template_kind = p_kind;
    inst.giver_entity_id = p_giver_entity_id;
    inst.status = "offered";
    inst.target = 0;
    inst.progress = 0;
    inst.started_turn = -1;

    if (!db) {
        UtilityFunctions::push_error("[QuestTracker] _sample_one: db is null");
        inst.params["__unfilled"] = true;
        return inst;
    }

    Array target_range = db->get_target_range(p_kind);
    int target_min = target_range.size() > 0 ? int(target_range[0]) : 1;
    int target_max = target_range.size() > 1 ? int(target_range[1]) : target_min;
    inst.target = quest_rng.range(target_min, target_max);

    Array tier_names = db->get_tier_names(p_kind);
    if (tier_names.is_empty()) {
        inst.params["__unfilled"] = true;
        return inst;
    }
    int tier_idx = quest_rng.range(0, tier_names.size() - 1);
    String tier = String(tier_names[tier_idx]);

    int amt_min = 1, amt_max = 1;
    db->get_tier_amount_range_vec(p_kind, tier, amt_min, amt_max);
    int reward_amount = quest_rng.range(amt_min, amt_max);

    std::vector<uint16_t> pool_vec;
    db->get_tier_item_pool_vec(p_kind, tier, pool_vec);
    String reward_item_id;
    Array reward_items;
    uint16_t reward_loot_table = db->get_tier_reward_loot_table_id(p_kind, tier);
    LootDb* loot_db = LootDb::get_singleton();
    IdRegistry* reg = IdRegistry::get_singleton();
    if (reward_loot_table != 0 && loot_db && reg) {
        std::vector<LootStack> stacks;
        loot_db->roll_table(reward_loot_table, quest_rng, stacks);
        for (const LootStack& stack : stacks) {
            if (stack.item_id == 0 || stack.amount <= 0) continue;
            Dictionary entry;
            entry["id"] = reg->get_string(stack.item_id);
            entry["amount"] = stack.amount;
            reward_items.push_back(entry);
        }
    } else if (!pool_vec.empty()) {
        uint16_t pick = pool_vec[quest_rng.range(0, (int)pool_vec.size() - 1)];
        IdRegistry* reg = IdRegistry::get_singleton();
        reward_item_id = reg ? reg->get_string(pick) : "";
    }

    int scale = inst.target / 3;
    if (scale < 1) scale = 1;
    if (scale > 3) scale = 3;
    int friendship_delta = db->get_tier_friendship(p_kind, tier) * scale;
    int romance_delta    = db->get_tier_romance(p_kind, tier) * scale;

    if (_is_gather_kind(p_kind)) {
        Array type_filter = db->get_item_type_filter(p_kind);
        std::vector<String> candidates;
        ItemDb* idb = ItemDb::get_singleton();
        std::vector<uint16_t> target_pool;
        std::vector<uint16_t> target_tags;
        db->get_target_item_pool_vec(p_kind, target_pool);
        db->get_target_item_tags_vec(p_kind, target_tags);
        uint16_t target_loot_table = db->get_target_loot_table_id(p_kind);
        if (target_loot_table != 0 && loot_db && reg) {
            std::vector<LootStack> stacks;
            loot_db->roll_table(target_loot_table, quest_rng, stacks);
            for (const LootStack& stack : stacks) {
                String id = reg->get_string(stack.item_id);
                if (!id.is_empty()) {
                    candidates.push_back(id);
                    break;
                }
            }
        }
        if (candidates.empty() && idb) {
            IdRegistry* reg = IdRegistry::get_singleton();
            if (!target_pool.empty() && reg) {
                for (uint16_t item_id : target_pool) {
                    String id = reg->get_string(item_id);
                    if (!id.is_empty()) candidates.push_back(id);
                }
            } else {
                Array ids = idb->get_ids();
                for (int i = 0; i < ids.size(); i++) {
                    String id = String(ids[i]);
                    const ItemInfo* info = idb->get_item_info(id);
                    if (!info) continue;
                    bool type_ok = type_filter.is_empty();
                    if (!type_ok) {
                        String t = idb->get_item_type(id);
                        for (int j = 0; j < type_filter.size(); j++) {
                            if (t == String(type_filter[j])) { type_ok = true; break; }
                        }
                    }
                    if (!type_ok) continue;
                    if (!target_tags.empty() && !TagRegistry::has_tag_any(info->tags, target_tags)) continue;
                    candidates.push_back(id);
                }
            }
        }
        if (!candidates.empty()) {
            String pick = candidates[quest_rng.range(0, (int)candidates.size() - 1)];
            inst.params["item_id"] = pick;
            String item_name = idb ? idb->get_item_name(pick) : "";
            if (item_name.is_empty()) item_name = pick;
            String label_tmpl = db->get_label_template(p_kind);
            String count_str = String::num_int64(inst.target);
            String label_after_count = label_tmpl.replace("{count}", count_str);
            inst.params["__label"] = label_after_count.replace("{item_name}", item_name);
            String desc_tmpl = db->get_description_template(p_kind);
            String desc_after_count = desc_tmpl.replace("{count}", count_str);
            inst.params["__description"] = desc_after_count.replace("{item_name}", item_name);
        } else {
            inst.params["__unfilled"] = true;
        }
    } else if ((db ? db->get_objective_kind(p_kind) : p_kind) == "kill") {
        Array race_exclude = db->get_race_exclude(p_kind);
        std::vector<String> race_candidates;
        std::vector<String> job_candidates;
        std::vector<String> target_races;
        std::vector<String> target_jobs;
        db->get_target_races_vec(p_kind, target_races);
        db->get_target_jobs_vec(p_kind, target_jobs);
        RaceDb* race_db_singleton = RaceDb::get_singleton();
        JobDb* job_db_singleton = JobDb::get_singleton();
        if (!target_races.empty()) {
            for (const String& race_id : target_races) {
                if (race_db_singleton && race_db_singleton->get_race_info(race_id)) {
                    race_candidates.push_back(race_id);
                }
            }
        } else if (target_jobs.empty() && race_db_singleton) {
            Array ids = race_db_singleton->get_ids();
            for (int i = 0; i < ids.size(); i++) {
                String id = String(ids[i]);
                bool excluded = false;
                for (int j = 0; j < race_exclude.size(); j++) {
                    if (id == String(race_exclude[j])) { excluded = true; break; }
                }
                if (!excluded) race_candidates.push_back(id);
            }
        }
        for (const String& job_id : target_jobs) {
            if (job_db_singleton && job_db_singleton->get_job_info(job_id)) {
                job_candidates.push_back(job_id);
            }
        }
        const bool race_filter_valid = target_races.empty() || !race_candidates.empty();
        const bool job_filter_valid = target_jobs.empty() || !job_candidates.empty();
        if (race_filter_valid && job_filter_valid) {
            String race_name;
            if (!race_candidates.empty()) {
                String pick_race = race_candidates[quest_rng.range(0, (int)race_candidates.size() - 1)];
                inst.params["race_id"] = pick_race;
                race_name = quest_race_name(pick_race, inst.target);
            }

            String job_name;
            if (!job_candidates.empty()) {
                String pick_job = job_candidates[quest_rng.range(0, (int)job_candidates.size() - 1)];
                inst.params["target_job"] = pick_job;
                job_name = quest_job_name(pick_job);
            }

            String label_tmpl = db->get_label_template(p_kind);
            String count_str = String::num_int64(inst.target);
            String label_after_count = label_tmpl.replace("{count}", count_str);
            label_after_count = label_after_count.replace("{race_name}", race_name);
            inst.params["__label"] = label_after_count.replace("{job_name}", job_name);
            String desc_tmpl = db->get_description_template(p_kind);
            String desc_after_count = desc_tmpl.replace("{count}", count_str);
            desc_after_count = desc_after_count.replace("{race_name}", race_name);
            inst.params["__description"] = desc_after_count.replace("{job_name}", job_name);
        } else {
            inst.params["__unfilled"] = true;
        }
    } else if ((db ? db->get_objective_kind(p_kind) : p_kind) == "reach") {
        std::vector<String> candidates;
        TileDb* tile_db_singleton = TileDb::get_singleton();
        if (tile_db_singleton) {
            Array ids = tile_db_singleton->get_ids();
            for (int i = 0; i < ids.size(); i++) {
                String id = String(ids[i]);
                String n = tile_db_singleton->get_tile_name(id);
                if (n.is_empty() || n == "???") continue;
                if (n.length() < 3) continue;
                candidates.push_back(id);
            }
        }
        if (!candidates.empty()) {
            String pick = candidates[quest_rng.range(0, (int)candidates.size() - 1)];
            inst.params["tile_id"] = pick;
            String tile_name = tile_db_singleton ? tile_db_singleton->get_tile_name(pick) : String();
            String label_tmpl = db->get_label_template(p_kind);
            inst.params["__label"] = label_tmpl.replace("{tile_name}", tile_name);
            String desc_tmpl = db->get_description_template(p_kind);
            inst.params["__description"] = desc_tmpl.replace("{tile_name}", tile_name);
        } else {
            inst.params["__unfilled"] = true;
        }
    }

    if (!reward_item_id.is_empty() && reward_amount > 0) {
        Dictionary entry;
        entry["id"] = reward_item_id;
        entry["amount"] = reward_amount;
        reward_items.push_back(entry);
    }
    inst.rewards["items"] = reward_items;
    inst.rewards["friendship_delta"] = friendship_delta;
    inst.rewards["romance_delta"]    = romance_delta;
    inst.rewards["event_category"]   = "quest";

    UtilityFunctions::print("[QuestTracker] _sample_one OK kind=", p_kind, " tier=", tier, " target=", inst.target, " reward_item=", reward_item_id);
    return inst;
}

bool QuestTracker::_giver_can_offer(const String& p_kind, uint32_t p_giver_entity_id) const {
    if (!db) return false;

    const String required_dialogue_id = db->get_giver_dialogue_id(p_kind);
    std::vector<String> jobs;
    const bool has_job_filter = db->get_giver_jobs_vec(p_kind, jobs);
    if (!has_job_filter && required_dialogue_id.is_empty()) return true;
    if (!ledger) return false;

    Dictionary profile = ledger->get_social_profile(p_giver_entity_id);
    if (!required_dialogue_id.is_empty() &&
        String(profile.get("dialogue_id", "")) != required_dialogue_id) {
        return false;
    }

    if (!has_job_filter) return true;
    String job = String(profile.get("job", ""));
    for (const String& allowed : jobs) {
        if (allowed == job) return true;
    }
    return false;
}

Dictionary QuestTracker::generate_offer(uint32_t p_giver_entity_id, const String& p_kind) {
    _expire_due_quests();
    if (!db || p_kind.is_empty() || db->is_story_kind(p_kind)) return Dictionary();
    if (!_giver_can_offer(p_kind, p_giver_entity_id)) return Dictionary();
    if (!_failure_allows_offer(p_giver_entity_id, p_kind)) return Dictionary();
    QuestInstance inst = _sample_one(p_kind, p_giver_entity_id);
    if (inst.params.has("__unfilled")) return Dictionary();
    instances[inst.id] = inst;
    Dictionary view = _view(inst.id, inst);
    _emit(inst.id);
    return view;
}

Dictionary QuestTracker::generate_story_offer(uint32_t p_giver_entity_id, const String& p_kind) {
    _expire_due_quests();
    if (!db || p_kind.is_empty() || !db->is_story_kind(p_kind)) return Dictionary();
    if (!_giver_can_offer(p_kind, p_giver_entity_id)) return Dictionary();
    if (!_failure_allows_offer(p_giver_entity_id, p_kind)) return Dictionary();

    const String objective_kind = db->get_objective_kind(p_kind);
    const bool supported_objective = _is_gather_kind(p_kind) ||
        objective_kind == "kill" || objective_kind == "reach";
    if (!supported_objective) return Dictionary();

    const String prerequisite = db->get_prerequisite_quest(p_kind);
    if (!prerequisite.is_empty() && !is_completed(prerequisite)) {
        return Dictionary();
    }
    if (prerequisite.is_empty()) {
        bool predecessor_declared = false;
        bool predecessor_completed = false;
        for (const Variant& kind_value : db->get_kinds()) {
            const String predecessor = String(kind_value);
            if (!db->is_story_kind(predecessor) || db->get_next_quest(predecessor) != p_kind) {
                continue;
            }
            predecessor_declared = true;
            if (is_completed(predecessor)) predecessor_completed = true;
        }
        if (predecessor_declared && !predecessor_completed) return Dictionary();
    }

    auto existing = instances.find(p_kind);
    if (existing != instances.end()) {
        QuestInstance& q = existing->second;
        if (q.giver_entity_id != p_giver_entity_id) return Dictionary();
        if (q.status == "completed") return Dictionary();
        if (q.status == "declined" || q.status == "failed") {
            q.status = "offered";
            q.progress = 0;
            q.started_turn = -1;
            q.deadline_turn = -1.0f;
            q.params.erase("__failure_reason");
            q.params.erase("__failure_turn");
            _emit(p_kind);
        }
        return _view(p_kind, q);
    }

    QuestInstance inst = _sample_one(p_kind, p_giver_entity_id);
    if (inst.params.has("__unfilled")) return Dictionary();
    // Story kinds are their own stable instance IDs.  This makes completion
    // checks and prerequisites independent of the random-offer ID scheme.
    inst.id = p_kind;
    instances[p_kind] = inst;
    Dictionary view = _view(p_kind, inst);
    _emit(p_kind);
    return view;
}

Array QuestTracker::generate_offers(uint32_t p_giver_entity_id, int p_count) {
    _expire_due_quests();
    Array out;
    if (!db) return out;
    Array kinds = db->get_kinds();
    if (kinds.is_empty()) return out;
    for (int i = 0; i < p_count; i++) {
        Array order = kinds.duplicate();
        int n = order.size();
        for (int k = n - 1; k > 0; k--) {
            int j = _randi_index(k + 1);
            if (k != j) {
                Variant tmp = order[k];
                order[k] = order[j];
                order[j] = tmp;
            }
        }
        bool filled = false;
        for (int j = 0; j < order.size(); j++) {
            String kind = String(order[j]);
            if (db->is_story_kind(kind)) continue;
            if (!_giver_can_offer(kind, p_giver_entity_id)) continue;
            if (!_failure_allows_offer(p_giver_entity_id, kind)) continue;
            QuestInstance inst = _sample_one(kind, p_giver_entity_id);
            if (!inst.params.has("__unfilled")) {
                instances[inst.id] = inst;
                out.push_back(_view(inst.id, inst));
                _emit(inst.id);
                filled = true;
                break;
            }
        }
        if (!filled) {
            if (!_failure_allows_offer(p_giver_entity_id, "gather")) return out;
            QuestInstance inst = _sample_one("gather", p_giver_entity_id);
            inst.params["item_id"] = "stick";
            inst.params["__label"] = String("Gather ") + String::num_int64(inst.target) + String(" sticks");
            inst.params["__description"] = String("Bring me ") + String::num_int64(inst.target) + String(" sticks.");
            instances[inst.id] = inst;
            out.push_back(_view(inst.id, inst));
            _emit(inst.id);
        }
    }
    return out;
}

bool QuestTracker::accept(const String& p_quest_id) {
    _expire_due_quests();
    auto it = instances.find(p_quest_id);
    if (it == instances.end()) return false;
    if (it->second.status != "offered") return false;
    it->second.status = "active";
    it->second.progress = 0;
    it->second.started_turn = static_cast<int>(std::floor(_current_turn()));
    const int time_limit = db ? db->get_time_limit_turns(it->second.template_kind) : 0;
    it->second.deadline_turn = time_limit > 0
        ? _current_turn() + static_cast<float>(time_limit)
        : -1.0f;
    _emit(p_quest_id);
    return true;
}

bool QuestTracker::decline(const String& p_quest_id) {
    auto it = instances.find(p_quest_id);
    if (it == instances.end()) return false;
    if (it->second.status != "offered") return false;
    it->second.status = "declined";
    _emit(p_quest_id);
    return true;
}

bool QuestTracker::fail_for_dead_giver(uint32_t p_giver_entity_id) {
    bool changed = false;
    std::vector<String> to_fail;
    for (const auto& pair : instances) {
        const QuestInstance& q = pair.second;
        if (q.giver_entity_id != p_giver_entity_id) continue;
        if (q.status != "offered" && q.status != "active") continue;
        to_fail.push_back(pair.first);
    }
    for (const String& quest_id : to_fail) {
        _fail(quest_id, "giver_died");
        changed = true;
    }
    return changed;
}

void QuestTracker::_fail(const String& p_quest_id, const String& p_reason) {
    auto it = instances.find(p_quest_id);
    if (it == instances.end()) return;
    QuestInstance& q = it->second;
    if (q.status != "offered" && q.status != "active") return;
    _record_failure(q);
    q.status = "failed";
    q.params["__failure_reason"] = p_reason;
    q.params["__failure_turn"] = _current_turn();
    _emit(p_quest_id);
}

void QuestTracker::_expire_due_quests() {
    const float now = _current_turn();
    std::vector<String> due;
    for (const auto& pair : instances) {
        const QuestInstance& q = pair.second;
        if (q.status == "active" && q.deadline_turn >= 0.0f && now >= q.deadline_turn) {
            due.push_back(pair.first);
        }
    }
    for (const String& quest_id : due) {
        _fail(quest_id, "timeout");
    }
}

bool QuestTracker::_has_required_items(const QuestInstance& p_q) const {
    if (!_is_gather_kind(p_q.template_kind)) return true;
    if (!ledger) return false;
    String item_id = String(p_q.params.get("item_id", ""));
    if (item_id.is_empty() || p_q.target <= 0) return false;
    return ledger->get_inventory_item_amount(player_entity_id, item_id) >= p_q.target;
}

bool QuestTracker::_remove_required_items(const QuestInstance& p_q) {
    if (!_is_gather_kind(p_q.template_kind)) return true;
    if (!ledger) return false;
    String item_id = String(p_q.params.get("item_id", ""));
    if (item_id.is_empty() || p_q.target <= 0) return false;
    if (ledger->get_inventory_item_amount(player_entity_id, item_id) < p_q.target) return false;
    return ledger->remove_inventory_item(player_entity_id, item_id, p_q.target);
}

bool QuestTracker::can_complete(const String& p_quest_id) {
    _expire_due_quests();
    auto it = instances.find(p_quest_id);
    if (it == instances.end()) return false;
    const QuestInstance& q = it->second;
    if (q.status != "active") return false;
    if (!_is_gather_kind(q.template_kind) && q.progress < q.target) return false;
    return _has_required_items(q);
}

bool QuestTracker::complete(const String& p_quest_id) {
    _expire_due_quests();
    auto it = instances.find(p_quest_id);
    if (it == instances.end()) return false;
    QuestInstance& q = it->second;
    if (q.status != "active") return false;
    if (!_is_gather_kind(q.template_kind) && q.progress < q.target) return false;
    if (!_remove_required_items(q)) return false;
    _mark_completed(p_quest_id);
    return true;
}

void QuestTracker::_advance(const String& p_quest_id, int p_delta) {
    auto it = instances.find(p_quest_id);
    if (it == instances.end()) return;
    if (it->second.status != "active") return;
    it->second.progress += p_delta;
    if (it->second.progress > it->second.target) {
        it->second.progress = it->second.target;
    }
    _emit(p_quest_id);
}

void QuestTracker::_mark_completed(const String& p_quest_id) {
    auto it = instances.find(p_quest_id);
    if (it == instances.end()) return;
    if (it->second.status == "completed") return;
    it->second.status = "completed";
    it->second.progress = it->second.target;
    _apply_rewards(it->second);
    _emit(p_quest_id);
}

void QuestTracker::_apply_rewards(const QuestInstance& p_q) {
    if (!ledger) return;
    Array items = p_q.rewards.get("items", Array());
    for (int i = 0; i < items.size(); i++) {
        Dictionary entry = items[i];
        String id = String(entry.get("id", ""));
        int amount = int(entry.get("amount", 0));
        if (id.is_empty() || amount <= 0) continue;
        ledger->add_inventory_item(player_entity_id, id, amount);
    }
    int df = int(p_q.rewards.get("friendship_delta", 0));
    int dr = int(p_q.rewards.get("romance_delta", 0));
    if (p_q.giver_entity_id != 0 && (df != 0 || dr != 0)) {
        if (ledger->has_relationship(p_q.giver_entity_id)) {
            int f = ledger->get_friendship(p_q.giver_entity_id);
            int r = ledger->get_romance(p_q.giver_entity_id);
            ledger->set_friendship(p_q.giver_entity_id, f + df);
            ledger->set_romance(p_q.giver_entity_id, r + dr);
        }
    }
}

Dictionary QuestTracker::_view(const String& p_quest_id, const QuestInstance& p_q) const {
    Dictionary d;
    d["quest_id"]        = p_quest_id;
    d["kind"]            = p_q.template_kind;
    d["label"]           = String(p_q.params.get("__label", ""));
    d["description"]     = String(p_q.params.get("__description", ""));
    d["giver_entity_id"] = (int)p_q.giver_entity_id;
    d["status"]          = p_q.status;
    d["target"]          = p_q.target;
    d["progress"]        = p_q.progress;
    d["started_turn"]    = p_q.started_turn;
    d["deadline_turn"]   = p_q.deadline_turn;
    int time_remaining = 0;
    if (p_q.status == "active" && p_q.deadline_turn >= 0.0f) {
        time_remaining = static_cast<int>(std::ceil(p_q.deadline_turn - _current_turn()));
        if (time_remaining < 0) time_remaining = 0;
    }
    d["time_remaining_turns"] = time_remaining;
    const bool deadline_passed = p_q.status == "active" &&
        p_q.deadline_turn >= 0.0f && _current_turn() >= p_q.deadline_turn;
    bool objective_ready = _is_gather_kind(p_q.template_kind) ? _has_required_items(p_q) : p_q.progress >= p_q.target;
    d["can_complete"]    = p_q.status == "active" && !deadline_passed && objective_ready;
    d["params"]          = p_q.params;
    d["rewards"]         = p_q.rewards;
    if (db) {
        const String location_context = db->get_location_context(p_q.template_kind);
        if (!location_context.is_empty()) d["location_context"] = location_context;
    }
    if (db && db->is_story_kind(p_q.template_kind)) {
        d["story_id"] = p_q.template_kind;
        d["objective_kind"] = db->get_objective_kind(p_q.template_kind);
        d["prerequisite_quest"] = db->get_prerequisite_quest(p_q.template_kind);
        d["next_quest"] = db->get_next_quest(p_q.template_kind);
        d["next_giver"] = db->get_next_giver(p_q.template_kind);
    }
    return d;
}

Array QuestTracker::get_offers_for(uint32_t p_giver_entity_id) {
    _expire_due_quests();
    Array out;
    for (const auto& pair : instances) {
        if (pair.second.giver_entity_id != p_giver_entity_id) continue;
        if (pair.second.status != "offered") continue;
        out.push_back(_view(pair.first, pair.second));
    }
    return out;
}

Array QuestTracker::get_active() {
    _expire_due_quests();
    Array out;
    for (const auto& pair : instances) {
        if (pair.second.status == "active") out.push_back(_view(pair.first, pair.second));
    }
    return out;
}

Array QuestTracker::get_completed() {
    _expire_due_quests();
    Array out;
    for (const auto& pair : instances) {
        if (pair.second.status == "completed") out.push_back(_view(pair.first, pair.second));
    }
    return out;
}

Array QuestTracker::get_offered() {
    _expire_due_quests();
    Array out;
    for (const auto& pair : instances) {
        if (pair.second.status == "offered") out.push_back(_view(pair.first, pair.second));
    }
    return out;
}

Dictionary QuestTracker::get_quest(const String& p_quest_id) {
    _expire_due_quests();
    auto it = instances.find(p_quest_id);
    if (it == instances.end()) return Dictionary();
    return _view(it->first, it->second);
}

bool QuestTracker::is_completed(const String& p_quest_id) {
    _expire_due_quests();
    auto it = instances.find(p_quest_id);
    return it != instances.end() && it->second.status == "completed";
}

bool QuestTracker::has_failed(uint32_t p_giver_entity_id, const String& p_quest_ref) {
    _expire_due_quests();
    if (p_quest_ref.is_empty()) return false;

    auto instance_it = instances.find(p_quest_ref);
    if (instance_it != instances.end() &&
        instance_it->second.status == "failed" &&
        instance_it->second.giver_entity_id == p_giver_entity_id) {
        return true;
    }

    const QuestFailureHistory* history = _get_failure_history(p_giver_entity_id, p_quest_ref);
    if (history) return history->ever_failed;
    return false;
}

bool QuestTracker::can_offer(uint32_t p_giver_entity_id, const String& p_kind) {
    _expire_due_quests();
    if (!db || p_kind.is_empty()) return false;
    if (!_giver_can_offer(p_kind, p_giver_entity_id)) return false;
    for (const auto& pair : instances) {
        const QuestInstance& q = pair.second;
        if (q.giver_entity_id != p_giver_entity_id || q.template_kind != p_kind) continue;
        if (q.status == "offered" || q.status == "active") return false;
        if (q.status == "completed" && db->is_story_kind(p_kind)) return false;
    }
    return _failure_allows_offer(p_giver_entity_id, p_kind);
}

Dictionary QuestTracker::serialize() const {
    Dictionary data;
    data["version"] = 1;
    Array arr;
    for (const auto& pair : instances) {
        arr.push_back(_view(pair.first, pair.second));
    }
    data["instances"] = arr;

    Array failures;
    for (const auto& pair : failure_history) {
        const QuestFailureHistory& history = pair.second;
        Dictionary entry;
        entry["kind"] = history.template_kind;
        entry["giver_entity_id"] = static_cast<int64_t>(history.giver_entity_id);
        entry["ever_failed"] = history.ever_failed;
        entry["last_failed_turn"] = history.last_failed_turn;
        entry["failure_count"] = history.failure_count;
        failures.push_back(entry);
    }
    data["failure_history"] = failures;
    return data;
}

void QuestTracker::deserialize(const Dictionary& p_data) {
    instances.clear();
    failure_history.clear();
    if (p_data.is_empty()) return;

    Array failures = p_data.get("failure_history", Array());
    for (int i = 0; i < failures.size(); i++) {
        Dictionary d = failures[i];
        const String kind = String(d.get("kind", ""));
        if (kind.is_empty()) continue;
        QuestFailureHistory history;
        history.template_kind = kind;
        history.giver_entity_id = static_cast<uint32_t>(static_cast<int64_t>(d.get("giver_entity_id", 0)));
        history.ever_failed = bool(d.get("ever_failed", true));
        history.last_failed_turn = static_cast<float>(static_cast<double>(d.get("last_failed_turn", -1.0)));
        history.failure_count = int(d.get("failure_count", 1));
        if (history.failure_count <= 0) history.failure_count = 1;
        if (history.failure_count > 0) history.ever_failed = true;
        failure_history[_failure_key(history.giver_entity_id, history.template_kind)] = history;
    }

    Array arr = p_data.get("instances", Array());
    for (int i = 0; i < arr.size(); i++) {
        Dictionary d = arr[i];
        String id = String(d.get("quest_id", ""));
        if (id.is_empty()) continue;
        QuestInstance q;
        q.id               = id;
        q.template_kind    = String(d.get("kind", ""));
        q.status           = String(d.get("status", "offered"));
        q.giver_entity_id  = (uint32_t)int(d.get("giver_entity_id", 0));
        q.target           = int(d.get("target", 0));
        q.progress         = int(d.get("progress", 0));
        q.started_turn     = int(d.get("started_turn", -1));
        q.deadline_turn    = static_cast<float>(static_cast<double>(d.get("deadline_turn", -1.0)));
        q.params           = d.get("params", Dictionary());
        q.rewards          = d.get("rewards", Dictionary());
        instances[id] = q;
    }
    _expire_due_quests();
}

}
