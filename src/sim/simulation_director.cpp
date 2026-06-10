#include "sim/simulation_director.h"

#include "path/path_request.h"
#include "path/path_result.h"
#include "data/tile_db.h"
#include "data/race_db.h"
#include "data/style_db.h"
#include "data/loot_db.h"
#include "core/world_coords.h"
#include "core/id_registry.h"
#include "core/tag_registry.h"
#include "core/faction.h"
#include "world/entity_lifecycle.h"
#include "components/action_resolver.h"
#include "components/ai_controller.h"
#include "components/combat_resolver.h"
#include "components/perception.h"
#include "components/locomotion.h"
#include "components/stamina.h"
#include "components/effects.h"
#include "components/equipment.h"
#include "components/anatomy.h"
#include "data/body_part_db.h"

#include <godot_cpp/variant/utility_functions.hpp>
#include <cmath>
#include <vector>

using namespace godot;

void SimulationDirector::configure(const SimulationDirectorDeps& deps) {
    d = deps;
}

String SimulationDirector::entity_faction(uint32_t entity_id) const {
    auto anat_it = d.ledger->anatomy_data.find(entity_id);
    if (anat_it == d.ledger->anatomy_data.end()) return String();
    RaceDb* race_db = RaceDb::get_singleton();
    if (!race_db) return String();
    const RaceInfo* race = race_db->get_race_info(anat_it->second.race_id);
    return race ? race->faction : String();
}

uint32_t SimulationDirector::find_nearest_hostile(uint32_t entity_id, int radius) const {
    const Entity* self = d.ledger->get_entity_pool().get_entity(entity_id);
    if (!self) return EntityPool::INVALID_ID;

    String my_faction = entity_faction(entity_id);
    if (my_faction.is_empty()) return EntityPool::INVALID_ID;

    uint32_t best_id = EntityPool::INVALID_ID;
    long best_dist_sq = -1;

    for (uint32_t other_id : d.ledger->get_entity_pool().get_live_ids()) {
        if (other_id == entity_id) continue;
        const Entity* other = d.ledger->get_entity_pool().get_entity(other_id);
        if (!other) continue;

        // Cheap bounding-box reject before any distance math.
        int dx = other->x - self->x;
        int dy = other->y - self->y;
        if (dx > radius || dx < -radius || dy > radius || dy < -radius) continue;

        if (!Faction::are_hostile(my_faction, entity_faction(other_id))) continue;

        long dist_sq = static_cast<long>(dx) * dx + static_cast<long>(dy) * dy;
        if (best_dist_sq < 0 || dist_sq < best_dist_sq) {
            best_dist_sq = dist_sq;
            best_id = other_id;
        }
    }
    return best_id;
}

float SimulationDirector::entity_base_damage(uint32_t entity_id) const {
    auto anat_it = d.ledger->anatomy_data.find(entity_id);
    if (anat_it == d.ledger->anatomy_data.end()) return 10.0f;
    RaceDb* race_db = RaceDb::get_singleton();
    if (!race_db) return 10.0f;
    const RaceInfo* race = race_db->get_race_info(anat_it->second.race_id);
    return race ? race->base_damage : 10.0f;
}

Rng::Seeded SimulationDirector::combat_rng_for(uint32_t attacker_id, uint32_t defender_id) const {
    const Entity* attacker = d.ledger->get_entity_pool().get_entity(attacker_id);
    Vector2i pos = attacker ? Vector2i(attacker->x, attacker->y) : Vector2i();
    uint64_t salt = (static_cast<uint64_t>(attacker_id) << 32) ^ static_cast<uint64_t>(defender_id);
    if (attacker) {
        salt ^= Rng::mix64(static_cast<uint64_t>(attacker->next_turn_time * 1000.0f));
    }
    uint32_t seed = d.world_seed ? static_cast<uint32_t>(*d.world_seed) : 0;
    return Rng::at(seed, pos, Rng::COMBAT, salt);
}

