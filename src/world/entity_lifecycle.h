#ifndef SPACETRAVELLER_ENTITY_LIFECYCLE_H
#define SPACETRAVELLER_ENTITY_LIFECYCLE_H

#include <cstdint>
#include <godot_cpp/variant/vector2i.hpp>

namespace godot {

class EntityArchive;
class EntityLedger;
class TurnScheduler;
class WorldBubble;

namespace EntityLifecycle {
    bool activate_entity(
        uint32_t entity_id,
        const Vector2i& pos,
        float initial_turn_time,
        EntityLedger& ledger,
        WorldBubble& bubble,
        TurnScheduler& scheduler
    );

    bool freeze_entity(
        uint32_t entity_id,
        EntityArchive& archive,
        EntityLedger& ledger,
        WorldBubble& bubble,
        TurnScheduler& scheduler
    );

    uint32_t thaw_entity(
        uint64_t packed_pos,
        EntityArchive& archive,
        EntityLedger& ledger,
        WorldBubble& bubble,
        TurnScheduler& scheduler,
        float minimum_turn_time
    );

    bool despawn_entity(
        uint32_t entity_id,
        EntityLedger& ledger,
        WorldBubble& bubble,
        TurnScheduler& scheduler,
        uint32_t world_seed,
        bool drop_contents = true
    );
}

}

#endif // SPACETRAVELLER_ENTITY_LIFECYCLE_H
