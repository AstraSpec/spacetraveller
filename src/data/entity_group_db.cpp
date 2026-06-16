#include "entity_group_db.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

template<> EntityGroupDb* DataBase<EntityGroupInfo, EntityGroupDb>::singleton = nullptr;

void EntityGroupDb::_bind_methods() {
    ClassDB::bind_static_method("EntityGroupDb", D_METHOD("get_singleton"), &EntityGroupDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &EntityGroupDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_ids"), &EntityGroupDb::get_ids);
    ClassDB::bind_method(D_METHOD("get_atlas_coords", "id"), &EntityGroupDb::get_atlas_coords);
}

EntityGroupDb::EntityGroupDb() {}
EntityGroupDb::~EntityGroupDb() {}

EntityGroupInfo EntityGroupDb::_parse_row(const Dictionary &p_data) {
    EntityGroupInfo info;
    info.id = String(p_data.get("id", ""));

    Array entries = p_data.get("entries", Array());
    for (int i = 0; i < entries.size(); i++) {
        if (entries[i].get_type() != Variant::DICTIONARY) continue;
        Dictionary entry_data = entries[i];

        EntityGroupEntry entry;
        entry.entity = String(entry_data.get("entity", entry_data.get("race_id", "")));
        entry.none = bool(entry_data.get("none", false));
        if (entry.none && !entry.entity.is_empty()) {
            UtilityFunctions::push_error("[EntityGroupDb] Entry in entity group ", info.id, " cannot combine none with entity");
            continue;
        }
        if (!entry.none && entry.entity.is_empty()) {
            UtilityFunctions::push_error("[EntityGroupDb] Entry in entity group ", info.id, " has neither entity nor none");
            continue;
        }

        entry.weight = static_cast<int>(entry_data.get("weight", 1));
        if (entry.weight <= 0) continue;

        entry.job = String(entry_data.get("job", ""));
        entry.dialogue_profile = String(entry_data.get("dialogue_profile", ""));
        entry.attitude = String(entry_data.get("attitude", ""));
        entry.ai_state = String(entry_data.get("ai_state", ""));

        info.total_weight += entry.weight;
        entry.cumulative_weight = info.total_weight;
        info.entries.push_back(entry);
    }

    if (info.entries.empty() || info.total_weight <= 0) {
        UtilityFunctions::push_error("[EntityGroupDb] Entity group has no rollable entries: ", info.id);
    }

    return info;
}

const EntityGroupInfo* EntityGroupDb::get_entity_group(const String &p_id) const {
    return get_info(p_id);
}

Vector2i EntityGroupDb::get_atlas_coords(const String &) const {
    return Vector2i(72, 18);
}

const EntityGroupEntry* EntityGroupDb::pick_weighted_entry(const String &p_id, Rng::Seeded &p_rng) const {
    const EntityGroupInfo* group = get_entity_group(p_id);
    if (!group || group->entries.empty() || group->total_weight <= 0) return nullptr;

    int roll = p_rng.range(1, group->total_weight);
    for (const EntityGroupEntry& entry : group->entries) {
        if (entry.weight <= 0) continue;
        if (roll <= entry.cumulative_weight) return &entry;
    }
    return &group->entries.front();
}

}
