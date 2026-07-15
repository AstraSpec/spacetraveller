#ifndef SPACETRAVELLER_FACTION_H
#define SPACETRAVELLER_FACTION_H

#include <godot_cpp/variant/string.hpp>

namespace godot {

enum class FactionRelation {
    ALLIED,
    NEUTRAL,
    HOSTILE
};

namespace Faction {
    inline FactionRelation relation_from_string(const String& value) {
        const String normalized = value.to_lower();
        if (normalized == "allied" || normalized == "friendly") return FactionRelation::ALLIED;
        if (normalized == "hostile") return FactionRelation::HOSTILE;
        return FactionRelation::NEUTRAL;
    }

    inline String relation_to_string(FactionRelation value) {
        switch (value) {
            case FactionRelation::ALLIED: return "allied";
            case FactionRelation::HOSTILE: return "hostile";
            case FactionRelation::NEUTRAL: break;
        }
        return "neutral";
    }

    inline String relation_to_attitude_string(FactionRelation value) {
        return value == FactionRelation::ALLIED ? String("friendly") : relation_to_string(value);
    }
}

}

#endif // SPACETRAVELLER_FACTION_H
