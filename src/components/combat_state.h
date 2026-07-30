#ifndef SPACETRAVELLER_COMBAT_STATE_H
#define SPACETRAVELLER_COMBAT_STATE_H

#include <godot_cpp/variant/dictionary.hpp>

namespace godot {

struct CombatStateData {
    int successive_defenses = 0;
};

namespace CombatState {
    void reset_defenses(CombatStateData& data);
    void record_defense(CombatStateData& data);
    Dictionary serialize(const CombatStateData& data);
    void deserialize(CombatStateData& data, const Dictionary& dict);
}

}

#endif
