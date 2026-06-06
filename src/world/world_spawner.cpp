#include "world_spawner.h"
#include "world_spawn_state.h"
#include "world_bubble.h"
#include "world_generator.h"
#include "turn_scheduler.h"
#include "data/spawn_db.h"
#include "data/structure_db.h"
#include "data/tile_db.h"
#include "core/rng.h"
#include "core/tag_registry.h"
#include "core/world_coords.h"
#include "entities/entity_factory.h"
#include "entities/entity_pool.h"

namespace godot {

static bool tile_allows_rule(uint16_t p_tile_id, const SpawnRuleInfo& p_rule) {
    TileDb* tile_db = TileDb::get_singleton();
    if (!tile_db) return false;
    const TileInfo* tile = tile_db->get_tile_info(p_tile_id);
    if (!tile) return false;
    if (tile->solid) return false;
    if (!p_rule.tile_tags.empty() && !TagRegistry::has_tag_any(tile->tags, p_rule.tile_tags)) return false;
    return true;
}

static int floor_div_chunk(int p_value) {
    return (p_value >= 0)
        ? (p_value / WorldCoords::CHUNK_SIZE)
        : ((p_value - (WorldCoords::CHUNK_SIZE - 1)) / WorldCoords::CHUNK_SIZE);
}

static Vector2i resolve_spawn_point_local(const Vector2i& p_structure_pos, uint8_t p_rotation) {
    int max_coord = WorldCoords::CHUNK_SIZE - 1;
    switch (p_rotation) {
        case WorldCoords::ROT_WEST:
            return Vector2i(max_coord - p_structure_pos.y, p_structure_pos.x);
        case WorldCoords::ROT_NORTH:
            return Vector2i(max_coord - p_structure_pos.x, max_coord - p_structure_pos.y);
        case WorldCoords::ROT_EAST:
            return Vector2i(p_structure_pos.y, max_coord - p_structure_pos.x);
        case WorldCoords::ROT_SOUTH:
        default:
            return p_structure_pos;
    }
}

static bool spawn_with_rule(
    uint32_t p_world_seed,
    float p_spawn_turn_time,
    const Vector2i& p_pos,
    const SpawnRuleInfo& p_rule,
    WorldBubble& p_bubble,
    EntityLedger& p_ledger,
    TurnScheduler& p_scheduler
) {
    uint16_t tile_id = p_bubble.query_tile_id(p_pos.x, p_pos.y);
    if (!tile_allows_rule(tile_id, p_rule)) return false;

    Rng::Seeded spawn_rng = Rng::at(p_world_seed, p_pos, Rng::SPAWN);
    if (!spawn_rng.chance(p_rule.chance)) return false;

    EntityFactory::SpawnOverrides overrides;
    overrides.job = p_rule.job;
    overrides.dialogue_profile = p_rule.dialogue_profile;
    EntityFactory::create_npc(p_rule.race_id, p_pos, static_cast<int>(p_world_seed), p_ledger, p_bubble, p_scheduler, overrides, p_spawn_turn_time);
    return true;
}

static bool rule_matches_spawn_point(const SpawnRuleInfo& p_rule, const StructureSpawnPoint& p_point) {
    return p_rule.spawn_point_tags.empty() || TagRegistry::has_tag_any(p_point.tags, p_rule.spawn_point_tags);
}

void WorldSpawner::spawn_for_newly_seen_cells(
    uint32_t p_world_seed,
    float p_spawn_turn_time,
    const std::vector<uint64_t>& p_newly_seen_cells,
    WorldGenerator& p_generator,
    WorldBubble& p_bubble,
    EntityLedger& p_ledger,
    TurnScheduler& p_scheduler,
    WorldSpawnState& p_spawn_state
) {
    SpawnDb* spawn_db = SpawnDb::get_singleton();
    if (!spawn_db) return;

    std::vector<const SpawnRuleInfo*> rules;
    for (uint64_t packed : p_newly_seen_cells) {
        Vector2i pos = WorldCoords::unpack_coords(packed);
        if (p_spawn_state.has_attempted(packed)) continue;
        if (p_bubble.get_entity_at(pos.x, pos.y) != nullptr) continue;
        if (p_bubble.has_frozen_entity(packed)) continue;

        uint16_t chunk_id = p_generator.get_chunk_id_for_cell(pos.x, pos.y);

        String structure_id = p_generator.get_structure_id_for_chunk(chunk_id);
        if (!structure_id.is_empty()) {
            StructureDb* structure_db = StructureDb::get_singleton();
            const StructureInfo* structure = structure_db ? structure_db->get_structure_info(structure_id) : nullptr;
            if (structure) {
                int chunk_x = floor_div_chunk(pos.x);
                int chunk_y = floor_div_chunk(pos.y);
                Vector2i local_pos(
                    pos.x - chunk_x * WorldCoords::CHUNK_SIZE,
                    pos.y - chunk_y * WorldCoords::CHUNK_SIZE
                );
                uint8_t rotation = p_generator.get_chunk_rotation_for_cell(pos.x, pos.y);

                for (const StructureSpawnPoint& point : structure->spawn_points) {
                    Vector2i point_local = resolve_spawn_point_local(point.pos, rotation);
                    if (point_local != local_pos) continue;

                    spawn_db->get_matching_rules(chunk_id, "spawn_points", rules);
                    std::vector<const SpawnRuleInfo*> point_rules;
                    for (const SpawnRuleInfo* rule : rules) {
                        if (rule && rule_matches_spawn_point(*rule, point)) {
                            point_rules.push_back(rule);
                        }
                    }
                    if (point_rules.empty()) break;

                    p_spawn_state.mark_attempted(packed);
                    Rng::Seeded rule_rng = Rng::at(p_world_seed, pos, Rng::SPAWN_RULE);
                    const SpawnRuleInfo* rule = spawn_db->pick_weighted_rule(point_rules, rule_rng);
                    if (rule) spawn_with_rule(p_world_seed, p_spawn_turn_time, pos, *rule, p_bubble, p_ledger, p_scheduler);
                    break;
                }
            }
            continue;
        }

        spawn_db->get_matching_rules(chunk_id, "free_cell", rules);
        if (rules.empty()) continue;

        p_spawn_state.mark_attempted(packed);
        Rng::Seeded rule_rng = Rng::at(p_world_seed, pos, Rng::SPAWN_RULE);
        const SpawnRuleInfo* rule = spawn_db->pick_weighted_rule(rules, rule_rng);
        if (rule) spawn_with_rule(p_world_seed, p_spawn_turn_time, pos, *rule, p_bubble, p_ledger, p_scheduler);
    }
}

}
