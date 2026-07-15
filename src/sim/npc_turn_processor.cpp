#include "npc_turn_processor.h"

#include "simulation_director.h"

#include "components/action_resolver.h"
#include "components/ai_controller.h"
#include "components/effects.h"
#include "components/locomotion.h"
#include "components/perception.h"
#include "core/faction.h"
#include "core/tag_registry.h"
#include "data/faction_db.h"
#include "data/tile_db.h"
#include "entities/entity_tracker.h"
#include "path/path_request.h"
#include "path/path_result.h"
#include "world/traversal_rules.h"

#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

using namespace godot;

namespace {

constexpr int TARGET_SWITCH_ADVANTAGE = 3;
constexpr int LOST_TARGET_LIMIT = 12;

class ScopedBubbleZ {
public:
    ScopedBubbleZ(WorldBubble& p_bubble, int p_z) : bubble(p_bubble), previous_z(p_bubble.get_active_z()) {
        bubble.set_active_z(p_z);
    }
    ~ScopedBubbleZ() { bubble.set_active_z(previous_z); }

private:
    WorldBubble& bubble;
    int previous_z = 0;
};

int chebyshev_distance(const Vector2i& a, const Vector2i& b) {
    return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
}

bool is_openable_tile(WorldBubble& bubble, const Vector2i& pos) {
    TileDb* tile_db = TileDb::get_singleton();
    TagRegistry* tag_reg = TagRegistry::get_singleton();
    if (!tile_db || !tag_reg) return false;
    const uint16_t tile_id = bubble.query_tile_id(pos.x, pos.y);
    const TileInfo* info = tile_db->get_tile_info(tile_id);
    const uint16_t can_open = tag_reg->get_tag_id("CAN_OPEN");
    return info && info->opens_to != 0 && can_open != 0 && tile_db->has_tag(tile_id, can_open);
}

EntityRelation resolved_relation(uint32_t self_id, uint32_t other_id, const EntityLedger& ledger) {
    const AllegianceData* self = ledger.try_get_allegiance(self_id);
    if (self) {
        const auto personal = self->personal_relations.find(other_id);
        if (personal != self->personal_relations.end()) return personal->second;
    }

    const AllegianceData* other = ledger.try_get_allegiance(other_id);
    FactionDb* factions = FactionDb::get_singleton();
    if (!factions) return EntityRelation::NEUTRAL;
    const FactionRelation relation = factions->get_relation_value(
        self ? self->faction_id : String("unaffiliated"),
        other ? other->faction_id : String("unaffiliated")
    );
    if (relation == FactionRelation::ALLIED) return EntityRelation::FRIENDLY;
    if (relation == FactionRelation::HOSTILE) return EntityRelation::HOSTILE;
    return EntityRelation::NEUTRAL;
}

bool target_is_eligible(uint32_t self_id, uint32_t other_id, const AIData& ai, const EntityLedger& ledger) {
    const EntityRelation relation = resolved_relation(self_id, other_id, ledger);
    switch (ai.reaction_policy) {
        case ReactionPolicy::PASSIVE:
            return false;
        case ReactionPolicy::TIMID:
        case ReactionPolicy::PREDATORY:
            return relation != EntityRelation::FRIENDLY;
        case ReactionPolicy::AGGRESSIVE:
            return relation == EntityRelation::HOSTILE;
        case ReactionPolicy::DEFENSIVE: {
            const AllegianceData* allegiance = ledger.try_get_allegiance(self_id);
            if (!allegiance) return false;
            const auto personal = allegiance->personal_relations.find(other_id);
            return personal != allegiance->personal_relations.end() &&
                personal->second == EntityRelation::HOSTILE;
        }
    }
    return false;
}

bool should_scan(AIData& ai) {
    if (ai.state == AIState::COMBAT || ai.state == AIState::FLEE) return true;
    if (ai.reaction_policy == ReactionPolicy::PASSIVE) return false;
    if (ai.calm_scan_countdown > 0) {
        --ai.calm_scan_countdown;
        return false;
    }
    ai.calm_scan_countdown = UtilityFunctions::randi_range(1, 3);
    return true;
}

struct Candidate {
    uint32_t id = EntityPool::INVALID_ID;
    int distance = 0;
};

bool can_see(const Entity& self, const Entity& target, int radius, WorldBubble& bubble, TileDb& tile_db) {
    if (target.z != self.z) return false;
    if (chebyshev_distance(Vector2i(self.x, self.y), Vector2i(target.x, target.y)) > radius) return false;
    return Perception::has_line_of_sight(self.x, self.y, target.x, target.y, bubble, tile_db);
}

bool legal_step(
    const Vector2i& from,
    const Vector2i& to,
    const std::function<bool(Vector2i)>& can_enter_terrain
) {
    const int dx = to.x - from.x;
    const int dy = to.y - from.y;
    if (std::abs(dx) > 1 || std::abs(dy) > 1 || (dx == 0 && dy == 0)) return false;
    if (!can_enter_terrain(to)) return false;
    if (dx != 0 && dy != 0) {
        return can_enter_terrain(Vector2i(from.x + dx, from.y)) &&
            can_enter_terrain(Vector2i(from.x, from.y + dy));
    }
    return true;
}

uint32_t occupant_at(const Vector2i& pos, int z, const EntityTracker* tracker) {
    return tracker
        ? tracker->get_at(Vector3i(pos.x, pos.y, z))
        : EntityPool::INVALID_ID;
}

bool choose_local_alternative(
    uint32_t entity_id,
    const Entity& entity,
    const AIData& ai,
    const LocomotionData& loco,
    const Vector2i& threat_or_target,
    const std::function<bool(Vector2i)>& can_enter_terrain,
    const EntityTracker* tracker,
    Vector2i& out_step
) {
    static const Vector2i directions[] = {
        Vector2i(0, -1), Vector2i(1, -1), Vector2i(1, 0), Vector2i(1, 1),
        Vector2i(0, 1), Vector2i(-1, 1), Vector2i(-1, 0), Vector2i(-1, -1)
    };

    const Vector2i self_pos(entity.x, entity.y);
    const bool fleeing = ai.state == AIState::FLEE;
    const Vector2i goal = fleeing ? threat_or_target :
        (Locomotion::has_path(loco) ? loco.path_goal : threat_or_target);
    const int current_distance = chebyshev_distance(self_pos, goal);
    int best_score = fleeing ? current_distance : current_distance + 2;
    bool found = false;
    const int offset = static_cast<int>(entity_id % 8);

    for (int i = 0; i < 8; ++i) {
        const Vector2i candidate = self_pos + directions[(i + offset) % 8];
        if (!legal_step(self_pos, candidate, can_enter_terrain)) continue;
        if (occupant_at(candidate, entity.z, tracker) != EntityPool::INVALID_ID) continue;

        const int distance = chebyshev_distance(candidate, goal);
        if (fleeing) {
            if (distance < current_distance || (found && distance <= best_score)) continue;
            best_score = distance;
        } else {
            if (distance > current_distance + 1 || (found && distance >= best_score)) continue;
            best_score = distance;
        }
        out_step = candidate;
        found = true;
    }
    return found;
}

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
    const float base_time = entity->next_turn_time;