CombatOutcome SimulationDirector::resolve_entity_attack(uint32_t attacker_id, uint32_t defender_id) {
    CombatOutcome outcome;

    AnatomyData* attacker_anatomy = d.ledger->try_get_anatomy(attacker_id);
    AnatomyData* defender_anatomy = d.ledger->try_get_anatomy(defender_id);
    HealthData* defender_health = d.ledger->try_get_health(defender_id);
    EquipmentData* attacker_equipment = d.ledger->try_get_equipment(attacker_id);
    StaminaData* attacker_stamina = d.ledger->try_get_stamina(attacker_id);
    if (!attacker_anatomy || !defender_anatomy || !defender_health ||
        !attacker_equipment || !attacker_stamina || !defender_health->alive) {
        return outcome;
    }

    const StyleInfo* style = nullptr;
    auto style_it = d.ledger->combat_style.find(attacker_id);
    if (style_it != d.ledger->combat_style.end()) {
        StyleDb* style_db = StyleDb::get_singleton();
        if (style_db) style = style_db->get_style_info(style_it->second);
    }

    Rng::Seeded rng = combat_rng_for(attacker_id, defender_id);
    CombatContext ctx{
        *attacker_anatomy,
        *defender_anatomy,
        *defender_health,
        *attacker_equipment,
        rng,
        entity_base_damage(attacker_id),
        style,
        attacker_stamina
    };
    return CombatResolver::resolve_attack(ctx);
}

void SimulationDirector::handle_entity_death(uint32_t entity_id, const String& cause, uint32_t killer_id) {
    if (entity_id == d.player_entity_id) {
        d.sink->on_player_died(cause);
        return;
    }

    d.sink->on_entity_died(entity_id, cause);
    if (d.event_listener) {
        GameEvent e;
        e.type = GameEventType::ENTITY_KILLED;
        e.subject_id = killer_id;
        e.target_id = entity_id;
        if (auto* dead = d.ledger->get_entity_pool().get_entity(entity_id)) {
            e.position = Vector2i(dead->x, dead->y);
        }
        d.event_listener->on_game_event(e);
    }
    uint32_t seed = d.world_seed ? static_cast<uint32_t>(*d.world_seed) : 0;
    EntityLifecycle::despawn_entity(entity_id, *d.ledger, *d.bubble, *d.scheduler, seed, true);
}

bool SimulationDirector::finish_entity_action(uint32_t entity_id, float cost, float base_time) {
    if (cost <= 0.0f) return false;

    auto stam_it = d.ledger->stamina_data.find(entity_id);
    if (stam_it != d.ledger->stamina_data.end()) {
        Stamina::regen(stam_it->second, cost * StaminaTuning::REGEN_PER_TIME);
    }

    advance_entity_time(entity_id, cost);

    Entity* entity = d.ledger->get_entity_pool().get_entity(entity_id);
    if (!entity) return false;

    float next_time = base_time + cost;
    entity->next_turn_time = next_time;
    d.scheduler->push(entity_id, next_time);
    return true;
}

void SimulationDirector::emit_movement_if_needed(uint32_t entity_id, const Vector2i& old_pos) {
    const Entity* entity = d.ledger->get_entity_pool().get_entity(entity_id);
    if (!entity || (entity->x == old_pos.x && entity->y == old_pos.y)) return;

    Vector2i new_pos(entity->x, entity->y);
    d.sink->on_entity_moved(entity_id, new_pos, entity_chunk(entity_id));
    if (d.event_listener) {
        GameEvent e;
        e.type = GameEventType::ENTITY_MOVED;
        e.subject_id = entity_id;
        e.position = new_pos;
        d.event_listener->on_game_event(e);
    }
}

void SimulationDirector::apply_attack_effects(uint32_t attacker_id, uint32_t defender_id, const CombatOutcome& atk) {
    if (!atk.hit) return;
    EffectsData& fx = d.ledger->effects_data[defender_id];

    if (atk.hit_part_type == "head") {
        bool was_stunned = Effects::is_stunned(fx);
        Effects::add(fx, Effects::make_stun_decay(EffectTuning::STUN_PER_HEAD_HIT));
        if (!was_stunned && Effects::is_stunned(fx)) {
            d.sink->on_effect_event(defender_id, "stun", "onset", "");
        }
    }

    if (!atk.effect_type.is_empty() && atk.effect_magnitude > 0.0f) {
        if (atk.effect_type == "bleed" && atk.hit_part_index >= 0) {
            Effects::add(fx, Effects::make_bleed(atk.hit_part_index, atk.effect_magnitude));
            d.sink->on_effect_event(defender_id, "bleed", "onset", atk.part_name);
        } else if (atk.effect_type == "stun") {
            if (atk.effect_mode == "timer") {
                Effects::add(fx, Effects::make_stun_timer(atk.effect_duration, atk.effect_magnitude));
            } else {
                Effects::add(fx, Effects::make_stun_decay(atk.effect_magnitude));
            }
            d.sink->on_effect_event(defender_id, "stun", "onset", "");
        }
    }
}

