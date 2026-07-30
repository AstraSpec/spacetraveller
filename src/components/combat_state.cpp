#include "combat_state.h"
#include <godot_cpp/variant/variant.hpp>
#include <algorithm>

using namespace godot;

void CombatState::reset_defenses(CombatStateData& data) {
    data.successive_defenses = 0;
}

void CombatState::record_defense(CombatStateData& data) {
    ++data.successive_defenses;
}

Dictionary CombatState::serialize(const CombatStateData& data) {
    Dictionary result;
    result["successive_defenses"] = data.successive_defenses;
    return result;
}

void CombatState::deserialize(
    CombatStateData& data,
    const Dictionary& dict
) {
    data.successive_defenses = std::max(
        0,
        static_cast<int>(dict.get("successive_defenses", 0)));
}
