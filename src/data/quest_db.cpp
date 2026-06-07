#include "quest_db.h"
#include "core/id_registry.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

template<> QuestDb* DataBase<QuestTemplate, QuestDb>::singleton = nullptr;

void QuestDb::_bind_methods() {
    ClassDB::bind_static_method("QuestDb", D_METHOD("get_singleton"), &QuestDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &QuestDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_ids"), &QuestDb::get_ids);
    ClassDB::bind_method(D_METHOD("get_kinds"), &QuestDb::get_kinds);
    ClassDB::bind_method(D_METHOD("get_label_template", "kind"), &QuestDb::get_label_template);
    ClassDB::bind_method(D_METHOD("get_description_template", "kind"), &QuestDb::get_description_template);
    ClassDB::bind_method(D_METHOD("get_target_range", "kind"), &QuestDb::get_target_range);
    ClassDB::bind_method(D_METHOD("get_item_type_filter", "kind"), &QuestDb::get_item_type_filter);
    ClassDB::bind_method(D_METHOD("get_target_item_pool", "kind"), &QuestDb::get_target_item_pool);
    ClassDB::bind_method(D_METHOD("get_target_item_tags", "kind"), &QuestDb::get_target_item_tags);
    ClassDB::bind_method(D_METHOD("get_target_loot_table", "kind"), &QuestDb::get_target_loot_table);
    ClassDB::bind_method(D_METHOD("get_giver_jobs", "kind"), &QuestDb::get_giver_jobs);
    ClassDB::bind_method(D_METHOD("get_race_exclude", "kind"), &QuestDb::get_race_exclude);
    ClassDB::bind_method(D_METHOD("get_tier_names", "kind"), &QuestDb::get_tier_names);
    ClassDB::bind_method(D_METHOD("get_tier_item_pool", "kind", "tier"), &QuestDb::get_tier_item_pool);
    ClassDB::bind_method(D_METHOD("get_tier_reward_loot_table", "kind", "tier"), &QuestDb::get_tier_reward_loot_table);
    ClassDB::bind_method(D_METHOD("get_tier_amount_range", "kind", "tier"), &QuestDb::get_tier_amount_range);
    ClassDB::bind_method(D_METHOD("get_tier_friendship", "kind", "tier"), &QuestDb::get_tier_friendship);
    ClassDB::bind_method(D_METHOD("get_tier_romance", "kind", "tier"), &QuestDb::get_tier_romance);
}

QuestDb::QuestDb() {}
QuestDb::~QuestDb() {}

static std::vector<uint16_t> resolve_item_ids(const Array &p_strings) {
    std::vector<uint16_t> out;
    IdRegistry* reg = IdRegistry::get_singleton();
    if (!reg) return out;
    out.reserve(p_strings.size());
    for (int i = 0; i < p_strings.size(); i++) {
        uint16_t id = reg->get_id(String(p_strings[i]));
        if (id != 0) out.push_back(id);
    }
    return out;
}

QuestTemplate QuestDb::_parse_row(const Dictionary &p_data) {
    QuestTemplate t;
    t.kind                = p_data.get("id", "");
    t.label_template      = p_data.get("label_template", "");
    t.description_template = p_data.get("description_template", "");

    // target_range: [min, max]
    Array tr = p_data.get("target_range", Array());
    for (int i = 0; i < tr.size() && i < 2; i++) {
        t.target_range.push_back(int(tr[i]));
    }
    if (t.target_range.empty()) t.target_range.push_back(1);

    // gather-only filter
    Array itf = p_data.get("item_type_filter", Array());
    for (int i = 0; i < itf.size(); i++) {
        t.item_type_filter.emplace_back(String(itf[i]));
    }

    Array target_pool = p_data.get("target_item_pool", Array());
    t.target_item_pool = resolve_item_ids(target_pool);
    t.target_item_tags = _parse_tags(p_data.get("target_item_tags", Array()));
    String target_loot_table = String(p_data.get("target_loot_table", ""));
    if (!target_loot_table.is_empty() && IdRegistry::get_singleton()) {
        t.target_loot_table = IdRegistry::get_singleton()->register_string(target_loot_table);
    }

    Array giver_jobs = p_data.get("giver_jobs", Array());
    for (int i = 0; i < giver_jobs.size(); i++) {
        t.giver_jobs.emplace_back(String(giver_jobs[i]));
    }

    // kill-only filter
    Array re = p_data.get("race_exclude", Array());
    for (int i = 0; i < re.size(); i++) {
        t.race_exclude.emplace_back(String(re[i]));
    }

    // tiers
    Dictionary tiers = p_data.get("tiers", Dictionary());
    Array tier_keys = tiers.keys();
    for (int i = 0; i < tier_keys.size(); i++) {
        String tier_name = String(tier_keys[i]);
        Variant tier_var = tiers[tier_keys[i]];
        if (tier_var.get_type() != Variant::DICTIONARY) continue;
        Dictionary tier_data = tier_var;

        QuestTier tier;
        Array pool = tier_data.get("item_pool", Array());
        tier.item_pool = resolve_item_ids(pool);
        String reward_loot_table = String(tier_data.get("reward_loot_table", ""));
        if (!reward_loot_table.is_empty() && IdRegistry::get_singleton()) {
            tier.reward_loot_table = IdRegistry::get_singleton()->register_string(reward_loot_table);
        }

        Array ar = tier_data.get("amount_range", Array());
        for (int j = 0; j < ar.size() && j < 2; j++) {
            tier.amount_range.push_back(int(ar[j]));
        }
        if (tier.amount_range.empty()) tier.amount_range.push_back(1);

        tier.friendship = int(tier_data.get("friendship", 0));
        tier.romance    = int(tier_data.get("romance", 0));

        t.tiers[tier_name] = tier;
    }

    return t;
}

Array QuestDb::get_kinds() const {
    Array arr;
    for (const auto& pair : cache) {
        arr.push_back(pair.first);
    }
    return arr;
}

String QuestDb::get_label_template(const String &p_kind) const {
    const QuestTemplate* t = get_info(p_kind);
    return t ? t->label_template : "";
}

String QuestDb::get_description_template(const String &p_kind) const {
    const QuestTemplate* t = get_info(p_kind);
    return t ? t->description_template : "";
}

Array QuestDb::get_target_range(const String &p_kind) const {
    Array arr;
    const QuestTemplate* t = get_info(p_kind);
    if (t) for (int v : t->target_range) arr.push_back(v);
    return arr;
}

Array QuestDb::get_item_type_filter(const String &p_kind) const {
    Array arr;
    const QuestTemplate* t = get_info(p_kind);
    if (t) for (const String& v : t->item_type_filter) arr.push_back(v);
    return arr;
}

Array QuestDb::get_target_item_pool(const String &p_kind) const {
    Array arr;
    const QuestTemplate* t = get_info(p_kind);
    if (t) for (uint16_t v : t->target_item_pool) arr.push_back((int)v);
    return arr;
}

Array QuestDb::get_target_item_tags(const String &p_kind) const {
    Array arr;
    const QuestTemplate* t = get_info(p_kind);
    if (t) for (uint16_t v : t->target_item_tags) arr.push_back((int)v);
    return arr;
}

String QuestDb::get_target_loot_table(const String &p_kind) const {
    const QuestTemplate* t = get_info(p_kind);
    IdRegistry* reg = IdRegistry::get_singleton();
    return (t && reg && t->target_loot_table != 0) ? reg->get_string(t->target_loot_table) : String();
}

Array QuestDb::get_giver_jobs(const String &p_kind) const {
    Array arr;
    const QuestTemplate* t = get_info(p_kind);
    if (t) for (const String& v : t->giver_jobs) arr.push_back(v);
    return arr;
}

Array QuestDb::get_race_exclude(const String &p_kind) const {
    Array arr;
    const QuestTemplate* t = get_info(p_kind);
    if (t) for (const String& v : t->race_exclude) arr.push_back(v);
    return arr;
}

Array QuestDb::get_tier_names(const String &p_kind) const {
    Array arr;
    const QuestTemplate* t = get_info(p_kind);
    if (t) for (const auto& pair : t->tiers) arr.push_back(pair.first);
    return arr;
}

Array QuestDb::get_tier_item_pool(const String &p_kind, const String &p_tier) const {
    Array arr;
    const QuestTemplate* t = get_info(p_kind);
    if (!t) return arr;
    auto it = t->tiers.find(p_tier);
    if (it == t->tiers.end()) return arr;
    for (uint16_t v : it->second.item_pool) arr.push_back((int)v);
    return arr;
}

String QuestDb::get_tier_reward_loot_table(const String &p_kind, const String &p_tier) const {
    const QuestTemplate* t = get_info(p_kind);
    if (!t) return String();
    auto it = t->tiers.find(p_tier);
    IdRegistry* reg = IdRegistry::get_singleton();
    return (it != t->tiers.end() && reg && it->second.reward_loot_table != 0) ? reg->get_string(it->second.reward_loot_table) : String();
}

Array QuestDb::get_tier_amount_range(const String &p_kind, const String &p_tier) const {
    Array arr;
    const QuestTemplate* t = get_info(p_kind);
    if (!t) return arr;
    auto it = t->tiers.find(p_tier);
    if (it == t->tiers.end()) return arr;
    for (int v : it->second.amount_range) arr.push_back((int)v);
    return arr;
}

bool QuestDb::get_tier_item_pool_vec(const String &p_kind, const String &p_tier, std::vector<uint16_t> &r_out) const {
    r_out.clear();
    const QuestTemplate* t = get_info(p_kind);
    if (!t) return false;
    auto it = t->tiers.find(p_tier);
    if (it == t->tiers.end()) return false;
    r_out = it->second.item_pool; // direct std::vector copy, no Godot types
    return !r_out.empty();
}

bool QuestDb::get_tier_amount_range_vec(const String &p_kind, const String &p_tier, int &r_min, int &r_max) const {
    r_min = 1;
    r_max = 1;
    const QuestTemplate* t = get_info(p_kind);
    if (!t) return false;
    auto it = t->tiers.find(p_tier);
    if (it == t->tiers.end()) return false;
    const auto &ar = it->second.amount_range;
    if (ar.empty()) return false;
    r_min = ar.front();
    r_max = ar.back();
    if (r_min > r_max) std::swap(r_min, r_max);
    return true;
}

bool QuestDb::get_target_item_pool_vec(const String &p_kind, std::vector<uint16_t> &r_out) const {
    r_out.clear();
    const QuestTemplate* t = get_info(p_kind);
    if (!t) return false;
    r_out = t->target_item_pool;
    return !r_out.empty();
}

bool QuestDb::get_target_item_tags_vec(const String &p_kind, std::vector<uint16_t> &r_out) const {
    r_out.clear();
    const QuestTemplate* t = get_info(p_kind);
    if (!t) return false;
    r_out = t->target_item_tags;
    return !r_out.empty();
}

uint16_t QuestDb::get_target_loot_table_id(const String &p_kind) const {
    const QuestTemplate* t = get_info(p_kind);
    return t ? t->target_loot_table : 0;
}

uint16_t QuestDb::get_tier_reward_loot_table_id(const String &p_kind, const String &p_tier) const {
    const QuestTemplate* t = get_info(p_kind);
    if (!t) return 0;
    auto it = t->tiers.find(p_tier);
    return it == t->tiers.end() ? 0 : it->second.reward_loot_table;
}

bool QuestDb::get_giver_jobs_vec(const String &p_kind, std::vector<String> &r_out) const {
    r_out.clear();
    const QuestTemplate* t = get_info(p_kind);
    if (!t) return false;
    r_out = t->giver_jobs;
    return !r_out.empty();
}

int QuestDb::get_tier_friendship(const String &p_kind, const String &p_tier) const {
    const QuestTemplate* t = get_info(p_kind);
    if (!t) return 0;
    auto it = t->tiers.find(p_tier);
    return it == t->tiers.end() ? 0 : it->second.friendship;
}

int QuestDb::get_tier_romance(const String &p_kind, const String &p_tier) const {
    const QuestTemplate* t = get_info(p_kind);
    if (!t) return 0;
    auto it = t->tiers.find(p_tier);
    return it == t->tiers.end() ? 0 : it->second.romance;
}

}
