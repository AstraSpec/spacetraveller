#include "ai_controller.h"
#include "locomotion.h"
#include "entities/entity.h"
#include "world/world_bubble.h"
#include "data/tile_db.h"
#include "path/path_result.h"
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

static Vector2i pick_wander_target(const AIData& ai, const Entity& self,
                                    WorldBubble& bubble, const TileDb& tile_db) {
    for (int attempt = 0; attempt < 20; attempt++) {
        int ox = UtilityFunctions::randi_range(-static_cast<int>(ai.wander_radius),
                                                static_cast<int>(ai.wander_radius));
        int oy = UtilityFunctions::randi_range(-static_cast<int>(ai.wander_radius),
                                                static_cast<int>(ai.wander_radius));
        int tx = ai.wander_center.x + ox;
        int ty = ai.wander_center.y + oy;
        if (tx == self.x && ty == self.y) continue;

        uint16_t tile_id = bubble.query_tile_id(tx, ty);
        if (tile_id == 0) return Vector2i(tx, ty);
        const TileInfo* info = tile_db.get_tile_info(tile_id);
        if (info && !info->solid) return Vector2i(tx, ty);
    }
    return Vector2i(self.x, self.y);
}

static bool is_adjacent(const Vector2i& a, const Vector2i& b) {
    int dx = abs(a.x - b.x);
    int dy = abs(a.y - b.y);
    return dx <= 1 && dy <= 1 && (dx + dy) > 0;
}

AIState AIController::state_from_string(const String& value, AIState fallback) {
    String normalized = value.to_lower();
    if (normalized == "wander") return AIState::WANDER;
    if (normalized == "chase") return AIState::CHASE;
    if (normalized == "idle") return AIState::IDLE;
    return fallback;
}

String AIController::state_to_string(AIState value) {
    switch (value) {
        case AIState::WANDER: return "wander";
        case AIState::CHASE: return "chase";
        case AIState::IDLE: return "idle";
    }
    return "wander";
}

Intent AIController::tick(AIData& ai, LocomotionData& loco, const AIContext& ctx) {
    if (ai.perception_tier == PerceptionTier::NONE) {
        return Intent{IntentType::NONE};
    }

    if (ai.state == AIState::IDLE) {
        Locomotion::clear_path(loco);
        return Intent{IntentType::NONE};
    }

    if (ctx.perception.player_seen) {
        ai.state = AIState::CHASE;
    } else if (ai.state == AIState::CHASE) {
        Vector2i pos(ctx.self.x, ctx.self.y);
        if (is_adjacent(pos, ctx.perception.last_known_player_pos) ||
            (!Locomotion::has_path(loco) && ai.stuck_counter > 3)) {
            ai.state = AIState::WANDER;
            ai.wander_center = pos;
            ai.stuck_counter = 0;
        }
    }

    Vector2i next_tile;
    if (Locomotion::peek_next_step(loco, next_tile)) {
        bool path_stale = false;
        if (ai.state == AIState::CHASE && ctx.perception.player_seen) {
            if (loco.path_goal != ctx.player_pos) {
                path_stale = true;
            }
        }
        if (!path_stale && is_adjacent(Vector2i(ctx.self.x, ctx.self.y), next_tile)) {
            return Intent{IntentType::MOVE, next_tile};
        }
        Locomotion::clear_path(loco);
    }

    if (ai.state == AIState::WANDER) {
        if (ai.wander_cooldown > 0) {
            ai.wander_cooldown--;
            return Intent{IntentType::NONE};
        }

        Vector2i target = pick_wander_target(ai, ctx.self, ctx.bubble, ctx.tile_db);
        if (target.x == ctx.self.x && target.y == ctx.self.y) {
            ai.wander_cooldown = 10;
            return Intent{IntentType::NONE};
        }

        PathResult result = ctx.find_path(Vector2i(ctx.self.x, ctx.self.y), target);
        if (result.found && result.waypoints.size() >= 1) {
            Locomotion::set_path(loco, result.waypoints, target);

            if (Locomotion::peek_next_step(loco, next_tile)) {
                return Intent{IntentType::MOVE, next_tile};
            }
        }

        ai.wander_cooldown = 5 + ai.stuck_counter * 2;
        ai.stuck_counter++;
        return Intent{IntentType::NONE};
    }

    // CHASE state
    Vector2i target_pos = ctx.perception.player_seen
        ? ctx.player_pos
        : ctx.perception.last_known_player_pos;

    if (is_adjacent(Vector2i(ctx.self.x, ctx.self.y), target_pos)) {
        ai.stuck_counter = 0;
        return Intent{IntentType::MOVE, target_pos};
    }

    PathResult result = ctx.find_path(Vector2i(ctx.self.x, ctx.self.y), target_pos);
    if (result.found && result.waypoints.size() >= 1) {
        Locomotion::set_path(loco, result.waypoints, target_pos);

        if (Locomotion::peek_next_step(loco, next_tile)) {
            return Intent{IntentType::MOVE, next_tile};
        }
    }

    ai.stuck_counter++;
    return Intent{IntentType::NONE};
}

Dictionary AIController::serialize(const AIData& data) {
    Dictionary d;
    d["state"] = static_cast<int>(data.state);
    d["state_name"] = state_to_string(data.state);
    d["attitude"] = data.attitude;
    d["role"] = data.role;
    d["perception_tier"] = static_cast<int>(data.perception_tier);
    d["wander_center_x"] = data.wander_center.x;
    d["wander_center_y"] = data.wander_center.y;
    d["wander_radius"] = data.wander_radius;
    d["wander_cooldown"] = data.wander_cooldown;
    d["stuck_counter"] = data.stuck_counter;
    return d;
}

void AIController::deserialize(AIData& data, const Dictionary& dict) {
    if (dict.has("state_name")) {
        data.state = state_from_string(String(dict.get("state_name", "")), AIState::WANDER);
    } else {
        data.state = static_cast<AIState>(static_cast<int>(dict.get("state", 0)));
    }
    data.attitude = String(dict.get("attitude", "neutral")).to_lower();
    data.role = String(dict.get("role", "none")).to_lower();
    data.perception_tier = static_cast<PerceptionTier>(static_cast<int>(dict.get("perception_tier", 0)));
    data.wander_center = Vector2i(
        static_cast<int>(dict.get("wander_center_x", 0)),
        static_cast<int>(dict.get("wander_center_y", 0))
    );
    data.wander_radius = static_cast<float>(static_cast<double>(dict.get("wander_radius", 10.0)));
    data.wander_cooldown = static_cast<int>(dict.get("wander_cooldown", 0));
    data.stuck_counter = static_cast<int>(dict.get("stuck_counter", 0));
}
