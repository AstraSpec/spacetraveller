#ifndef SPACETRAVELLER_FACTION_H
#define SPACETRAVELLER_FACTION_H

#include <godot_cpp/variant/string.hpp>

namespace godot {

namespace Faction {
    // Two entities are hostile when they belong to different, non-empty factions.
    inline bool are_hostile(const String& a, const String& b) {
        if (a.is_empty() || b.is_empty()) return false;
        return a != b;
    }
}

}

#endif // SPACETRAVELLER_FACTION_H
