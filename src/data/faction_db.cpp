#include "faction_db.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

template<> FactionDb* DataBase<FactionInfo, FactionDb>::singleton = nullptr;

void FactionDb::_bind_methods() {
    ClassDB::bind_static_method("FactionDb", D_METHOD("get_singleton"), &FactionDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &FactionDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_ids"), &FactionDb::get_ids);
    ClassDB::bind_method(D_METHOD("has_faction", "id"), &FactionDb::has_faction);
    ClassDB::bind_method(D_METHOD("get_relation", "a", "b"), &FactionDb::get_relation);
}

FactionInfo FactionDb::_parse_row(const Dictionary& p_data) {
    FactionInfo info;
    info.id = String(p_data.get("id", "")).to_lower();
    info.display_name = String(p_data.get("display_name", info.id.capitalize()));

    Variant relations_var = p_data.get("relations", Dictionary());
    if (relations_var.get_type() == Variant::DICTIONARY) {
        Dictionary relations = relations_var;
        Array keys = relations.keys();
        for (int i = 0; i < keys.size(); ++i) {
            const String target = String(keys[i]).to_lower();
            const String relation_name = String(relations[keys[i]]).to_lower();
            if (relation_name != "allied" && relation_name != "neutral" && relation_name != "hostile") {
                UtilityFunctions::push_error("[FactionDb] Invalid relation '", relation_name,
                    "' from ", info.id, " to ", target, ".");
                continue;
            }
            info.declared_relations[target] = Faction::relation_from_string(relation_name);
        }
    }
    return info;
}

void FactionDb::initialize_data() {
    DataBase<FactionInfo, FactionDb>::initialize_data("res://data/factions");
    rebuild_relations();
}

void FactionDb::rebuild_relations() {
    resolved_relations.clear();
    for (const auto& faction_pair : cache) {
        const String& source = faction_pair.first;
        for (const auto& relation_pair : faction_pair.second.declared_relations) {
            const String& target = relation_pair.first;
            const FactionRelation relation = relation_pair.second;
            if (!has_faction(target)) {
                UtilityFunctions::push_error("[FactionDb] Unknown target faction '", target,
                    "' referenced by ", source, ".");
                continue;
            }
            if (source == target && relation != FactionRelation::ALLIED) {
                UtilityFunctions::push_error("[FactionDb] Faction ", source,
                    " cannot declare a non-allied relation with itself.");
                continue;
            }

            const FactionInfo* target_info = get_info(target);
            if (target_info) {
                const auto reverse_declaration = target_info->declared_relations.find(source);
                if (reverse_declaration != target_info->declared_relations.end() &&
                    reverse_declaration->second != relation) {
                    UtilityFunctions::push_error("[FactionDb] Conflicting symmetric relation between ",
                        source, " and ", target, ". Both declarations were rejected.");
                    continue;
                }
            }

            auto reverse_source = resolved_relations.find(target);
            if (reverse_source != resolved_relations.end()) {
                auto reverse = reverse_source->second.find(source);
                if (reverse != reverse_source->second.end() && reverse->second != relation) {
                    UtilityFunctions::push_error("[FactionDb] Conflicting symmetric relation between ",
                        source, " and ", target, ".");
                    continue;
                }
            }
            resolved_relations[source][target] = relation;
            resolved_relations[target][source] = relation;
        }
    }
}

bool FactionDb::has_faction(const String& p_id) const {
    return cache.find(p_id.to_lower()) != cache.end();
}

FactionRelation FactionDb::get_relation_value(const String& p_a, const String& p_b) const {
    const String a = p_a.is_empty() ? String("unaffiliated") : p_a.to_lower();
    const String b = p_b.is_empty() ? String("unaffiliated") : p_b.to_lower();
    if (a == b) return FactionRelation::ALLIED;

    auto source = resolved_relations.find(a);
    if (source == resolved_relations.end()) return FactionRelation::NEUTRAL;
    auto target = source->second.find(b);
    return target == source->second.end() ? FactionRelation::NEUTRAL : target->second;
}

String FactionDb::get_relation(const String& p_a, const String& p_b) const {
    return Faction::relation_to_string(get_relation_value(p_a, p_b));
}

}
