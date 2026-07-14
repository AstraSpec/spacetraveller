#include "npc_turn_processor.h"

#include "simulation_director.h"

#include "path/path_request.h"
#include "path/path_result.h"
#include "data/tile_db.h"
#include "components/action_resolver.h"
#include "components/ai_controller.h"
#include "components/perception.h"
#include "components/locomotion.h"
#include "components/effects.h"
#include "world/traversal_rules.h"
#include "core/tag_registry.h"
#include "entities/entity_tracker.h"

#include <functional>

using namespace godot;

namespace {

class ScopedBubbleZ {
public:
    ScopedBubbleZ(WorldBubble& p_bubble, int p_z) : bubble(p_bubble), previous_z(p_bubble.get_active_z()) {
        bubble.set_active_z(p_z);
    }

    ~ScopedBubbleZ() {
        bubble.set_active_z(previous_z);
    }

private:
    WorldBubble& bubble;
    int previous_z = 0;
};

bool is_openable_tile(WorldBubble& bubble, const Vector2i& pos) {
    TileDb* tile_db = TileDb::get_singleton();
    TagRegistry* tag_reg = TagRegistry::get_singleton();
    if (!tile_db || !tag_reg) return false;

    const uint16_t tile_id = bubble.query_tile_id(pos.x, pos.y);
    const TileInfo* info = tile_db->get_tile_info(tile_id);
    const uint16_t can_open = tag_reg->get_tag_id("CAN_OPEN");
    return info && info->opens_to != 0 && can_open != 0 && tile_db->has_tag(tile_id, can_open);
}

}

float NpcTurnProcessor::resolve_move(
    uint32_t entity_id,
    Entity& entity,
    const Intent& intent,
    LocomotionData& loco,
    AIData& ai,
    std::vector<Vector2i>& blocking_positions,
    SimulationDirector& director
) {
    ActionResult move_result = ActionResolver::resolve_move(intent, entity, *director.d.bubble, loco, director.d.ledger, director.d.tracker);
    if (move_result.success && move_result.cost > 0.0f) {
        if (ai.state == AIState::FOLLOW) {
            ai.stuck_counter = 0;
        }
        float cost = director.movement_action_cost(entity_id, move_result.cost, loco);
        for (auto& bp : blocking_positions) {
            Vector2i old_pos(entity.x - (intent.target.x - entity.x),
                             entity.y - (intent.target.y - entity.y));
            if (bp == old_pos) {
                bp = Vector2i(entity.x, entity.y);
                break;
            }
        }
        return cost;
    }

    Locomotion::clear_path(loco);
    ai.stuck_counter++;
    return 0.0f;
}

