#ifndef SPACETRAVELLER_AI_CONTROLLER_H
#define SPACETRAVELLER_AI_CONTROLLER_H

#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <cstdint>
#include <functional>
#include <limits>
#include <unordered_map>
#include "action_resolver.h"
#include "perception.h"

namespace godot {

struct LocomotionData;
struct PerceptionMemory;
class WorldBubble;
class TileDb;
struct PathResult;
struct Entity;

enum class AIState { WANDER, COMBAT, IDLE, FOLLOW, FLEE, GUARD };
enum class EntityRelation { NEUTRAL, FRIENDLY, HOSTILE };

struct AIData {
    AIState state = AIState::WANDER;
    String disposition = "neutral";
    std::unordered_map<uint32_t, EntityRelation> relations;
    uint32_t target_entity_id = std::numeric_limits<uint32_t>::max();
    PerceptionTier perception_tier = PerceptionTier::RAYCAST;
    Vector2i wander_center;
    float wander_radius = 4.0f;
    int wander_cooldown = 0;
    int stuck_counter = 0;
    // Runtime-only tracking used to invalidate a follower's path when its
    // leader moves.  The path is rebuilt after loading rather than relying on
    // this transient value being serialized.
    bool has_follow_target_position = false;
    Vector2i follow_target_position;
};

struct AIContext {
    const Entity& self;
    WorldBubble& bubble;
    const TileDb& tile_db;
    const PerceptionMemory& perception;
    const Vector2i& target_pos;
    bool has_target = false;
    const std::function<bool(Vector2i)>& can_enter;
    const std::function<PathResult(Vector2i, Vector2i)>& find_path;
    bool target_same_level = true;
};

namespace AIController {
    Intent tick(AIData& ai, LocomotionData& loco, const AIContext& ctx);
    AIState state_from_string(const String& value);
    String state_to_string(AIState value);
    bool is_valid_state_name(const String& value);
    String normalize_disposition(const String& value);
    EntityRelation relation_from_string(const String& value);
    String relation_to_string(EntityRelation value);
    Dictionary serialize(const AIData& data);
    void deserialize(AIData& data, const Dictionary& dict);
}

}

#endif // SPACETRAVELLER_AI_CONTROLLER_H
