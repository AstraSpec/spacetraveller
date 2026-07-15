#include "allegiance.h"
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <godot_cpp/variant/array.hpp>
#include <algorithm>
#include <limits>
#include <vector>

namespace godot {

EntityRelation Allegiance::relation_from_string(const String& value) {
    const String normalized = value.to_lower();
    if (normalized == "friendly" || normalized == "allied") return EntityRelation::FRIENDLY;
    if (normalized == "hostile") return EntityRelation::HOSTILE;
    return EntityRelation::NEUTRAL;
}

String Allegiance::relation_to_string(EntityRelation value) {
    switch (value) {
        case EntityRelation::FRIENDLY: return "friendly";
        case EntityRelation::HOSTILE: return "hostile";
        case EntityRelation::NEUTRAL: break;
    }
    return "neutral";
}

Dictionary Allegiance::serialize(const AllegianceData& data) {
    Dictionary result;
    result["faction"] = data.faction_id;

    std::vector<uint32_t> targets;
    targets.reserve(data.personal_relations.size());
    for (const auto& pair : data.personal_relations) {
        if (pair.second != EntityRelation::NEUTRAL) targets.push_back(pair.first);
    }
    std::sort(targets.begin(), targets.end());

    Array relations;
    for (uint32_t target_id : targets) {
        Dictionary entry;
        entry["target_id"] = static_cast<int64_t>(target_id);
        entry["relation"] = relation_to_string(data.personal_relations.at(target_id));
        relations.push_back(entry);
    }
    if (!relations.is_empty()) result["relations"] = relations;
    return result;
}

void Allegiance::deserialize(AllegianceData& data, const Dictionary& dict) {
    data.faction_id = String(dict.get("faction", "unaffiliated")).to_lower();
    if (data.faction_id.is_empty()) data.faction_id = "unaffiliated";
    data.personal_relations.clear();

    Variant relations_var = dict.get("relations", Array());
    if (relations_var.get_type() != Variant::ARRAY) return;
    Array relations = relations_var;
    for (int i = 0; i < relations.size(); ++i) {
        if (relations[i].get_type() != Variant::DICTIONARY) continue;
        Dictionary entry = relations[i];
        const int64_t target = static_cast<int64_t>(entry.get("target_id", static_cast<int64_t>(-1)));
        if (target < 0 || target > static_cast<int64_t>(std::numeric_limits<uint32_t>::max())) continue;
        const EntityRelation relation = relation_from_string(String(entry.get("relation", "neutral")));
        if (relation != EntityRelation::NEUTRAL) {
            data.personal_relations[static_cast<uint32_t>(target)] = relation;
        }
    }
}

}
