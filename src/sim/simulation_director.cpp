#include "sim/simulation_director.h"

#include "path/path_request.h"
#include "path/path_result.h"
#include "data/tile_db.h"
#include "data/race_db.h"
#include "core/world_coords.h"
#include "core/id_registry.h"
#include "components/action_resolver.h"
#include "components/ai_controller.h"
#include "components/perception.h"
#include "components/locomotion.h"
#include "components/equipment.h"

#include <cmath>
#include <vector>

using namespace godot;

void SimulationDirector::configure(const SimulationDirectorDeps& deps) {
    d = deps;
}

float SimulationDirector::submit_player_intent(int intent_type, int target_x, int target_y, const String& param) {
    if (d.ledger == nullptr || d.bubble == nullptr || d.scheduler == nullptr || d.sink == nullptr) {
        return 0.0f;
    }

    Intent intent;
    intent.type = static_cast<IntentType>(intent_type);
    intent.target = Vector2i(target_x, target_y);
    intent.param = param;

    Entity* entity = d.ledger->get_entity_pool().get_entity(d.player_entity_id);
    if (!entity) return 0.0f;

    auto pl_hp_it = d.ledger->health_data.find(d.player_entity_id);
    if (pl_hp_it != d.ledger->health_data.end() && !pl_hp_it->second.alive) {
        return 0.0f;
    }

    if (intent.type == IntentType::MOVE) {
        const WorldBubble::CellEntity* occupant = d.bubble->get_entity_at(intent.target.x, intent.target.y);
        if (occupant && occupant->entity_id != d.player_entity_id) {
            intent.type = IntentType::ATTACK;
        }
    }

    auto& loco = d.ledger->locomotion_data[d.player_entity_id];
    int old_x = entity->x, old_y = entity->y;
    float cost = 0.0f;

    if (intent.type == IntentType::ATTACK) {
        const WorldBubble::CellEntity* occupant = d.bubble->get_entity_at(intent.target.x, intent.target.y);
        if (!occupant || occupant->entity_id == d.player_entity_id) return 0.0f;

        uint32_t defender_id = occupant->entity_id;
        auto def_hp_it = d.ledger->health_data.find(defender_id);
        if (def_hp_it == d.ledger->health_data.end() || !def_hp_it->second.alive) return 0.0f;

        cost = ActionResolver::resolve_attack(d.player_entity_id, defender_id, *d.bubble, def_hp_it->second, d.ledger->equipment_data[d.player_entity_id], 10.0f);

        if (cost > 0.0f) {
            float dmg = 10.0f + Equipment::get_attack_power(d.ledger->equipment_data[d.player_entity_id]);
            d.sink->on_combat_event(d.player_entity_id, defender_id, dmg, def_hp_it->second.alive ? "hit" : "kill");
            if (!def_hp_it->second.alive) {
                d.sink->on_entity_died(defender_id, "combat");
                despawn_entity(defender_id);
            }
        }
    } else {
        cost = ActionResolver::resolve(d.player_entity_id, intent, *d.bubble, *entity, loco);
    }

    if (cost > 0.0f) {
        if (entity->x != old_x || entity->y != old_y) {
            Vector2i new_pos(entity->x, entity->y);
            Vector2i new_chunk = entity_chunk(d.player_entity_id);
            d.sink->on_entity_moved(d.player_entity_id, new_pos, new_chunk);
        }

        float player_next_time = entity->next_turn_time + cost;
        entity->next_turn_time = player_next_time;
        d.scheduler->push(d.player_entity_id, player_next_time);

        process_game_turn(player_next_time);

        d.sink->on_player_action_resolved(d.player_entity_id, cost, player_next_time);
    }

    return cost;
}