void SimulationDirector::advance_entity_time(uint32_t entity_id, float dt) {
    if (dt <= 0.0f) return;

    auto hp_regen_it = d.ledger->health_data.find(entity_id);
    if (hp_regen_it != d.ledger->health_data.end()) {
        Health::heal(hp_regen_it->second, HealthTuning::REGEN_PER_TIME * dt);
    }

    auto fx_it = d.ledger->effects_data.find(entity_id);
    if (fx_it == d.ledger->effects_data.end() || fx_it->second.effects.empty()) return;

    float bleed = Effects::total_bleed(fx_it->second);
    if (bleed > 0.0f) {
        auto hp_it = d.ledger->health_data.find(entity_id);
        if (hp_it != d.ledger->health_data.end() && hp_it->second.alive) {
            Health::damage(hp_it->second, bleed * EffectTuning::BLEED_HP_PER_MAG * dt);
                if (!hp_it->second.alive) {
                    handle_entity_death(entity_id, "bleed", 0);
                    return;
                }
        }
    }

    std::vector<int> expired_bleeds;
    Effects::tick(fx_it->second, dt, &expired_bleeds);

    for (int part_index : expired_bleeds) {
        String part_name = "";
        auto anat_it = d.ledger->anatomy_data.find(entity_id);
        if (anat_it != d.ledger->anatomy_data.end()) {
            BodyPartDb* bpd = BodyPartDb::get_singleton();
            if (bpd) part_name = bpd->get_body_part_name(Anatomy::get_type_id(anat_it->second, part_index));
        }
        d.sink->on_effect_event(entity_id, "bleed", "stopped", part_name);
    }
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

    // Stun freeze: while heavily stunned the player can't act; any input is a forced wait
    {
        auto fx_it = d.ledger->effects_data.find(d.player_entity_id);
        if (fx_it != d.ledger->effects_data.end() && Effects::is_stunned(fx_it->second)) {
            float wait = EffectTuning::STUN_WAIT_STEP;
            advance_entity_time(d.player_entity_id, wait);
            float next_time = entity->next_turn_time + wait;
            entity->next_turn_time = next_time;
            d.scheduler->push(d.player_entity_id, next_time);
            process_game_turn(next_time);
            d.sink->on_effect_event(d.player_entity_id, "stun", "frozen", "");
            d.sink->on_player_action_resolved(d.player_entity_id, wait, next_time);
            return wait;
        }
    }

    if (intent.type == IntentType::MOVE) {
        const WorldBubble::CellEntity* occupant = d.bubble->get_entity_at(intent.target.x, intent.target.y);
        if (occupant && occupant->entity_id != d.player_entity_id) {
            // Hostile occupant -> attack. Friendly occupant -> interact (no turn spent).
            if (Faction::are_hostile(entity_faction(d.player_entity_id), entity_faction(occupant->entity_id))) {
                intent.type = IntentType::ATTACK;
            } else {
                d.sink->on_interact_event(d.player_entity_id, occupant->entity_id);
                return 0.0f;
            }
        }
        if (!occupant) {
            TileDb* tile_db = TileDb::get_singleton();
            TagRegistry* tag_reg = TagRegistry::get_singleton();
            uint16_t can_open = tag_reg ? tag_reg->get_tag_id("CAN_OPEN") : 0;
            uint16_t tile_id = d.bubble->query_tile_id(intent.target.x, intent.target.y);
            const TileInfo* info = tile_db ? tile_db->get_tile_info(tile_id) : nullptr;
            if (info && can_open != 0 && info->opens_to != 0 && tile_db->has_tag(tile_id, can_open)) {
                intent.type = IntentType::OPEN;
            }
        }
    }

    auto& loco = d.ledger->locomotion_data[d.player_entity_id];
    float player_base_time = entity->next_turn_time;
    Vector2i old_pos(entity->x, entity->y);
    float cost = 0.0f;

    if (intent.type == IntentType::ATTACK) {
        const WorldBubble::CellEntity* occupant = d.bubble->get_entity_at(intent.target.x, intent.target.y);
        if (!occupant || occupant->entity_id == d.player_entity_id) return 0.0f;

        uint32_t defender_id = occupant->entity_id;
        auto def_hp_it = d.ledger->health_data.find(defender_id);
        if (def_hp_it == d.ledger->health_data.end() || !def_hp_it->second.alive) return 0.0f;

        CombatOutcome atk = resolve_entity_attack(d.player_entity_id, defender_id);

        if (atk.no_limbs) {
            d.sink->on_combat_event(d.player_entity_id, defender_id, 0.0f, "no_limbs", atk.verb, "");
            cost = ActionCost::ATTACK;
        } else if (atk.exhausted) {
            d.sink->on_combat_event(d.player_entity_id, defender_id, 0.0f, "exhausted", atk.verb, "");
            cost = ActionCost::ATTACK;
        } else {
            cost = ActionCost::ATTACK / (atk.speed > 0.0f ? atk.speed : 1.0f);

            String result_str;
            if (!atk.hit) result_str = "miss";
            else if (atk.killed) result_str = atk.crit ? "crit_kill" : "kill";
            else result_str = atk.crit ? "crit" : "hit";
            d.sink->on_combat_event(d.player_entity_id, defender_id, atk.damage, result_str, atk.verb, atk.part_name);
            if (atk.killed) {
                handle_entity_death(defender_id, "combat", d.player_entity_id);
            } else {
                apply_attack_effects(d.player_entity_id, defender_id, atk);
            }
        }
    } else if (intent.type == IntentType::SMASH) {
        uint16_t tile_numeric = d.bubble->query_tile_id(intent.target.x, intent.target.y);
        IdRegistry* reg = IdRegistry::get_singleton();
        String tile_id = reg ? reg->get_string(tile_numeric) : "";

        auto stam_it = d.ledger->stamina_data.find(d.player_entity_id);
        bool has_stam = stam_it != d.ledger->stamina_data.end();
        if (has_stam && !Stamina::can_afford(stam_it->second, StaminaTuning::SMASH_COST)) {
            d.sink->on_smash_event(d.player_entity_id, tile_id, "exhausted");
            return 0.0f;
        }

        if (UtilityFunctions::randf() < ActionTuning::SMASH_FAIL_CHANCE) {
            // Failed swing: still costs the turn and stamina, tile survives.
            cost = ActionCost::SMASH;
            if (has_stam) Stamina::drain(stam_it->second, StaminaTuning::SMASH_COST);
            d.sink->on_smash_event(d.player_entity_id, tile_id, "failed");
        } else {
            cost = ActionResolver::resolve(d.player_entity_id, intent, *d.bubble, *entity, loco);
            if (cost > 0.0f) {
                if (has_stam) Stamina::drain(stam_it->second, StaminaTuning::SMASH_COST);
                TileDb* tile_db_singleton = TileDb::get_singleton();
                const TileInfo* smashed_tile = tile_db_singleton ? tile_db_singleton->get_tile_info(tile_numeric) : nullptr;
                LootDb* loot_db = LootDb::get_singleton();
                if (smashed_tile && smashed_tile->smash_loot_table != 0 && loot_db) {
                    uint32_t seed = d.world_seed ? static_cast<uint32_t>(*d.world_seed) : 0;
                    Rng::Seeded loot_rng = Rng::at(seed, intent.target, Rng::TILE_LOOT);
                    std::vector<LootStack> stacks;
                    loot_db->roll_table(smashed_tile->smash_loot_table, loot_rng, stacks);
                    for (const LootStack& stack : stacks) {
                        if (stack.item_id != 0 && stack.amount > 0) {
                            d.bubble->drop_item(intent.target, stack.item_id, stack.amount);
                        }
                    }
                }
                d.sink->on_smash_event(d.player_entity_id, tile_id, "smashed");
            }
        }
    } else {
        cost = ActionResolver::resolve(d.player_entity_id, intent, *d.bubble, *entity, loco);
        if (cost > 0.0f && intent.type == IntentType::MOVE) {
            cost /= (loco.speed > 0.0f ? loco.speed : 1.0f);
            auto stam_it = d.ledger->stamina_data.find(d.player_entity_id);
            if (stam_it != d.ledger->stamina_data.end()) {
                cost *= Stamina::move_cost_multiplier(stam_it->second);
            }
        }
    }

    if (cost > 0.0f) {
        if (finish_entity_action(d.player_entity_id, cost, player_base_time)) {
            emit_movement_if_needed(d.player_entity_id, old_pos);
            Entity* player = d.ledger->get_entity_pool().get_entity(d.player_entity_id);
            if (player) {
                process_game_turn(player->next_turn_time);
                d.sink->on_player_action_resolved(d.player_entity_id, cost, player->next_turn_time);
            }
        }
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

    std::vector<Vector2i> blocking_positions;
    blocking_positions.reserve(pool.living_count());
    for (uint32_t id : pool.get_live_ids()) {
        const Entity* entity = pool.get_entity(id);
        if (!entity) continue;
        if (id != d.player_entity_id) {
            blocking_positions.push_back({entity->x, entity->y});
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

        // Stunned NPCs skip their turn: tick effects over a wait step and reschedule.
        auto fx_it = d.ledger->effects_data.find(entity_id);
        if (fx_it != d.ledger->effects_data.end() && Effects::is_stunned(fx_it->second)) {
            float wait = EffectTuning::STUN_WAIT_STEP;
            advance_entity_time(entity_id, wait);
            if (!pool.get_entity(entity_id)) continue; // died to bleed during tick
            entity->next_turn_time = base_time + wait;
            d.scheduler->push(entity_id, entity->next_turn_time);
            continue;
        }

        auto loco_it = d.ledger->locomotion_data.find(entity_id);
        if (loco_it == d.ledger->locomotion_data.end()) continue;
        auto& loco = loco_it->second;

        auto mem_it = d.ledger->perception_memory.find(entity_id);
        auto ai_it = d.ledger->ai_data.find(entity_id);
        if (mem_it == d.ledger->perception_memory.end() || ai_it == d.ledger->ai_data.end()) continue;
        auto& mem = mem_it->second;
        auto& ai = ai_it->second;

        // Acquire the nearest hostile target (faction-based). Falls back to wandering if none.
        int acquire_radius = d.bubble->get_world_bubble_radius();
        uint32_t target_id = find_nearest_hostile(entity_id, acquire_radius);
        Entity* target_entity = (target_id != EntityPool::INVALID_ID) ? pool.get_entity(target_id) : nullptr;
        Vector2i target_pos = target_entity ? Vector2i(target_entity->x, target_entity->y)
                                            : Vector2i(entity->x, entity->y);

        switch (ai.perception_tier) {
            case PerceptionTier::FULL_OCCLUSION:
                Perception::tick_full(mem, *entity, *d.bubble, target_pos);
                break;
            case PerceptionTier::RAYCAST:
                Perception::tick_raycast(mem, *entity, target_pos, *d.bubble, *tile_db);
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

        AIContext ctx{*entity, *d.bubble, *tile_db, mem, target_pos, find_path_fn};
        Intent intent = AIController::tick(ai, loco, ctx);

        float cost = 1.0f;
        if (intent.type == IntentType::MOVE) {
            // If the NPC tries to step onto its target's cell, attack that target instead.
            if (target_entity && intent.target.x == target_entity->x && intent.target.y == target_entity->y) {
                auto def_hp_it = d.ledger->health_data.find(target_id);
                if (def_hp_it != d.ledger->health_data.end() && def_hp_it->second.alive) {
                    CombatOutcome atk = resolve_entity_attack(entity_id, target_id);

                    if (atk.no_limbs) {
                        cost = ActionCost::ATTACK;
                        d.sink->on_combat_event(entity_id, target_id, 0.0f, "no_limbs", atk.verb, "");
                        finish_entity_action(entity_id, cost, base_time);
                        continue;
                    }

                    if (atk.exhausted) {
                        // Too tired to attack; rest this turn and recover.
                        cost = ActionCost::ATTACK;
                        finish_entity_action(entity_id, cost, base_time);
                        continue;
                    }

                    cost = ActionCost::ATTACK / (atk.speed > 0.0f ? atk.speed : 1.0f);

                    String result_str;
                    if (!atk.hit) result_str = "miss";
                    else if (atk.killed) result_str = atk.crit ? "crit_kill" : "kill";
                    else result_str = atk.crit ? "crit" : "hit";
                    d.sink->on_combat_event(entity_id, target_id, atk.damage, result_str, atk.verb, atk.part_name);
                    if (atk.killed) {
                        handle_entity_death(target_id, "combat", entity_id);
                    } else {
                        apply_attack_effects(entity_id, target_id, atk);
                    }
                }
                if (cost <= 0.0f) cost = 1.0f;
                finish_entity_action(entity_id, cost, base_time);
                continue;
            }

            float move_cost = ActionResolver::resolve_move(intent, *entity, *d.bubble, loco);
            if (move_cost > 0.0f) {
                cost = move_cost / loco.speed;
                auto stam_it = d.ledger->stamina_data.find(entity_id);
                if (stam_it != d.ledger->stamina_data.end()) {
                    cost *= Stamina::move_cost_multiplier(stam_it->second);
                }
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
        finish_entity_action(entity_id, cost, base_time);
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

Vector2i SimulationDirector::entity_chunk(uint32_t entity_id) const {
    const Entity* e = d.ledger->get_entity_pool().get_entity(entity_id);
    if (e) {
        int cs = WorldCoords::CHUNK_SIZE;
        return Vector2i(floor((float)e->x / cs), floor((float)e->y / cs));
    }
    return Vector2i();
}
