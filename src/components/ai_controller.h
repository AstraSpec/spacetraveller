#ifndef SPACETRAVELLER_AI_CONTROLLER_H
#define SPACETRAVELLER_AI_CONTROLLER_H

#include <godot_cpp/variant/vector2i.hpp>
#include <functional>
#include "action_resolver.h"
#include "perception.h"

namespace godot {

struct LocomotionData;
struct PerceptionMemory;
class WorldBubble;
class TileDb;
struct PathResult;
struct Entity;

enum class AIState { WANDER, CHASE };

struct AIData {
    AIState state = AIState::WANDER;
    PerceptionTier perception_tier = PerceptionTier::RAYCAST;
    Vector2i wander_center;
    float wander_radius = 4.0f;
    int wander_cooldown = 0;
    int stuck_counter = 0;
};

struct AIContext {
    const Entity& self;
    WorldBubble& bubble;
    const TileDb& tile_db;
    const PerceptionMemory& perception;
    const Vector2i& player_pos;
    const std::function<PathResult(Vector2i, Vector2i)>& find_path;
};

namespace AIController {
    Intent tick(AIData& ai, LocomotionData& loco, const AIContext& ctx);
}

}

#endif // SPACETRAVELLER_AI_CONTROLLER_H