void SimulationDirector::process_game_turn(float current_time) {
    if (d.ledger == nullptr || d.bubble == nullptr || d.scheduler == nullptr || d.sink == nullptr) {
        return;
    }

    TileDb* tile_db = TileDb::get_singleton();
    if (!tile_db) return;

    EntityPool& pool = d.ledger->get_entity_pool();
    Entity* player_entity = pool.get_entity(d.player_entity_id);
    Vector2i player_pos(player_entity ? player_entity->x : 0, player_entity ? player_entity->y : 0);

    std::vector<Vector2i> blocking_positions;
    blocking_positions.reserve(pool.living_count());
    for (const auto& entity : pool.get_all()) {
        if (entity.id != d.player_entity_id) {
            blocking_positions.push_back({entity.x, entity.y});
        }
    }

    while (d.scheduler->peek_time() <= current_time) {
        uint32_t entity_id = d.scheduler->pop();
        if (entity_id == EntityPool::INVALID_ID) break;
        if (entity_id == d.player_entity_id) {
            d.sink->on_player_turn_ready(entity_id);
            break;
        }

        Entity* entity = pool.get_entity(entity_id);
        if (!entity) continue;

        float base_time = entity->next_turn_time;
        if (base_time + 1.0f < current_time) {
            base_time = current_time;
        }

        auto loco_it = d.ledger->locomotion_data.find(entity_id);
        if (loco_it == d.ledger->locomotion_data.end()) continue;
        auto& loco = loco_it->second;

        auto mem_it = d.ledger->perception_memory.find(entity_id);
        auto ai_it = d.ledger->ai_data.find(entity_id);
        if (mem_it == d.ledger->perception_memory.end() || ai_it == d.ledger->ai_data.end()) continue;
        auto& mem = mem_it->second;
        auto& ai = ai_it->second;

        switch (ai.perception_tier) {
            case PerceptionTier::FULL_OCCLUSION:
                Perception::tick_full(mem, *entity, *d.bubble, player_pos);
                break;
            case PerceptionTier::RAYCAST:
                Perception::tick_raycast(mem, *entity, player_pos, *d.bubble, *tile_db);
                break;
            default:
                break;
        }

        auto find_path_fn = [&](const Vector2i& from, const Vector2i& to) -> PathResult {
            std::vector<Vector2i> entity_blocking;
            for (const auto& bp : blocking_positions) {
                if (bp != from) entity_blocking.push_back(bp);
            }
            TraversalSnapshot traversal = d.bubble->build_traversal_snapshot(from, to, entity_blocking);
            PathRequest request;
            request.start = from;
            request.goal = to;
            request.flags = PATH_FLAG_ALLOW_DIAGONAL;
            return d.pathfinder->find_path(request, traversal);
        };

        AIContext ctx{*entity, *d.bubble, *tile_db, mem, player_pos, find_path_fn};
        Intent intent = AIController::tick(ai, loco, ctx);

        float cost = 1.0f;
        if (intent.type == IntentType::MOVE) {
            // If the NPC tries to step onto the player's cell, attack instead
            if (player_entity && player_entity->id == d.player_entity_id &&
                intent.target.x == player_entity->x && intent.target.y == player_entity->y) {
                auto pl_hp_it = d.ledger->health_data.find(d.player_entity_id);
                if (pl_hp_it != d.ledger->health_data.end() && pl_hp_it->second.alive) {
                    // Look up attacker's race base damage
                    float atk_damage = 10.0f;
                    auto atk_anat_it = d.ledger->anatomy_data.find(entity_id);
                    if (atk_anat_it != d.ledger->anatomy_data.end()) {
                        RaceDb* race_db = RaceDb::get_singleton();
                        if (race_db) {
                            const RaceInfo* race = race_db->get_race_info(atk_anat_it->second.race_id);
                            if (race) atk_damage = race->base_damage;
                        }
                    }
                    cost = ActionResolver::resolve_attack(entity_id, d.player_entity_id, *d.bubble,
                                                          pl_hp_it->second, d.ledger->equipment_data[entity_id], atk_damage);
                    if (cost > 0.0f) {
                        float dmg = atk_damage + Equipment::get_attack_power(d.ledger->equipment_data[entity_id]);
                        d.sink->on_combat_event(entity_id, d.player_entity_id, dmg,
                                                pl_hp_it->second.alive ? "hit" : "kill");
                        if (!pl_hp_it->second.alive) {
                            d.sink->on_player_died("combat");
                        }
                    }
                }
                if (cost <= 0.0f) cost = 1.0f;
                entity->next_turn_time = base_time + cost;
                d.scheduler->push(entity_id, entity->next_turn_time);
                continue;
            }

            float move_cost = ActionResolver::resolve_move(intent, *entity, *d.bubble, loco);
            if (move_cost > 0.0f) {
                cost = move_cost / loco.speed;
                for (auto& bp : blocking_positions) {
                    Vector2i old_pos(entity->x - (intent.target.x - entity->x),
                                     entity->y - (intent.target.y - entity->y));
                    if (bp == old_pos) {
                        bp = Vector2i(entity->x, entity->y);
                        break;
                    }
                }
            } else {
                Locomotion::clear_path(loco);
                ai.stuck_counter++;
            }
        } else {
            ai.stuck_counter++;
        }

        if (cost <= 0.0f) cost = 1.0f / loco.speed;
        entity->next_turn_time = base_time + cost;
        d.scheduler->push(entity_id, entity->next_turn_time);
    }
}

