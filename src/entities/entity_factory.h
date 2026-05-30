#ifndef SPACETRAVELLER_ENTITY_FACTORY_H
#define SPACETRAVELLER_ENTITY_FACTORY_H

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <cstdint>

namespace godot {

class EntityLedger;
class WorldBubble;
class TurnScheduler;

namespace EntityFactory {
    uint32_t create_npc(const String& race_id, const Vector2i& pos, int world_seed,
                        EntityLedger& ledger, WorldBubble& bubble, TurnScheduler& scheduler);
    uint32_t create_player(const String& race_id, const Vector2i& pos,
                        EntityLedger& ledger, WorldBubble& bubble, TurnScheduler& scheduler);
}

}

#endif // SPACETRAVELLER_ENTITY_FACTORY_H
