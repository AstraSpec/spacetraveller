#include "world_spawner.h"
#include "world_spawn_state.h"
#include "world_bubble.h"
#include "world_generator.h"
#include "turn_scheduler.h"
#include "data/loot_db.h"
#include "data/spawn_db.h"
#include "data/structure_db.h"
#include "data/tile_db.h"
#include "core/rng.h"
#include "core/tag_registry.h"
#include "core/world_coords.h"
#include "entities/entity_factory.h"
#include "entities/entity_pool.h"
#include <godot_cpp/variant/utility_functions.hpp>

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

static bool tile_allows_entity_spawn(uint16_t p_tile_id) {
    TileDb* tile_db = TileDb::get_singleton();
    if (!tile_db) return false;
    const TileInfo* tile = tile_db->get_tile_info(p_tile_id);
    return tile && !tile->solid;
}

static int floor_div_chunk(int p_value) {
    return (p_value >= 0)
        ? (p_value / WorldCoords::CHUNK_SIZE)
        : ((p_value - (WorldCoords::CHUNK_SIZE - 1)) / WorldCoords::CHUNK_SIZE);
}

static Vector2i resolve_structure_rule_local(const Vector2i& p_structure_pos, uint8_t p_rotation) {
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

static bool spawn_npc_at(
    const String& p_race_id,
    const String& p_job,
    const String& p_dialogue_profile,
    uint32_t p_world_seed,
    float p_spawn_turn_time,
    const Vector2i& p_pos,
    EntityLedger& p_ledger,
    WorldBubble& p_bubble,
    TurnScheduler& p_scheduler
) {
    if (p_race_id.is_empty()) return false;

    EntityFactory::SpawnOverrides overrides;
    overrides.job = p_job;
    overrides.dialogue_profile = p_dialogue_profile;

    return EntityFactory::create_npc(
        p_race_id,
        p_pos,
        static_cast<int>(p_world_seed),
        p_ledger,
        p_bubble,
        p_scheduler,
        overrides,
        p_spawn_turn_time
    ) != EntityPool::INVALID_ID;
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

    return spawn_npc_at(
        p_rule.race_id,
        p_rule.job,
        p_rule.dialogue_profile,
        p_world_seed,
        p_spawn_turn_time,
        p_pos,
        p_ledger,
        p_bubble,
        p_scheduler
    );
}

static bool spawn_with_structure_rule(
    uint32_t p_world_seed,
    float p_spawn_turn_time,
    const Vector2i& p_pos,
    const StructureRuleInfo& p_rule,
    WorldBubble& p_bubble,
    EntityLedger& p_ledger,
    TurnScheduler& p_scheduler
) {
    if (p_rule.entity.is_empty()) return false;
    uint16_t tile_id = p_bubble.query_tile_id(p_pos.x, p_pos.y);
    if (!tile_allows_entity_spawn(tile_id)) return false;

    return spawn_npc_at(
        p_rule.entity,
        p_rule.job,
        p_rule.dialogue_profile,
        p_world_seed,
        p_spawn_turn_time,
        p_pos,
        p_ledger,
        p_bubble,
        p_scheduler
    );
}

static uint64_t get_structure_rule_salt(const String& p_structure_id, const StructureRuleInfo& p_rule) {
    String rule_id = p_rule.id.is_empty() ? p_rule.type : p_rule.id;
    return (static_cast<uint64_t>(static_cast<uint32_t>(p_structure_id.hash())) << 32)
        ^ static_cast<uint32_t>(rule_id.hash());
}

static void apply_structure_loot_rule(
    uint32_t p_world_seed,
    const String& p_structure_id,
    const StructureRuleInfo& p_rule,
    const Vector2i& p_pos,
    WorldBubble& p_bubble
) {
    if (p_rule.loot_table == 0) return;

    LootDb* loot_db = LootDb::get_singleton();
    if (!loot_db) return;

    Rng::Seeded loot_rng = Rng::at(
        p_world_seed,
        p_pos,
        Rng::CONTAINER_LOOT,
        get_structure_rule_salt(p_structure_id, p_rule)
    );

    std::vector<LootStack> stacks;
    if (!loot_db->roll_table(p_rule.loot_table, loot_rng, stacks)) return;

    for (const LootStack& stack : stacks) {
        if (stack.item_id != 0 && stack.amount > 0) {
            p_bubble.drop_item(p_pos, stack.item_id, stack.amount);
        }
    }
}

static bool apply_structure_spawn_rule(
    uint32_t p_world_seed,
    float p_spawn_turn_time,
    const String& p_structure_id,
    const StructureRuleInfo& p_rule,
    const Vector2i& p_pos,
    WorldBubble& p_bubble,
    EntityLedger& p_ledger,
    TurnScheduler& p_scheduler
) {
    if (p_rule.entity.is_empty()) return false;

    if (p_bubble.get_entity_at(p_pos.x, p_pos.y) != nullptr
        || p_bubble.has_frozen_entity(WorldCoords::pack_coords(p_pos.x, p_pos.y))) {
        return false;
    }

    if (!spawn_with_structure_rule(p_world_seed, p_spawn_turn_time, p_pos, p_rule, p_bubble, p_ledger, p_scheduler)) {
        UtilityFunctions::push_error("[WorldSpawner] Failed structure spawn rule: ", p_structure_id, "/", p_rule.id, " entity=", p_rule.entity);
        return false;
    }

    return true;
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

    std::vector<const SpawnRuleInfo*> rules;
    for (uint64_t packed : p_newly_seen_cells) {
        Vector2i pos = WorldCoords::unpack_coords(packed);
        uint16_t chunk_id = p_generator.get_chunk_id_for_cell(pos.x, pos.y);

        String structure_id = p_generator.get_structure_id_for_chunk(chunk_id);
        if (!structure_id.is_empty()) {
            if (p_spawn_state.has_attempted(packed)) continue;

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

                bool spawned_from_rule = false;
                for (const StructureRuleInfo& rule : structure->rules) {
                    Vector2i rule_local = resolve_structure_rule_local(rule.pos, rotation);
                    if (rule_local != local_pos) continue;

                    if (rule.type == "loot_table") {
                        apply_structure_loot_rule(p_world_seed, structure_id, rule, pos, p_bubble);
                    } else if (!spawned_from_rule && rule.type == "spawn_point") {
                        spawned_from_rule = apply_structure_spawn_rule(p_world_seed, p_spawn_turn_time, structure_id, rule, pos, p_bubble, p_ledger, p_scheduler);
                    }
                }

                p_spawn_state.mark_attempted(packed);
            }
            continue;
        }

        if (!spawn_db) continue;
        if (p_spawn_state.has_attempted(packed)) continue;
        if (p_bubble.get_entity_at(pos.x, pos.y) != nullptr) continue;
        if (p_bubble.has_frozen_entity(packed)) continue;

        spawn_db->get_matching_rules(chunk_id, "free_cell", rules);
        if (rules.empty()) continue;

        p_spawn_state.mark_attempted(packed);
        Rng::Seeded rule_rng = Rng::at(p_world_seed, pos, Rng::SPAWN_RULE);
        const SpawnRuleInfo* rule = spawn_db->pick_weighted_rule(rules, rule_rng);
        if (rule) spawn_with_rule(p_world_seed, p_spawn_turn_time, pos, *rule, p_bubble, p_ledger, p_scheduler);
    }
}

}
