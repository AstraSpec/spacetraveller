#ifndef SPACETRAVELLER_ENTITY_FACTORY_H
#define SPACETRAVELLER_ENTITY_FACTORY_H

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/array.hpp>
#include <cstdint>

namespace godot {

class EntityLedger;
class EntityTracker;
class WorldBubble;
class TurnScheduler;

namespace EntityFactory {
    struct SpawnOverrides {
        String job;
        String dialogue_id;
        String faction;
        String reaction_policy;
        int reaction_radius = 0;
        String ai_state;
        Array traits;
        Array context_tags;
    };

    uint32_t create_npc(const String& race_id, const Vector2i& pos, int world_seed,
                        EntityLedger& ledger, EntityTracker& tracker, WorldBubble& bubble, TurnScheduler& scheduler);
    uint32_t create_npc(const String& race_id, const Vector2i& pos, int world_seed,
                        EntityLedger& ledger, EntityTracker& tracker, WorldBubble& bubble, TurnScheduler& scheduler,
                        const SpawnOverrides& overrides,
                        float p_initial_turn_time = 0.0f);
    uint32_t create_player(const String& race_id, const Vector2i& pos,
                        EntityLedger& ledger, EntityTracker& tracker, WorldBubble& bubble, TurnScheduler& scheduler);
}

}

#endif // SPACETRAVELLER_ENTITY_FACTORY_H
