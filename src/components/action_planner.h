#ifndef SPACETRAVELLER_ACTION_PLANNER_H
#define SPACETRAVELLER_ACTION_PLANNER_H

#include "components/action_resolver.h"
#include "core/faction.h"
#include "world/world_bubble.h"
#include "data/tile_db.h"
#include "core/tag_registry.h"
#include <cstdint>

namespace godot {

struct ActionPlan {
    Intent intent;
    uint32_t interact_target = 0;
    bool should_interact = false;
};

namespace ActionPlanner {
    ActionPlan plan_player_intent(
        const Intent& raw_intent,
        WorldBubble& bubble,
        uint32_t player_id,
        const String& player_faction,
        const String& target_faction
    );
}

}

#endif // SPACETRAVELLER_ACTION_PLANNER_H