    EffectsData* effects = director.d.ledger->try_get_effects(entity_id);
    if (effects && Effects::is_stunned(*effects)) {
        const float wait = EffectTuning::STUN_WAIT_STEP;
        director.advance_entity_time(entity_id, wait);
        if (!pool.get_entity(entity_id)) return;
        entity->next_turn_time = base_time + wait;
        director.d.scheduler->push(entity_id, entity->next_turn_time);
        return;
    }

    LocomotionData* loco = director.d.ledger->try_get_locomotion(entity_id);
    AIData* ai = director.d.ledger->try_get_ai(entity_id);
    if (!loco || !ai) return;

    auto restore_home_order = [&]() {
        ai->state = ai->home_state;
        ai->target_entity_id = ai->home_state == AIState::FOLLOW
            ? ai->follow_leader_id
            : EntityPool::INVALID_ID;
        ai->has_last_known_target_position = false;
        ai->lost_target_turns = 0;
        ai->blocked_move_count = 0;
        ai->path_retry_countdown = 0;
        ai->wait_turns = 0;
        ai->forced_reaction = false;
        Locomotion::clear_path(*loco);
    };

    bool target_visible = false;
    if (should_scan(*ai)) {
        const int radius = std::max(1, ai->reaction_radius);
        const bool reacting = ai->state == AIState::COMBAT || ai->state == AIState::FLEE;
        const uint32_t current_id = reacting ? ai->target_entity_id : EntityPool::INVALID_ID;
        Entity* current = current_id != EntityPool::INVALID_ID ? pool.get_entity(current_id) : nullptr;
        int current_distance = 0;
        if (current && director.d.ledger->is_alive(current_id) &&
            (ai->forced_reaction || target_is_eligible(entity_id, current_id, *ai, *director.d.ledger)) &&
            can_see(*entity, *current, radius, *director.d.bubble, tile_db)) {
            target_visible = true;
            current_distance = chebyshev_distance(
                Vector2i(entity->x, entity->y), Vector2i(current->x, current->y));
        }

        std::vector<uint32_t> nearby;
        if (director.d.tracker) {
            director.d.tracker->query_rect(
                Vector2i(entity->x - radius, entity->y - radius),
                Vector2i(entity->x + radius, entity->y + radius),
                nearby,
                entity->z
            );
        } else {
            nearby = pool.get_live_ids();
        }

        std::vector<Candidate> candidates;
        candidates.reserve(nearby.size());
        for (uint32_t candidate_id : nearby) {
            if (ai->forced_reaction) break;
            if (candidate_id == entity_id || candidate_id == current_id ||
                !director.d.ledger->is_alive(candidate_id) ||
                !target_is_eligible(entity_id, candidate_id, *ai, *director.d.ledger)) {
                continue;
            }
            Entity* candidate = pool.get_entity(candidate_id);
            if (!candidate || candidate->z != entity->z) continue;
            const int distance = chebyshev_distance(
                Vector2i(entity->x, entity->y), Vector2i(candidate->x, candidate->y));
            if (distance > radius) continue;
            if (target_visible && distance + TARGET_SWITCH_ADVANTAGE > current_distance) continue;
            candidates.push_back(Candidate{candidate_id, distance});
        }
        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            if (a.distance != b.distance) return a.distance < b.distance;
            return a.id < b.id;
        });

        uint32_t selected_id = target_visible ? current_id : EntityPool::INVALID_ID;
        for (const Candidate& candidate_info : candidates) {
            Entity* candidate = pool.get_entity(candidate_info.id);
            if (candidate && can_see(*entity, *candidate, radius, *director.d.bubble, tile_db)) {
                selected_id = candidate_info.id;
                target_visible = true;
                break;
            }
        }

        if (target_visible) {
            Entity* selected = pool.get_entity(selected_id);
            const bool target_changed = ai->target_entity_id != selected_id ||
                (ai->state != AIState::COMBAT && ai->state != AIState::FLEE);
            ai->state = ai->reaction_policy == ReactionPolicy::TIMID ? AIState::FLEE : AIState::COMBAT;
            if (!reacting) ai->forced_reaction = false;
            ai->target_entity_id = selected_id;
            ai->last_known_target_position = Vector2i(selected->x, selected->y);
            ai->has_last_known_target_position = true;
            ai->lost_target_turns = 0;
            ai->wait_turns = 0;
            if (target_changed) {
                ai->blocked_move_count = 0;
                ai->path_retry_countdown = 0;
                Locomotion::clear_path(*loco);
            }
        } else if (reacting) {
            Entity* remembered = pool.get_entity(ai->target_entity_id);
            if (!remembered || !director.d.ledger->is_alive(ai->target_entity_id) ||
                !ai->has_last_known_target_position) {
                restore_home_order();
            } else if (++ai->lost_target_turns >= LOST_TARGET_LIMIT) {
                restore_home_order();
            }
        }
    }

    if ((ai->state == AIState::COMBAT || ai->state == AIState::FLEE) &&
        ai->home_state == AIState::GUARD && ai->has_last_known_target_position &&
        (chebyshev_distance(Vector2i(entity->x, entity->y), ai->home_position) > ai->reaction_radius ||
         chebyshev_distance(ai->last_known_target_position, ai->home_position) > ai->reaction_radius)) {
        restore_home_order();
        target_visible = false;
    }

    uint32_t target_id = EntityPool::INVALID_ID;
    Entity* target_entity = nullptr;
    Vector2i target_pos(entity->x, entity->y);
    bool target_same_level = false;
    bool has_target = false;
    if (ai->state == AIState::COMBAT || ai->state == AIState::FLEE) {
        target_id = ai->target_entity_id;
        target_entity = pool.get_entity(target_id);
        target_pos = ai->last_known_target_position;
        has_target = target_entity && director.d.ledger->is_alive(target_id) && ai->has_last_known_target_position;
        target_same_level = has_target;
    } else if (ai->state == AIState::FOLLOW) {
        target_id = ai->follow_leader_id;
        target_entity = pool.get_entity(target_id);
        has_target = target_entity && director.d.ledger->is_alive(target_id);
        if (has_target) target_pos = Vector2i(target_entity->x, target_entity->y);
        target_same_level = has_target && target_entity->z == entity->z;
    }

    const bool can_open_doors = TraversalRules::can_open_doors(entity_id, *director.d.ledger);
    std::function<bool(Vector2i)> can_enter_terrain = [&](const Vector2i& pos) {
        const bool active_guard_reaction = ai->home_state == AIState::GUARD &&
            (ai->state == AIState::COMBAT || ai->state == AIState::FLEE);
        if (active_guard_reaction &&
            chebyshev_distance(pos, ai->home_position) > ai->reaction_radius) {
            return false;
        }
        const uint16_t tile_id = director.d.bubble->query_tile_id(pos.x, pos.y);
        return can_open_doors
            ? TraversalRules::can_enter_or_open(entity_id, tile_id, *director.d.ledger)
            : TraversalRules::can_enter(entity_id, tile_id, *director.d.ledger);
    };
    std::function<PathResult(Vector2i, Vector2i, int)> find_path = [&] (
        const Vector2i& from, const Vector2i& to, int goal_radius
    ) {
        TraversalSnapshot traversal = director.d.bubble->build_traversal_snapshot(
            from,
            director.d.ledger,
            entity_id,
            "",
            can_open_doors
        );
        PathRequest request;
        request.start = from;
        request.goal = to;
        request.goal_radius = std::max(0, goal_radius);
        request.flags = PATH_FLAG_ALLOW_DIAGONAL;
        return director.d.pathfinder->find_path(request, traversal);
    };

    AIContext context{
        *entity,
        target_pos,
        target_id,
        has_target,
        target_visible,
        target_same_level,
        can_enter_terrain,
        find_path
    };
    Intent intent = AIController::tick(*ai, *loco, context);

    const float actor_speed = loco->speed > 0.0f ? loco->speed : 1.0f;
    float cost = ActionCost::WAIT / actor_speed;
    if (intent.type == IntentType::ATTACK) {
        if (intent.entity_target_id != EntityPool::INVALID_ID &&
            director.d.ledger->is_alive(intent.entity_target_id)) {
            cost = director.resolve_attack(entity_id, intent.entity_target_id, false);
        }
    } else if (intent.type == IntentType::MOVE) {
        const Vector2i self_pos(entity->x, entity->y);
        const bool completing_wander_path = ai->state == AIState::WANDER &&
            Locomotion::has_path(*loco) &&
            loco->path_index + 1 == static_cast<int>(loco->path.size()) &&
            loco->path[loco->path_index] == intent.target;

        const uint32_t occupant = occupant_at(intent.target, entity->z, director.d.tracker);
        const bool desired_available = occupant == EntityPool::INVALID_ID &&
            legal_step(self_pos, intent.target, can_enter_terrain);
        bool detour = false;
        if (!desired_available) {
            ++ai->blocked_move_count;
            Vector2i alternative;
            if (choose_local_alternative(
                    entity_id, *entity, *ai, *loco, target_pos,
                    can_enter_terrain, director.d.tracker, alternative)) {
                intent.target = alternative;
                detour = true;
                Locomotion::clear_path(*loco);
            } else {
                if (ai->blocked_move_count >= 2) Locomotion::clear_path(*loco);
                intent.type = IntentType::NONE;
            }
        }

        if (intent.type == IntentType::MOVE) {
            if (can_open_doors && is_openable_tile(*director.d.bubble, intent.target)) {
                const ActionResult open_result = ActionResolver::resolve_open(intent, *entity, *director.d.bubble);
                if (open_result.success && open_result.cost > 0.0f) {
                    cost = open_result.cost;
                    ai->blocked_move_count = 0;
                }
            } else {
                const ActionResult move_result = ActionResolver::resolve_move(
                    intent, *entity, *director.d.bubble, *loco,
                    director.d.ledger, director.d.tracker);
                if (move_result.success && move_result.cost > 0.0f) {
                    ai->blocked_move_count = 0;
                    ai->path_retry_countdown = 0;
                    cost = director.movement_action_cost(entity_id, move_result.cost, *loco);
                    if (completing_wander_path && !detour) {
                        ai->wait_turns = UtilityFunctions::randi_range(3, 6);
                        Locomotion::clear_path(*loco);
                    }
                } else {
                    ++ai->blocked_move_count;
                    if (ai->blocked_move_count >= 2) Locomotion::clear_path(*loco);
                }
            }
        }
    }

    if (cost <= 0.0f) cost = ActionCost::WAIT / actor_speed;
    director.finish_entity_action(entity_id, cost, base_time);
}
