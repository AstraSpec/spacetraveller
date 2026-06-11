#ifndef SPACETRAVELLER_TRAVERSAL_RULES_H
#define SPACETRAVELLER_TRAVERSAL_RULES_H

#include <godot_cpp/variant/string.hpp>
#include <cstdint>

namespace godot {

class EntityLedger;

namespace TraversalRules {
    String get_entity_profile_id(uint32_t entity_id, const EntityLedger& ledger);
    String get_race_profile_id(const String& race_id);
    bool can_enter(uint32_t entity_id, uint16_t tile_id, const EntityLedger& ledger);
    bool can_race_enter(const String& race_id, uint16_t tile_id);
    bool can_profile_enter(const String& profile_id, uint16_t tile_id);
    bool can_enter_or_open(uint32_t entity_id, uint16_t tile_id, const EntityLedger& ledger);
    bool can_race_enter_or_open(const String& race_id, uint16_t tile_id);
    bool can_profile_enter_or_open(const String& profile_id, uint16_t tile_id);
    bool can_open_doors(uint32_t entity_id, const EntityLedger& ledger);
    bool can_race_open_doors(const String& race_id);
    bool can_profile_open_doors(const String& profile_id);
}

}

#endif // SPACETRAVELLER_TRAVERSAL_RULES_H
