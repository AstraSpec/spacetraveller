#include "quest_tracker.h"
#include "entities/entity_ledger.h"
#include "data/quest_db.h"
#include "data/item_db.h"
#include "data/race_db.h"
#include "data/tile_db.h"
#include "core/id_registry.h"
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

static inline int _randi_index(int p_size) {
    if (p_size <= 0) return 0;
    return (int)((uint32_t)UtilityFunctions::randi() % (uint32_t)p_size);
}

void QuestTracker::configure(EntityLedger* p_ledger, QuestDb* p_db, uint32_t p_player_id) {
    ledger = p_ledger;
    db = p_db;
    player_entity_id = p_player_id;
}

void QuestTracker::set_emit_callback(EmitFn p_emit, void* p_userdata) {
    emit_quest_updated = p_emit;
    emit_userdata = p_userdata;
}

void QuestTracker::_emit(const String& p_quest_id) {
    if (emit_quest_updated) emit_quest_updated(emit_userdata, p_quest_id);
}

void QuestTracker::on_game_event(const GameEvent& p_event) {
    switch (p_event.type) {
        case GameEventType::ENTITY_KILLED: {
            if (p_event.subject_id != player_entity_id) return;
            if (!ledger) return;
            Dictionary anatomy = ledger->get_anatomy(p_event.target_id);
            if (anatomy.is_empty()) return;
            String race_id = String(anatomy.get("race_id", ""));
            if (race_id.is_empty()) return;
            for (auto& pair : instances) {
                QuestInstance& q = pair.second;
                if (q.status != "active") continue;
                if (q.template_kind != "kill") continue;
                if (String(q.params.get("race_id", "")) != race_id) continue;
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
                if (q.template_kind != "gather") continue;
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
                if (q.template_kind != "reach") continue;
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
    inst.target = (int)UtilityFunctions::randi_range(target_min, target_max);

    Array tier_names = db->get_tier_names(p_kind);
    if (tier_names.is_empty()) {
        inst.params["__unfilled"] = true;
        return inst;
    }
    int tier_idx = _randi_index(tier_names.size());
    String tier = String(tier_names[tier_idx]);

    int amt_min = 1, amt_max = 1;
    db->get_tier_amount_range_vec(p_kind, tier, amt_min, amt_max);
    int reward_amount = (int)UtilityFunctions::randi_range(amt_min, amt_max);

    std::vector<uint16_t> pool_vec;
    db->get_tier_item_pool_vec(p_kind, tier, pool_vec);
    String reward_item_id;
    if (!pool_vec.empty()) {
        uint16_t pick = pool_vec[_randi_index((int)pool_vec.size())];
        IdRegistry* reg = IdRegistry::get_singleton();
        reward_item_id = reg ? reg->get_string(pick) : "";
    }

    int scale = inst.target / 3;
    if (scale < 1) scale = 1;
    if (scale > 3) scale = 3;
    int friendship_delta = db->get_tier_friendship(p_kind, tier) * scale;
    int romance_delta    = db->get_tier_romance(p_kind, tier) * scale;

    if (p_kind == "gather") {
        Array type_filter = db->get_item_type_filter(p_kind);
        std::vector<String> candidates;
        ItemDb* idb = ItemDb::get_singleton();
        if (idb) {
            Array ids = idb->get_ids();
            for (int i = 0; i < ids.size(); i++) {
                String id = String(ids[i]);
                if (type_filter.is_empty()) {
                    candidates.push_back(id);
                } else {
                    String t = idb->get_item_type(id);
                    for (int j = 0; j < type_filter.size(); j++) {
                        if (t == String(type_filter[j])) { candidates.push_back(id); break; }
                    }
                }
            }
        }
        if (!candidates.empty()) {
            String pick = candidates[_randi_index((int)candidates.size())];
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
    } else if (p_kind == "kill") {
        Array race_exclude = db->get_race_exclude(p_kind);
        std::vector<String> candidates;
        RaceDb* race_db_singleton = RaceDb::get_singleton();
        if (race_db_singleton) {
            Array ids = race_db_singleton->get_ids();
            for (int i = 0; i < ids.size(); i++) {
                String id = String(ids[i]);
                bool excluded = false;
                for (int j = 0; j < race_exclude.size(); j++) {
                    if (id == String(race_exclude[j])) { excluded = true; break; }
                }
                if (!excluded) candidates.push_back(id);
            }
        }
        if (!candidates.empty()) {
            String pick = candidates[_randi_index((int)candidates.size())];
            inst.params["race_id"] = pick;
            String race_name = pick.capitalize();
            String label_tmpl = db->get_label_template(p_kind);
            String count_str = String::num_int64(inst.target);
            String label_after_count = label_tmpl.replace("{count}", count_str);
            inst.params["__label"] = label_after_count.replace("{race_name}", race_name);
            String desc_tmpl = db->get_description_template(p_kind);
            String desc_after_count = desc_tmpl.replace("{count}", count_str);
            inst.params["__description"] = desc_after_count.replace("{race_name}", race_name);
        } else {
            inst.params["__unfilled"] = true;
        }
    } else if (p_kind == "reach") {
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
            String pick = candidates[_randi_index((int)candidates.size())];
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

    Array reward_items;
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

Array QuestTracker::generate_offers(uint32_t p_giver_entity_id, int p_count) {
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
    auto it = instances.find(p_quest_id);
    if (it == instances.end()) return false;
    if (it->second.status != "offered") return false;
    it->second.status = "active";
    it->second.progress = 0;
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

bool QuestTracker::_has_required_items(const QuestInstance& p_q) const {
    if (p_q.template_kind != "gather") return true;
    if (!ledger) return false;
    String item_id = String(p_q.params.get("item_id", ""));
    if (item_id.is_empty() || p_q.target <= 0) return false;
    return ledger->get_inventory_item_amount(player_entity_id, item_id) >= p_q.target;
}

bool QuestTracker::_remove_required_items(const QuestInstance& p_q) {
    if (p_q.template_kind != "gather") return true;
    if (!ledger) return false;
    String item_id = String(p_q.params.get("item_id", ""));
    if (item_id.is_empty() || p_q.target <= 0) return false;
    if (ledger->get_inventory_item_amount(player_entity_id, item_id) < p_q.target) return false;
    return ledger->remove_inventory_item(player_entity_id, item_id, p_q.target);
}

bool QuestTracker::can_complete(const String& p_quest_id) const {
    auto it = instances.find(p_quest_id);
    if (it == instances.end()) return false;
    const QuestInstance& q = it->second;
    if (q.status != "active") return false;
    if (q.template_kind != "gather" && q.progress < q.target) return false;
    return _has_required_items(q);
}

bool QuestTracker::complete(const String& p_quest_id) {
    auto it = instances.find(p_quest_id);
    if (it == instances.end()) return false;
    QuestInstance& q = it->second;
    if (q.status != "active") return false;
    if (q.template_kind != "gather" && q.progress < q.target) return false;
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
    bool objective_ready = p_q.template_kind == "gather" ? _has_required_items(p_q) : p_q.progress >= p_q.target;
    d["can_complete"]    = p_q.status == "active" && objective_ready;
    d["params"]          = p_q.params;
    d["rewards"]         = p_q.rewards;
    UtilityFunctions::print("[QuestTracker] _view id=", p_quest_id, " kind=", p_q.template_kind, " status=", p_q.status);
    return d;
}

Array QuestTracker::get_offers_for(uint32_t p_giver_entity_id) const {
    Array out;
    for (const auto& pair : instances) {
        if (pair.second.giver_entity_id != p_giver_entity_id) continue;
        if (pair.second.status != "offered") continue;
        out.push_back(_view(pair.first, pair.second));
    }
    return out;
}

Array QuestTracker::get_active() const {
    Array out;
    for (const auto& pair : instances) {
        if (pair.second.status == "active") out.push_back(_view(pair.first, pair.second));
    }
    return out;
}

Array QuestTracker::get_completed() const {
    Array out;
    for (const auto& pair : instances) {
        if (pair.second.status == "completed") out.push_back(_view(pair.first, pair.second));
    }
    return out;
}

Array QuestTracker::get_offered() const {
    Array out;
    for (const auto& pair : instances) {
        if (pair.second.status == "offered") out.push_back(_view(pair.first, pair.second));
    }
    return out;
}

Dictionary QuestTracker::get_quest(const String& p_quest_id) const {
    auto it = instances.find(p_quest_id);
    if (it == instances.end()) return Dictionary();
    return _view(it->first, it->second);
}

Dictionary QuestTracker::serialize() const {
    Dictionary data;
    data["version"] = 1;
    Array arr;
    for (const auto& pair : instances) {
        arr.push_back(_view(pair.first, pair.second));
    }
    data["instances"] = arr;
    return data;
}

void QuestTracker::deserialize(const Dictionary& p_data) {
    instances.clear();
    if (p_data.is_empty()) return;
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
        q.params           = d.get("params", Dictionary());
        q.rewards          = d.get("rewards", Dictionary());
        instances[id] = q;
    }
}

}
