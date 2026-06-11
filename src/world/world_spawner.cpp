#include "world_spawner.h"
#include "world_spawn_state.h"
#include "world_bubble.h"
#include "entity_archive.h"
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
#include "world/traversal_rules.h"
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

static bool tile_allows_rule(uint16_t p_tile_id, const SpawnRuleInfo& p_rule) {
    TileDb* tile_db = TileDb::get_singleton();
    if (!tile_db) return false;
    const TileInfo* tile = tile_db->get_tile_info(p_tile_id);
    if (!tile) return false;
    if (!TraversalRules::can_race_enter(p_rule.race_id, p_tile_id)) return false;
    if (!p_rule.tile_tags.empty() && !TagRegistry::has_tag_any(tile->tags, p_rule.tile_tags)) return false;
    return true;
}

static bool tile_allows_entity_spawn(const String& p_race_id, uint16_t p_tile_id) {
    TileDb* tile_db = TileDb::get_singleton();
    if (!tile_db) return false;
    const TileInfo* tile = tile_db->get_tile_info(p_tile_id);
    return tile && TraversalRules::can_race_enter(p_race_id, p_tile_id);
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
    const String& p_attitude,
    const String& p_role,
    const String& p_ai_state,
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
    overrides.attitude = p_attitude;
    overrides.role = p_role;
    overrides.ai_state = p_ai_state;

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
        p_rule.attitude,
        p_rule.role,
        p_rule.ai_state,
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
    if (!tile_allows_entity_spawn(p_rule.entity, tile_id)) return false;

    return spawn_npc_at(
        p_rule.entity,
        p_rule.job,
        p_rule.dialogue_profile,
        p_rule.attitude,
        p_rule.role,
        p_rule.ai_state,
        p_world_seed,
        p_spawn_turn_time,
        p_pos,
        p_ledger,
        p_bubble,
        p_scheduler
    );
}

static String rule_type_to_string(RuleType p_type) {
    switch (p_type) {
        case RuleType::SPAWN_ENTITY: return "spawn_entity";
        case RuleType::SPAWN_LOOT_TABLE: return "spawn_loot_table";
        case RuleType::SPAWN_ITEM: return "spawn_item";
        case RuleType::SET_METADATA: return "set_metadata";
    }
    return "unknown";
}

static uint64_t get_structure_rule_salt(const String& p_structure_id, const StructureRuleInfo& p_rule) {
    String rule_key = rule_type_to_string(p_rule.type)
        + String(":")
        + String::num_int64(p_rule.pos.x)
        + String(",")
        + String::num_int64(p_rule.pos.y);
    return (static_cast<uint64_t>(static_cast<uint32_t>(p_structure_id.hash())) << 32)
        ^ static_cast<uint32_t>(rule_key.hash());
}

static uint64_t get_tile_spawn_loot_salt(const String& p_structure_id, uint16_t p_tile_id) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(p_structure_id.hash())) << 32)
        ^ static_cast<uint32_t>(p_tile_id);
}

static void roll_loot_table_at(
    uint32_t p_world_seed,
    uint16_t p_loot_table,
    uint64_t p_salt,
    const Vector2i& p_pos,
    WorldBubble& p_bubble
) {
    if (p_loot_table == 0) return;

    LootDb* loot_db = LootDb::get_singleton();
    if (!loot_db) return;

    Rng::Seeded loot_rng = Rng::at(
        p_world_seed,
        p_pos,
        Rng::CONTAINER_LOOT,
        p_salt
    );

    std::vector<LootStack> stacks;
    if (!loot_db->roll_table(p_loot_table, loot_rng, stacks)) return;

    for (const LootStack& stack : stacks) {
        if (stack.item_id != 0 && stack.amount > 0) {
            p_bubble.drop_item(p_pos, stack.item_id, stack.amount);
        }
    }
}

static void apply_structure_loot_rule(
    uint32_t p_world_seed,
    const String& p_structure_id,
    const StructureRuleInfo& p_rule,
    const Vector2i& p_pos,
    WorldBubble& p_bubble
) {
    roll_loot_table_at(p_world_seed, p_rule.loot_table, get_structure_rule_salt(p_structure_id, p_rule), p_pos, p_bubble);
}

static void apply_structure_spawn_item(
    const Vector2i& p_pos,
    const StructureRuleInfo& p_rule,
    WorldBubble& p_bubble
) {
    if (p_rule.item_id == 0 || p_rule.amount <= 0) return;
    p_bubble.drop_item(p_pos, p_rule.item_id, p_rule.amount);
}