Array SimulationDirector::find_path(const Vector2i& start, const Vector2i& goal) {
    return find_path_with_flags(start, goal, 0);
}

Array SimulationDirector::request_player_path(const Vector2i& start, const Vector2i& goal) {
    if (!d.bubble->is_cell_seen(goal.x, goal.y)) {
        return Array();
    }
    return find_path_with_flags(start, goal, PATH_FLAG_ALLOW_DIAGONAL);
}

Array SimulationDirector::find_path_with_flags(const Vector2i& start, const Vector2i& goal, uint32_t flags) {
    if (!d.pathfinder) {
        return Array();
    }

    std::vector<Vector2i> blocking;
    blocking.push_back(start);

    TraversalSnapshot traversal = d.bubble->build_traversal_snapshot(start, goal, blocking);
    PathRequest request;
    request.start = start;
    request.goal = goal;
    request.flags = flags;

    PathResult result = d.pathfinder->find_path(request, traversal);
    return path_result_to_array(result);
}

void SimulationDirector::despawn_entity(uint32_t entity_id) {
    if (entity_id == d.player_entity_id) return;

    Entity* entity = d.ledger->get_entity_pool().get_entity(entity_id);
    if (entity) {
        Vector2i pos(entity->x, entity->y);

        // Drop all inventory items to the ground
        auto inv_it = d.ledger->inventory_data.find(entity_id);
        if (inv_it != d.ledger->inventory_data.end()) {
            for (const auto& item : inv_it->second.items) {
                if (item.amount > 0) {
                    d.bubble->drop_item(pos, item.id, item.amount);
                }
            }
        }

        // Drop the race-specific corpse
        auto anat_it = d.ledger->anatomy_data.find(entity_id);
        if (anat_it != d.ledger->anatomy_data.end()) {
            RaceDb* race_db = RaceDb::get_singleton();
            if (race_db) {
                const RaceInfo* race = race_db->get_race_info(anat_it->second.race_id);
                if (race && !race->corpse_item.is_empty()) {
                    IdRegistry* reg = IdRegistry::get_singleton();
                    if (reg) {
                        uint16_t corpse_id = reg->get_id(race->corpse_item);
                        if (corpse_id != 0) {
                            d.bubble->drop_item(pos, corpse_id, 1);
                        }
                    }
                }
            }
        }

        d.bubble->remove_entity(entity->x, entity->y);
    }

    d.ledger->destroy_entity(entity_id);
}

Vector2i SimulationDirector::entity_chunk(uint32_t entity_id) const {
    const Entity* e = d.ledger->get_entity_pool().get_entity(entity_id);
    if (e) {
        int cs = WorldCoords::CHUNK_SIZE;
        return Vector2i(floor((float)e->x / cs), floor((float)e->y / cs));
    }
    return Vector2i();
}
