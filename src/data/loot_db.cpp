#include "loot_db.h"
#include "core/id_registry.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>

namespace godot {

template<> LootDb* DataBase<LootTableInfo, LootDb>::singleton = nullptr;

void LootDb::_bind_methods() {
    ClassDB::bind_static_method("LootDb", D_METHOD("get_singleton"), &LootDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &LootDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_ids"), &LootDb::get_ids);
    ClassDB::bind_method(D_METHOD("get_atlas_coords", "id"), &LootDb::get_atlas_coords);
    ClassDB::bind_method(D_METHOD("roll_table", "table_id", "world_seed", "pos", "stream"), &LootDb::roll_table_for_position);
}

LootDb::LootDb() {}
LootDb::~LootDb() {}

void LootDb::initialize_data() {
    fast_cache.clear();
    DataBase<LootTableInfo, LootDb>::initialize_data("res://data/loot_tables");

    IdRegistry* reg = IdRegistry::get_singleton();
    for (const auto& pair : cache) {
        const LootTableInfo& table = pair.second;
        for (const LootEntry& entry : table.entries) {
            if (entry.table_id != 0 && !get_loot_table(entry.table_id)) {
                UtilityFunctions::push_error("[LootDb] Unknown nested table_id in loot table ", table.id, ": ", reg ? reg->get_string(entry.table_id) : String::num_int64(entry.table_id));
            }
        }
    }
}

static int clamp_non_negative_int(const Variant &p_value, int p_default) {
    int value = int(p_value);
    return value < 0 ? p_default : value;
}

LootTableInfo LootDb::_parse_row(const Dictionary &p_data) {
    LootTableInfo info;
    info.id = String(p_data.get("id", ""));
    IdRegistry* reg = IdRegistry::get_singleton();
    if (reg && !info.id.is_empty()) {
        info.numeric_id = reg->register_string(info.id);
    }

    info.rolls_min = clamp_non_negative_int(p_data.get("rolls_min", 1), 1);
    info.rolls_max = clamp_non_negative_int(p_data.get("rolls_max", info.rolls_min), info.rolls_min);
    if (info.rolls_min > info.rolls_max) std::swap(info.rolls_min, info.rolls_max);
    info.allow_duplicates = bool(p_data.get("allow_duplicates", true));

    Array entries = p_data.get("entries", Array());
    for (int i = 0; i < entries.size(); i++) {
        if (entries[i].get_type() != Variant::DICTIONARY) continue;
        Dictionary entry_data = entries[i];

        LootEntry entry;
        String item_id = String(entry_data.get("item_id", ""));
        String table_id = String(entry_data.get("table_id", ""));
        entry.none = bool(entry_data.get("none", false));
        if (entry.none && (!item_id.is_empty() || !table_id.is_empty())) {
            UtilityFunctions::push_error("[LootDb] Entry in loot table ", info.id, " cannot combine none with item_id or table_id");
            continue;
        }
        if (reg && !item_id.is_empty()) entry.item_id = reg->get_id(item_id);
        if (reg && !table_id.is_empty()) entry.table_id = reg->register_string(table_id);

        if (!item_id.is_empty() && entry.item_id == 0) {
            UtilityFunctions::push_error("[LootDb] Unknown item_id in loot table ", info.id, ": ", item_id);
            continue;
        }
        if (entry.item_id == 0 && entry.table_id == 0 && !entry.none) {
            UtilityFunctions::push_error("[LootDb] Entry in loot table ", info.id, " has neither item_id, table_id, nor none");
            continue;
        }

        entry.weight = int(entry_data.get("weight", 1));
        if (entry.weight <= 0) continue;
        entry.amount_min = int(entry_data.get("amount_min", entry_data.get("amount", 1)));
        entry.amount_max = int(entry_data.get("amount_max", entry.amount_min));
        if (entry.amount_min < 0) entry.amount_min = 0;
        if (entry.amount_max < 0) entry.amount_max = 0;
        if (entry.amount_min > entry.amount_max) std::swap(entry.amount_min, entry.amount_max);

        info.total_weight += entry.weight;
        entry.cumulative_weight = info.total_weight;
        info.entries.push_back(entry);
    }

    if (info.entries.empty() || info.total_weight <= 0) {
        UtilityFunctions::push_error("[LootDb] Loot table has no rollable entries: ", info.id);
    }

    if (info.numeric_id >= fast_cache.size()) {
        fast_cache.resize(info.numeric_id + 1);
    }
    if (info.numeric_id != 0) {
        fast_cache[info.numeric_id] = info;
    }

    return info;
}

const LootTableInfo* LootDb::get_loot_table(const String &p_id) const {
    return get_info(p_id);
}

const LootTableInfo* LootDb::get_loot_table(uint16_t p_id) const {
    if (p_id < fast_cache.size() && !fast_cache[p_id].id.is_empty()) {
        return &fast_cache[p_id];
    }
    return nullptr;
}

uint16_t LootDb::get_loot_table_id(const String &p_id) const {
    IdRegistry* reg = IdRegistry::get_singleton();
    return reg ? reg->get_id(p_id) : 0;
}

Vector2i LootDb::get_atlas_coords(const String &) const {
    return Vector2i(71, 18);
}

bool LootDb::roll_table(uint16_t p_table_id, Rng::Seeded &p_rng, std::vector<LootStack> &r_out) const {
    return _roll_table_internal(p_table_id, p_rng, r_out, 0);
}

bool LootDb::_roll_table_internal(uint16_t p_table_id, Rng::Seeded &p_rng, std::vector<LootStack> &r_out, int p_depth) const {
    if (p_depth > MAX_NESTED_DEPTH) {
        UtilityFunctions::push_error("[LootDb] Nested loot table depth exceeded; possible cycle");
        return false;
    }

    const LootTableInfo* table = get_loot_table(p_table_id);
    if (!table || table->entries.empty() || table->total_weight <= 0) return false;

    int rolls = p_rng.range(table->rolls_min, table->rolls_max);
    std::vector<bool> used_indices(table->entries.size(), false);
    int used_count = 0;
    bool produced = false;

    for (int roll_index = 0; roll_index < rolls; roll_index++) {
        if (!table->allow_duplicates && used_count >= static_cast<int>(table->entries.size())) break;

        const LootEntry* chosen = nullptr;
        int chosen_index = -1;
        for (int attempt = 0; attempt < 16; attempt++) {
            int roll = p_rng.range(1, table->total_weight);
            auto it = std::lower_bound(
                table->entries.begin(),
                table->entries.end(),
                roll,
                [](const LootEntry& entry, int value) {
                    return entry.cumulative_weight < value;
                }
            );
            if (it == table->entries.end()) continue;
            int i = static_cast<int>(it - table->entries.begin());
            if (!table->allow_duplicates && used_indices[i]) continue;
            chosen = &(*it);
            chosen_index = i;
            if (chosen) break;
        }

        if (!chosen) continue;
        if (!table->allow_duplicates) {
            used_indices[chosen_index] = true;
            used_count++;
        }

        if (chosen->table_id != 0) {
            produced = _roll_table_internal(chosen->table_id, p_rng, r_out, p_depth + 1) || produced;
        } else if (chosen->item_id != 0) {
            int amount = p_rng.range(chosen->amount_min, chosen->amount_max);
            if (amount > 0) {
                r_out.push_back({chosen->item_id, amount});
                produced = true;
            }
        } else if (chosen->none) {
            continue;
        }
    }

    return produced;
}

Array LootDb::roll_table_for_position(const String &p_table_id, int p_world_seed, const Vector2i &p_pos, int p_stream) const {
    Array arr;
    uint16_t table_id = get_loot_table_id(p_table_id);
    if (table_id == 0) return arr;

    Rng::Seeded rng = Rng::at(static_cast<uint32_t>(p_world_seed), p_pos, static_cast<Rng::Stream>(p_stream));
    std::vector<LootStack> stacks;
    roll_table(table_id, rng, stacks);

    IdRegistry* reg = IdRegistry::get_singleton();
    if (!reg) return arr;
    for (const LootStack& stack : stacks) {
        Dictionary d;
        d["id"] = reg->get_string(stack.item_id);
        d["amount"] = stack.amount;
        arr.push_back(d);
    }
    return arr;
}

}