static void apply_tile_spawn_loot(
    uint32_t p_world_seed,
    const String& p_structure_id,
    uint16_t p_tile_id,
    const Vector2i& p_pos,
    WorldBubble& p_bubble
) {
    TileDb* tile_db = TileDb::get_singleton();
    const TileInfo* tile = tile_db ? tile_db->get_tile_info(p_tile_id) : nullptr;
    if (!tile || tile->spawn_loot_table == 0) return;
    roll_loot_table_at(p_world_seed, tile->spawn_loot_table, get_tile_spawn_loot_salt(p_structure_id, p_tile_id), p_pos, p_bubble);
}

static bool apply_structure_spawn_rule(
    uint32_t p_world_seed,
    float p_spawn_turn_time,
    const String& p_structure_id,
    const StructureRuleInfo& p_rule,
    const Vector2i& p_pos,
    WorldBubble& p_bubble,
    const EntityArchive& p_entity_archive,
    EntityLedger& p_ledger,
    TurnScheduler& p_scheduler
) {
    if (p_rule.entity.is_empty()) return false;

    if (p_bubble.get_entity_at(p_pos.x, p_pos.y) != nullptr
        || p_entity_archive.has_frozen_entity(WorldCoords::pack_coords(p_pos.x, p_pos.y))) {
        return false;
    }

    if (!spawn_with_structure_rule(p_world_seed, p_spawn_turn_time, p_pos, p_rule, p_bubble, p_ledger, p_scheduler)) {
        UtilityFunctions::push_error(
            "[WorldSpawner] Failed structure spawn rule: ",
            p_structure_id,
            " type=",
            rule_type_to_string(p_rule.type),
            " pos=",
            p_rule.pos,
            " entity=",
            p_rule.entity
        );
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
    const EntityArchive& p_entity_archive,
    EntityLedger& p_ledger,
    TurnScheduler& p_scheduler,
    WorldSpawnState& p_spawn_state
) {
    SpawnDb* spawn_db = SpawnDb::get_singleton();

    std::vector<const SpawnRuleInfo*> rules;
    for (uint64_t packed : p_newly_seen_cells) {
        Vector2i pos = WorldCoords::unpack_coords(packed);
        uint16_t chunk_id = p_generator.get_chunk_id_for_cell(pos.x, pos.y);

        String structure_id = p_generator.get_structure_id_for_cell(pos.x, pos.y, static_cast<int>(p_world_seed));
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
                bool custom_loot_rule_matched = false;
                for (const StructureRuleInfo& rule : structure->rules) {
                    Vector2i rule_local = resolve_structure_rule_local(rule.pos, rotation);
                    if (rule_local != local_pos) continue;

                    switch (rule.type) {
                        case RuleType::SPAWN_LOOT_TABLE:
                            custom_loot_rule_matched = true;
                            apply_structure_loot_rule(p_world_seed, structure_id, rule, pos, p_bubble);
                            break;
                        case RuleType::SPAWN_ITEM:
                            custom_loot_rule_matched = true;
                            apply_structure_spawn_item(pos, rule, p_bubble);
                            break;
                        case RuleType::SPAWN_ENTITY:
                            if (!spawned_from_rule) {
                                spawned_from_rule = apply_structure_spawn_rule(p_world_seed, p_spawn_turn_time, structure_id, rule, pos, p_bubble, p_entity_archive, p_ledger, p_scheduler);
                            }
                            break;
                        case RuleType::SET_METADATA:
                            p_bubble.set_tile_metadata(pos, rule.params);
                            break;
                    }
                }

                if (!custom_loot_rule_matched) {
                    uint16_t tile_id = p_bubble.query_tile_id(pos.x, pos.y);
                    apply_tile_spawn_loot(p_world_seed, structure_id, tile_id, pos, p_bubble);
                }

                p_spawn_state.mark_attempted(packed);
            }
            continue;
        }

        if (!spawn_db) continue;
        if (p_spawn_state.has_attempted(packed)) continue;
        if (p_bubble.get_entity_at(pos.x, pos.y) != nullptr) continue;
        if (p_entity_archive.has_frozen_entity(packed)) continue;

        spawn_db->get_matching_rules(chunk_id, "free_cell", rules);
        if (rules.empty()) continue;

        p_spawn_state.mark_attempted(packed);
        Rng::Seeded rule_rng = Rng::at(p_world_seed, pos, Rng::SPAWN_RULE);
        const SpawnRuleInfo* rule = spawn_db->pick_weighted_rule(rules, rule_rng);
        if (rule) spawn_with_rule(p_world_seed, p_spawn_turn_time, pos, *rule, p_bubble, p_ledger, p_scheduler);
    }
}

}
