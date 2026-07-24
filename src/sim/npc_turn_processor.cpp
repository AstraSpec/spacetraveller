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
#include "world/point_of_interest_registry.h"
#include "world/city_population_director.h"

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

    auto cancel_routine = [&](bool p_retry) {
        if (director.d.poi_registry) {
            director.d.poi_registry->release_for_entity(entity_id);
        }
        ai->routine_has_target = false;
        ai->routine_phase = RoutinePhase::SEEKING;
        ai->routine_dwell_remaining = 0;
        ai->routine_failed_attempts = 0;
        if (p_retry) ai->routine_retry_turns = UtilityFunctions::randi_range(2, 4);
        Locomotion::clear_path(*loco);
    };

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

    if (ai->state != AIState::WANDER && ai->routine_has_target) {
        cancel_routine(false);
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

    bool has_routine_goal = false;
    bool has_ambient_goal = false;
    bool ambient_goal_is_road = false;
    Vector2i routine_goal(entity->x, entity->y);
    const SocialProfileData* social_profile = director.d.ledger->try_get_social_profile(entity_id);
    AmbientJourneyData* ambient = director.d.city_population
        ? director.d.city_population->get_journey(entity_id)
        : nullptr;

    if (ambient && ai->state != AIState::WANDER
        && ambient->phase != AmbientJourneyPhase::FOLLOWING_ROUTE) {
        director.d.city_population->cancel_detour(entity_id);
    }

    if (ambient && ai->state == AIState::WANDER) {
        const Vector3i current(entity->x, entity->y, entity->z);

        if (ambient->phase == AmbientJourneyPhase::DWELLING) {
            if (ambient->dwell_remaining > 0) --ambient->dwell_remaining;
            if (ambient->dwell_remaining <= 0) {
                if (director.d.poi_registry) {
                    director.d.poi_registry->release_for_entity(entity_id);
                }
                ambient->phase = AmbientJourneyPhase::RETURNING_TO_ROAD;
                ambient->return_final_index =
                    static_cast<int>(ambient->detour_final_path.size()) - 2;
                ambient->blocked_turns = 0;
                Locomotion::clear_path(*loco);
                if (ambient->return_final_index < 0) {
                    ambient->phase = AmbientJourneyPhase::FOLLOWING_ROUTE;
                    director.d.city_population->reroute(entity_id);
                }
            }
            const float actor_speed = loco->speed > 0.0f ? loco->speed : 1.0f;
            director.finish_entity_action(entity_id, ActionCost::WAIT / actor_speed, base_time);
            return;
        }

        if (ambient->phase == AmbientJourneyPhase::RETURNING_TO_ROAD) {
            while (ambient->return_final_index >= 0
                && current == ambient->detour_final_path[ambient->return_final_index]) {
                --ambient->return_final_index;
            }
            if (ambient->return_final_index < 0) {
                ambient->phase = AmbientJourneyPhase::FOLLOWING_ROUTE;
                if (!director.d.city_population->reroute(entity_id)) {
                    director.d.city_population->cancel_detour(entity_id);
                }
            } else {
                has_ambient_goal = true;
                has_routine_goal = true;
                const Vector3i& step =
                    ambient->detour_final_path[ambient->return_final_index];
                routine_goal = Vector2i(step.x, step.y);
                ambient_goal_is_road =
                    director.d.city_population->is_road_position(step);
            }
        }

        if (ambient->phase == AmbientJourneyPhase::FOLLOWING_ROUTE) {
            while (ambient->route_index < static_cast<int>(ambient->route.size())
                && current == ambient->route[ambient->route_index]) {
                ++ambient->route_index;
            }

            if (ambient->route_index >= static_cast<int>(ambient->route.size())) {
                if (!director.d.city_population->continue_route(entity_id)) {
                    director.d.city_population->despawn_ambient(entity_id);
                    return;
                }
            }

            if (ambient->wants_detour
                && !ambient->detour_attempted
                && director.d.poi_registry
                && social_profile) {
                std::vector<Vector3i> detours =
                    director.d.poi_registry->find_compatible_near(
                        current,
                        director.d.city_population->get_detour_radius(),
                        social_profile->context_tags,
                        director.d.city_population->get_detour_tags()
                    );
                detours.erase(
                    std::remove_if(
                        detours.begin(),
                        detours.end(),
                        [&](const Vector3i& candidate) {
                            if (!can_enter_terrain(Vector2i(candidate.x, candidate.y))) return true;
                            const uint32_t occupant = occupant_at(
                                Vector2i(candidate.x, candidate.y), candidate.z, director.d.tracker);
                            return occupant != EntityPool::INVALID_ID && occupant != entity_id;
                        }
                    ),
                    detours.end()
                );

                while (!detours.empty()
                    && ambient->phase == AmbientJourneyPhase::FOLLOWING_ROUTE) {
                    double total_weight = 0.0;
                    for (const Vector3i& candidate : detours) {
                        const PointOfInterestInfo* point =
                            director.d.poi_registry->get(candidate);
                        if (point) total_weight += static_cast<double>(point->weight);
                    }
                    if (total_weight <= 0.0) break;
                    double roll = UtilityFunctions::randf() * total_weight;
                    int selected_index = static_cast<int>(detours.size()) - 1;
                    for (int i = 0; i < static_cast<int>(detours.size()); ++i) {
                        const PointOfInterestInfo* point =
                            director.d.poi_registry->get(detours[i]);
                        if (!point) continue;
                        roll -= static_cast<double>(point->weight);
                        if (roll < 0.0) {
                            selected_index = i;
                            break;
                        }
                    }
                    const Vector3i selected = detours[selected_index];
                    detours.erase(detours.begin() + selected_index);
                    std::vector<Vector3i> road_route;
                    Vector3i approach;
                    if (!director.d.city_population->find_detour_approach(
                            current, selected, road_route, approach)) {
                        continue;
                    }
                    std::vector<Vector3i> final_path;
                    final_path.push_back(approach);
                    if (approach != selected) {
                        const PathResult final_result = find_path(
                            Vector2i(approach.x, approach.y),
                            Vector2i(selected.x, selected.y),
                            0
                        );
                        if (!final_result.found
                            || final_result.waypoints.empty()
                            || final_result.waypoints.size() > 2) {
                            continue;
                        }
                        for (const Vector2i& waypoint : final_result.waypoints) {
                            final_path.push_back(Vector3i(
                                waypoint.x, waypoint.y, entity->z));
                        }
                    }
                    if (!director.d.poi_registry->try_reserve(selected, entity_id)) {
                        continue;
                    }
                    ambient->detour_target = selected;
                    ambient->detour_attempted = true;
                    ambient->detour_road_route = std::move(road_route);
                    ambient->detour_road_index = 1;
                    ambient->detour_final_path = std::move(final_path);
                    ambient->detour_final_index = 1;
                    ambient->return_final_index = -1;
                    ambient->phase = AmbientJourneyPhase::TRAVELLING_TO_DETOUR;
                    ambient->blocked_turns = 0;
                    Locomotion::clear_path(*loco);
                }
            }
        }

        if (ambient->phase == AmbientJourneyPhase::TRAVELLING_TO_DETOUR) {
            const PointOfInterestInfo* point = director.d.poi_registry
                ? director.d.poi_registry->get(ambient->detour_target)
                : nullptr;
            if (!point
                || (!director.d.poi_registry->is_reserved_by(ambient->detour_target, entity_id)
                    && !director.d.poi_registry->try_reserve(ambient->detour_target, entity_id))) {
                director.d.city_population->cancel_detour(entity_id);
            }
            while (ambient->phase == AmbientJourneyPhase::TRAVELLING_TO_DETOUR
                && ambient->detour_road_index
                    < static_cast<int>(ambient->detour_road_route.size())
                && current == ambient->detour_road_route[ambient->detour_road_index]) {
                ++ambient->detour_road_index;
            }
            while (ambient->phase == AmbientJourneyPhase::TRAVELLING_TO_DETOUR
                && ambient->detour_road_index
                    >= static_cast<int>(ambient->detour_road_route.size())
                && ambient->detour_final_index
                    < static_cast<int>(ambient->detour_final_path.size())
                && current == ambient->detour_final_path[ambient->detour_final_index]) {
                ++ambient->detour_final_index;
            }
            if (ambient->phase == AmbientJourneyPhase::TRAVELLING_TO_DETOUR
                && current == ambient->detour_target
                && ambient->detour_final_index
                    >= static_cast<int>(ambient->detour_final_path.size())) {
                ambient->phase = AmbientJourneyPhase::DWELLING;
                ambient->dwell_remaining = UtilityFunctions::randi_range(
                    point->dwell_min, point->dwell_max);
                ambient->blocked_turns = 0;
                Locomotion::clear_path(*loco);
                const float actor_speed = loco->speed > 0.0f ? loco->speed : 1.0f;
                director.finish_entity_action(
                    entity_id, ActionCost::WAIT / actor_speed, base_time);
                return;
            }
            if (ambient->phase == AmbientJourneyPhase::TRAVELLING_TO_DETOUR) {
                has_ambient_goal = true;
                has_routine_goal = true;
                Vector3i step;
                if (ambient->detour_road_index
                    < static_cast<int>(ambient->detour_road_route.size())) {
                    step = ambient->detour_road_route[ambient->detour_road_index];
                } else if (ambient->detour_final_index
                    < static_cast<int>(ambient->detour_final_path.size())) {
                    step = ambient->detour_final_path[ambient->detour_final_index];
                } else {
                    step = ambient->detour_target;
                }
                routine_goal = Vector2i(step.x, step.y);
                ambient_goal_is_road =
                    director.d.city_population->is_road_position(step);
            }
        } else if (ambient->phase == AmbientJourneyPhase::FOLLOWING_ROUTE
            && ambient->route_index < static_cast<int>(ambient->route.size())) {
            const Vector3i& waypoint = ambient->route[ambient->route_index];
            if (chebyshev_distance(
                    Vector2i(current.x, current.y),
                    Vector2i(waypoint.x, waypoint.y)) > 1
                && !director.d.city_population->reroute(entity_id)) {
                ambient->blocked_turns = 3;
            }
            has_ambient_goal = true;
            has_routine_goal = true;
            const Vector3i& next = ambient->route[ambient->route_index];
            routine_goal = Vector2i(next.x, next.y);
            ambient_goal_is_road = true;
        }
    }

    const bool routine_enabled = director.d.poi_registry
        && !ambient
        && ai->has_routine_scope
        && ai->home_state == AIState::WANDER
        && ai->state == AIState::WANDER
        && social_profile
        && !social_profile->context_tags.is_empty();

    if (!routine_enabled) {
        if (ai->routine_has_target) cancel_routine(false);
    } else {
        const PointOfInterestScope scope{
            ai->routine_structure_id,
            ai->routine_scope_origin
        };

        if (ai->routine_has_target) {
            const PointOfInterestInfo* target_point =
                director.d.poi_registry->get(ai->routine_target);
            if (!target_point || !(target_point->scope == scope)
                || (!director.d.poi_registry->is_reserved_by(ai->routine_target, entity_id)
                    && !director.d.poi_registry->try_reserve(ai->routine_target, entity_id))) {
                cancel_routine(true);
            }
        }

        if (ai->routine_has_target
            && Vector3i(entity->x, entity->y, entity->z) == ai->routine_target) {
            const PointOfInterestInfo* target_point =
                director.d.poi_registry->get(ai->routine_target);
            if (target_point && ai->routine_phase != RoutinePhase::DWELLING) {
                ai->routine_phase = RoutinePhase::DWELLING;
                ai->routine_dwell_remaining = UtilityFunctions::randi_range(
                    target_point->dwell_min,
                    target_point->dwell_max
                );
                ai->routine_failed_attempts = 0;
                Locomotion::clear_path(*loco);
            }
        }

        if (ai->routine_phase == RoutinePhase::DWELLING && ai->routine_has_target) {
            if (ai->routine_dwell_remaining > 0) {
                --ai->routine_dwell_remaining;
            }
            if (ai->routine_dwell_remaining <= 0) {
                ai->routine_last_position = ai->routine_target;
                ai->routine_has_last_position = true;
                cancel_routine(false);
            }
            const float actor_speed = loco->speed > 0.0f ? loco->speed : 1.0f;
            director.finish_entity_action(entity_id, ActionCost::WAIT / actor_speed, base_time);
            return;
        }

        if (!ai->routine_has_target) {
            if (ai->routine_retry_turns > 0) {
                --ai->routine_retry_turns;
                const float actor_speed = loco->speed > 0.0f ? loco->speed : 1.0f;
                director.finish_entity_action(entity_id, ActionCost::WAIT / actor_speed, base_time);
                return;
            }

            std::vector<Vector3i> candidates = director.d.poi_registry->find_compatible(
                scope,
                entity->z,
                social_profile->context_tags,
                ai->routine_last_position,
                ai->routine_has_last_position
            );
            candidates.erase(
                std::remove_if(
                    candidates.begin(),
                    candidates.end(),
                    [&](const Vector3i& candidate) {
                        if (!can_enter_terrain(Vector2i(candidate.x, candidate.y))) return true;
                        const uint32_t occupant = occupant_at(
                            Vector2i(candidate.x, candidate.y), candidate.z, director.d.tracker);
                        return occupant != EntityPool::INVALID_ID && occupant != entity_id;
                    }
                ),
                candidates.end()
            );

            while (!candidates.empty() && !ai->routine_has_target) {
                double total_weight = 0.0;
                for (const Vector3i& candidate : candidates) {
                    const PointOfInterestInfo* point =
                        director.d.poi_registry->get(candidate);
                    if (point) total_weight += static_cast<double>(point->weight);
                }
                if (total_weight <= 0.0) break;

                double roll = UtilityFunctions::randf() * total_weight;
                int index = static_cast<int>(candidates.size()) - 1;
                for (int candidate_index = 0;
                     candidate_index < static_cast<int>(candidates.size());
                     ++candidate_index) {
                    const PointOfInterestInfo* point =
                        director.d.poi_registry->get(candidates[candidate_index]);
                    if (!point) continue;
                    roll -= static_cast<double>(point->weight);
                    if (roll < 0.0) {
                        index = candidate_index;
                        break;
                    }
                }
                const Vector3i selected = candidates[index];
                candidates.erase(candidates.begin() + index);
                if (director.d.poi_registry->try_reserve(selected, entity_id)) {
                    ai->routine_target = selected;
                    ai->routine_has_target = true;
                    ai->routine_phase = RoutinePhase::TRAVELLING;
                    ai->routine_failed_attempts = 0;
                    ai->blocked_move_count = 0;
                    ai->path_retry_countdown = 0;
                    Locomotion::clear_path(*loco);
                }
            }

            if (!ai->routine_has_target) {
                ai->routine_retry_turns = UtilityFunctions::randi_range(2, 4);
                const float actor_speed = loco->speed > 0.0f ? loco->speed : 1.0f;
                director.finish_entity_action(entity_id, ActionCost::WAIT / actor_speed, base_time);
                return;
            }
        }

        if (ai->routine_has_target && ai->routine_phase == RoutinePhase::TRAVELLING) {
            has_routine_goal = true;
            routine_goal = Vector2i(ai->routine_target.x, ai->routine_target.y);
        }
    }

    AIContext context{
        *entity,
        target_pos,
        target_id,
        has_target,
        target_visible,
        target_same_level,
        has_routine_goal,
        routine_goal,
        can_enter_terrain,
        find_path
    };
    const int retry_countdown_before_tick = ai->path_retry_countdown;
    Intent intent;
    if (has_ambient_goal && Vector2i(entity->x, entity->y) != routine_goal) {
        intent.type = IntentType::MOVE;
        intent.target = routine_goal;
    } else {
        intent = AIController::tick(*ai, *loco, context);
    }

    if (has_routine_goal
        && !has_ambient_goal
        && intent.type == IntentType::NONE
        && Vector2i(entity->x, entity->y) != routine_goal
        && retry_countdown_before_tick == 0
        && ai->path_retry_countdown > 0) {
        ++ai->routine_failed_attempts;
        if (ai->routine_failed_attempts >= 2) {
            cancel_routine(true);
            has_routine_goal = false;
        }
    }

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
            !has_routine_goal &&
            Locomotion::has_path(*loco) &&
            loco->path_index + 1 == static_cast<int>(loco->path.size()) &&
            loco->path[loco->path_index] == intent.target;

        bool road_reserved = true;
        if (has_ambient_goal && ambient_goal_is_road) {
            road_reserved = director.d.city_population->try_reserve_road_cell(
                Vector3i(intent.target.x, intent.target.y, entity->z),
                entity_id
            );
        }
        const uint32_t occupant = occupant_at(intent.target, entity->z, director.d.tracker);
        const bool desired_available = road_reserved
            && occupant == EntityPool::INVALID_ID
            && legal_step(self_pos, intent.target, can_enter_terrain);
        bool detour = false;
        bool ambient_alternate = false;
        if (!desired_available) {
            ++ai->blocked_move_count;
            if (has_ambient_goal && ambient) {
                director.d.city_population->release_road_reservation(entity_id);
                ++ambient->blocked_turns;
                Vector3i alternative;
                if (ambient->phase == AmbientJourneyPhase::FOLLOWING_ROUTE
                    && ambient_goal_is_road
                    && director.d.city_population->find_alternate_road_step(
                        entity_id,
                        Vector3i(entity->x, entity->y, entity->z),
                        ambient->route.empty()
                            ? Vector3i(routine_goal.x, routine_goal.y, entity->z)
                            : ambient->route.back(),
                        alternative)
                    && director.d.city_population->try_reserve_road_cell(
                        alternative, entity_id)) {
                    intent.target = Vector2i(alternative.x, alternative.y);
                    ambient_alternate = true;
                    detour = true;
                } else {
                    intent.type = IntentType::NONE;
                    if (ambient->blocked_turns >= 3) {
                        if (ambient->phase == AmbientJourneyPhase::FOLLOWING_ROUTE) {
                            director.d.city_population->reroute_around_congestion(entity_id);
                        } else {
                            director.d.city_population->cancel_detour(entity_id);
                            if (director.d.city_population->is_road_position(
                                    Vector3i(entity->x, entity->y, entity->z))) {
                                director.d.city_population->reroute(entity_id);
                            }
                        }
                        ambient->blocked_turns = 0;
                        ai->blocked_move_count = 0;
                    }
                }
            } else if (has_routine_goal && ++ai->routine_failed_attempts >= 2) {
                cancel_routine(true);
                has_routine_goal = false;
                intent.type = IntentType::NONE;
            } else {
                Vector2i alternative;
                if (choose_local_alternative(
                        entity_id, *entity, *ai, *loco,
                        has_routine_goal ? routine_goal : target_pos,
                        can_enter_terrain, director.d.tracker, alternative)) {
                    intent.target = alternative;
                    detour = true;
                    Locomotion::clear_path(*loco);
                } else {
                    if (ai->blocked_move_count >= 2) Locomotion::clear_path(*loco);
                    intent.type = IntentType::NONE;
                }
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
                    if (has_ambient_goal && ambient) {
                        ambient->blocked_turns = 0;
                    }
                    if (!detour) ai->routine_failed_attempts = 0;
                    cost = director.movement_action_cost(entity_id, move_result.cost, *loco);
                    if (ambient_alternate && ambient) {
                        director.d.city_population->reroute_from(
                            entity_id,
                            Vector3i(entity->x, entity->y, entity->z)
                        );
                    }
                    if (ambient
                        && ambient->phase == AmbientJourneyPhase::FOLLOWING_ROUTE
                        && !ambient->route.empty()
                        && Vector3i(entity->x, entity->y, entity->z)
                            == ambient->route.back()) {
                        if (!director.d.city_population->continue_route(entity_id)) {
                            director.d.city_population->release_road_reservation(entity_id);
                            director.d.city_population->despawn_ambient(entity_id);
                            return;
                        }
                    }
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

    if (has_routine_goal && !has_ambient_goal && ai->blocked_move_count >= 2) {
        cancel_routine(true);
    }

    if (director.d.city_population) {
        director.d.city_population->release_road_reservation(entity_id);
    }
    if (cost <= 0.0f) cost = ActionCost::WAIT / actor_speed;
    director.finish_entity_action(entity_id, cost, base_time);
}