void NpcTurnProcessor::run_turn(
    uint32_t entity_id,
    EntityPool& pool,
    TileDb& tile_db,
    SimulationDirector& director
) {
    Entity* entity = pool.get_entity(entity_id);
    if (!entity) return;

    ScopedBubbleZ scoped_z(*director.d.bubble, entity->z);

    float base_time = entity->next_turn_time;

    EffectsData* effects = director.d.ledger->try_get_effects(entity_id);
    if (effects && Effects::is_stunned(*effects)) {
        float wait = EffectTuning::STUN_WAIT_STEP;
        director.advance_entity_time(entity_id, wait);
        if (!pool.get_entity(entity_id)) return;
        entity->next_turn_time = base_time + wait;
        director.d.scheduler->push(entity_id, entity->next_turn_time);
        return;
    }

    LocomotionData* loco = director.d.ledger->try_get_locomotion(entity_id);
    if (!loco) return;

    PerceptionMemory* mem = director.d.ledger->try_get_perception(entity_id);
    AIData* ai = director.d.ledger->try_get_ai(entity_id);
    if (!mem || !ai) return;

    std::vector<Vector2i> blocking_positions;
    std::vector<uint32_t> blocking_ids;
    const int block_radius = director.d.bubble->get_active_radius();
    if (director.d.tracker) {
        director.d.tracker->query_rect(
            Vector2i(entity->x - block_radius, entity->y - block_radius),
            Vector2i(entity->x + block_radius, entity->y + block_radius),
            blocking_ids,
            entity->z
        );
    } else {
        blocking_ids = pool.get_live_ids();
    }

    blocking_positions.reserve(blocking_ids.size());
    for (uint32_t id : blocking_ids) {
        if (id == entity_id) continue;
        const Entity* blocker = pool.get_entity(id);
        if (!blocker || blocker->z != entity->z) continue;
        blocking_positions.push_back(Vector2i(blocker->x, blocker->y));
    }

    int acquire_radius = director.d.bubble->get_active_radius();
    if (ai->state == AIState::FOLLOW && ai->follow_leader_id == EntityPool::INVALID_ID) {
        // Migrate older saves that only stored the follow target.
        ai->follow_leader_id = ai->target_entity_id;
    }
    const bool state_requires_target = ai->state == AIState::COMBAT || ai->state == AIState::FOLLOW || ai->state == AIState::FLEE;
    uint32_t target_id = state_requires_target ? ai->target_entity_id : EntityPool::INVALID_ID;
    Entity* target_entity = (target_id != EntityPool::INVALID_ID) ? pool.get_entity(target_id) : nullptr;
    const bool target_is_alive = target_entity && director.d.ledger->is_alive(target_id);
    bool target_same_level = target_is_alive && target_entity->z == entity->z;
    const bool target_is_valid = target_is_alive && target_same_level &&
        (ai->state != AIState::COMBAT || director.entity_is_hostile_to(entity_id, target_id));
    if (state_requires_target && !target_is_valid) {
        const bool preserve_follow_target = ai->state == AIState::FOLLOW && target_is_alive;
        const bool resume_escort = ai->state == AIState::COMBAT &&
            ai->follow_leader_id != EntityPool::INVALID_ID &&
            director.d.ledger->is_alive(ai->follow_leader_id);
        if (resume_escort) {
            ai->state = AIState::FOLLOW;
            ai->target_entity_id = ai->follow_leader_id;
            target_id = ai->target_entity_id;
            target_entity = pool.get_entity(target_id);
        } else if (!preserve_follow_target) {
            ai->target_entity_id = EntityPool::INVALID_ID;
            ai->state = AIState::WANDER;
            ai->follow_leader_id = EntityPool::INVALID_ID;
            target_id = EntityPool::INVALID_ID;
            target_entity = nullptr;
        }
    }

    if (ai->state == AIState::FOLLOW && target_entity) {
        const uint32_t hostile_id = director.find_nearest_hostile(entity_id, acquire_radius);
        if (hostile_id != EntityPool::INVALID_ID) {
            Entity* hostile = pool.get_entity(hostile_id);
            if (hostile && director.d.ledger->is_alive(hostile_id) && hostile->z == entity->z) {
                ai->state = AIState::COMBAT;
                ai->target_entity_id = hostile_id;
                target_id = hostile_id;
                target_entity = hostile;
            }
        }
    }

    const bool can_auto_acquire = ai->state == AIState::WANDER || ai->state == AIState::COMBAT || ai->state == AIState::GUARD;
    if (!target_entity && can_auto_acquire) {
        target_id = director.find_nearest_hostile(entity_id, acquire_radius);
        target_entity = (target_id != EntityPool::INVALID_ID) ? pool.get_entity(target_id) : nullptr;
        if (target_entity) {
            ai->state = AIState::COMBAT;
            ai->target_entity_id = target_id;
        }
    }
    target_same_level = target_entity &&
        target_id != EntityPool::INVALID_ID &&
        director.d.ledger->is_alive(target_id) &&
        target_entity->z == entity->z;
    Vector2i target_pos = target_entity ? Vector2i(target_entity->x, target_entity->y)
                                        : Vector2i(entity->x, entity->y);
    const Vector2i perception_target_pos = target_same_level
        ? target_pos
        : Vector2i(entity->x, entity->y);

    switch (ai->perception_tier) {
        case PerceptionTier::FULL_OCCLUSION:
            Perception::tick_full(*mem, *entity, *director.d.bubble, perception_target_pos, acquire_radius);
            break;
        case PerceptionTier::RAYCAST:
            Perception::tick_raycast(*mem, *entity, perception_target_pos, *director.d.bubble, tile_db);
            break;
        default:
            break;
    }

    std::function<PathResult(Vector2i, Vector2i)> find_path_fn = [&](const Vector2i& from, const Vector2i& to) -> PathResult {
        std::vector<Vector2i> entity_blocking;
        for (const auto& bp : blocking_positions) {
            if (bp != from) entity_blocking.push_back(bp);
        }
        TraversalSnapshot traversal = director.d.bubble->build_traversal_snapshot(
            from,
            to,
            entity_blocking,
            director.d.ledger,
            entity_id,
            "",
            TraversalRules::can_open_doors(entity_id, *director.d.ledger)
        );
        PathRequest request;
        request.start = from;
        request.goal = to;
        request.flags = PATH_FLAG_ALLOW_DIAGONAL;
        return director.d.pathfinder->find_path(request, traversal);
    };

    std::function<bool(Vector2i)> can_enter_fn = [&](const Vector2i& pos) -> bool {
        for (const Vector2i& blocked : blocking_positions) {
            if (blocked == pos) return false;
        }
        uint16_t tile_id = director.d.bubble->query_tile_id(pos.x, pos.y);
        return TraversalRules::can_enter(entity_id, tile_id, *director.d.ledger);
    };

    AIContext ctx{
        *entity,
        *director.d.bubble,
        tile_db,
        *mem,
        target_pos,
        target_entity != nullptr,
        can_enter_fn,
        find_path_fn,
        target_same_level
    };
    Intent intent = AIController::tick(*ai, *loco, ctx);

    float cost = 1.0f;
    if (intent.type == IntentType::MOVE) {
        if (ai->state == AIState::COMBAT && target_entity &&
            target_entity->z == entity->z &&
            intent.target.x == target_entity->x &&
            intent.target.y == target_entity->y) {
            cost = director.resolve_attack(entity_id, target_id, false);
            if (cost <= 0.0f) cost = 1.0f;
            director.finish_entity_action(entity_id, cost, base_time);
            return;
        }

        if (TraversalRules::can_open_doors(entity_id, *director.d.ledger) &&
            is_openable_tile(*director.d.bubble, intent.target)) {
            ActionResult open_result = ActionResolver::resolve_open(intent, *entity, *director.d.bubble);
            if (open_result.success && open_result.cost > 0.0f) {
                cost = open_result.cost;
            } else {
                Locomotion::clear_path(*loco);
                ai->stuck_counter++;
                cost = 0.0f;
            }
        } else {
            cost = resolve_move(entity_id, *entity, intent, *loco, *ai, blocking_positions, director);
        }
    } else {
        ai->stuck_counter++;
    }

    if (cost <= 0.0f) cost = 1.0f / loco->speed;
    director.finish_entity_action(entity_id, cost, base_time);
}
