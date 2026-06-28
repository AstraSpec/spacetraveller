#include "world_spawner.h"
#include "world_spawn_state.h"
#include "world_bubble.h"
#include "entity_archive.h"
#include "world_generator.h"
#include "turn_scheduler.h"
#include "data/chunk_db.h"
#include "data/dungeon_db.h"
#include "data/loot_db.h"
#include "data/entity_group_db.h"
#include "data/structure_db.h"
#include "data/tile_db.h"
#include "core/id_registry.h"
#include "core/rng.h"
#include "core/tag_registry.h"
#include "core/world_coords.h"
#include "entities/entity_factory.h"
#include "entities/entity_pool.h"
#include "entities/entity_tracker.h"
#include "world/traversal_rules.h"
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

static constexpr uint64_t AMBIENT_LOOT_ROLL_SALT = 0x414D424C4F4F5452ULL; // "AMBLOOTR"
static constexpr uint64_t AMBIENT_ENTITY_ROLL_SALT = 0x414D42454E545254ULL; // "AMBENTRT"
static constexpr uint64_t WEB_SPIDER_ROLL_SALT = 0x5745425350494452ULL; // "WEBSPIDR"

static bool tile_allows_entity_spawn(const String& p_race_id, uint16_t p_tile_id) {
    TileDb* tile_db = TileDb::get_singleton();
    if (!tile_db) return false;
    const TileInfo* tile = tile_db->get_tile_info(p_tile_id);
    return tile && TraversalRules::can_race_enter(p_race_id, p_tile_id);
}

static bool tile_has_ground_tag(uint16_t p_tile_id) {
    TileDb* tile_db = TileDb::get_singleton();
    TagRegistry* tag_reg = TagRegistry::get_singleton();
    if (!tile_db || !tag_reg) return false;

    const TileInfo* tile = tile_db->get_tile_info(p_tile_id);
    const uint16_t ground_tag_id = tag_reg->get_tag_id("GROUND");
    return tile && ground_tag_id != 0 && TagRegistry::has_tag(ground_tag_id, tile->tags);
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
    const String& p_dialogue_id,
    const String& p_attitude,
    const String& p_ai_state,
    uint32_t p_world_seed,
    float p_spawn_turn_time,
    const Vector2i& p_pos,
    EntityLedger& p_ledger,
    EntityTracker& p_tracker,
    WorldBubble& p_bubble,
    TurnScheduler& p_scheduler
) {
    if (p_race_id.is_empty()) return false;

    EntityFactory::SpawnOverrides overrides;
    overrides.job = p_job;
    overrides.dialogue_id = p_dialogue_id;
    overrides.attitude = p_attitude;
    overrides.ai_state = p_ai_state;

    return EntityFactory::create_npc(
        p_race_id,
        p_pos,
        static_cast<int>(p_world_seed),
        p_ledger,
        p_tracker,
        p_bubble,
        p_scheduler,
        overrides,
        p_spawn_turn_time
    ) != EntityPool::INVALID_ID;
}

static bool spawn_with_structure_rule(
    uint32_t p_world_seed,
    float p_spawn_turn_time,
    const Vector2i& p_pos,
    const StructureRuleInfo& p_rule,
    WorldBubble& p_bubble,
    EntityLedger& p_ledger,
    EntityTracker& p_tracker,
    TurnScheduler& p_scheduler
) {
    if (p_rule.entity.is_empty()) return false;
    uint16_t tile_id = p_bubble.query_tile_id(p_pos.x, p_pos.y);
    if (!tile_allows_entity_spawn(p_rule.entity, tile_id)) return false;

    return spawn_npc_at(
        p_rule.entity,
        p_rule.job,
        p_rule.dialogue_id,
        p_rule.attitude,
        p_rule.ai_state,
        p_world_seed,
        p_spawn_turn_time,
        p_pos,
        p_ledger,
        p_tracker,
        p_bubble,
        p_scheduler
    );
}

static String rule_type_to_string(RuleType p_type) {
    switch (p_type) {
        case RuleType::SPAWN_ENTITY: return "spawn_entity";
        case RuleType::SPAWN_ENTITY_GROUP: return "spawn_entity_group";
        case RuleType::SPAWN_LOOT_TABLE: return "spawn_loot_table";
        case RuleType::SPAWN_ITEM: return "spawn_item";
        case RuleType::SET_METADATA: return "set_metadata";
    }
    return "unknown";
}

static uint64_t get_structure_group_salt(const String& p_structure_id, const StructureRuleInfo& p_rule) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(p_structure_id.hash())) << 32)
        ^ static_cast<uint32_t>(p_rule.entity_group.hash())
        ^ (static_cast<uint64_t>(static_cast<uint32_t>(p_rule.pos.x)) << 16)
        ^ static_cast<uint32_t>(p_rule.pos.y);
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

static uint64_t get_ambient_loot_salt(uint16_t p_biome_id, uint16_t p_loot_table, int p_z) {
    return AMBIENT_LOOT_ROLL_SALT
        ^ (static_cast<uint64_t>(p_biome_id) << 48)
        ^ (static_cast<uint64_t>(p_loot_table) << 32)
        ^ static_cast<uint32_t>(p_z);
}

static uint64_t get_ambient_entity_salt(uint16_t p_biome_id, const String& p_entity_group, int p_z) {
    return AMBIENT_ENTITY_ROLL_SALT
        ^ (static_cast<uint64_t>(p_biome_id) << 48)
        ^ static_cast<uint32_t>(p_entity_group.hash())
        ^ (static_cast<uint64_t>(static_cast<uint32_t>(p_z)) << 32);
}

static uint64_t get_dungeon_ambient_loot_salt(const String& p_dungeon_type, uint16_t p_loot_table, int p_z) {
    return AMBIENT_LOOT_ROLL_SALT
        ^ (static_cast<uint64_t>(static_cast<uint32_t>(p_dungeon_type.hash())) << 32)
        ^ static_cast<uint32_t>(p_loot_table)
        ^ (static_cast<uint64_t>(static_cast<uint32_t>(p_z)) << 48);
}

static uint64_t get_dungeon_ambient_entity_salt(const String& p_dungeon_type, const String& p_entity_group, int p_z) {
    return AMBIENT_ENTITY_ROLL_SALT
        ^ (static_cast<uint64_t>(static_cast<uint32_t>(p_dungeon_type.hash())) << 32)
        ^ static_cast<uint32_t>(p_entity_group.hash())
        ^ (static_cast<uint64_t>(static_cast<uint32_t>(p_z)) << 48);
}

static const DungeonInfo* get_dungeon_info_for_cell(
    WorldGenerator& p_generator,
    const Vector3i& p_pos3,
    uint32_t p_world_seed
) {
    String dungeon_type = p_generator.get_dungeon_type_for_cell(p_pos3.x, p_pos3.y, p_pos3.z, static_cast<int>(p_world_seed));
    if (dungeon_type.is_empty()) return nullptr;
    DungeonDb* dungeon_db = DungeonDb::get_singleton();
    return dungeon_db ? dungeon_db->get_dungeon_info(dungeon_type) : nullptr;
}

static void apply_ambient_chunk_loot(
    uint32_t p_world_seed,
    const Vector3i& p_pos3,
    uint16_t p_biome_id,
    uint16_t p_tile_id,
    const Vector2i& p_pos,
    WorldBubble& p_bubble
) {
    ChunkDb* chunk_db = ChunkDb::get_singleton();
    const ChunkInfo* chunk_info = chunk_db ? chunk_db->get_chunk_info(p_biome_id) : nullptr;
    if (!chunk_info || chunk_info->ambient_loot_table == 0 || chunk_info->ambient_loot_chance <= 0.0f) return;
    if (!tile_has_ground_tag(p_tile_id)) return;

    uint64_t salt = get_ambient_loot_salt(p_biome_id, chunk_info->ambient_loot_table, p_pos3.z);
    Rng::Seeded chance_rng = Rng::at(p_world_seed, p_pos, Rng::LOOT, salt);
    if (!chance_rng.chance(chunk_info->ambient_loot_chance)) return;

    roll_loot_table_at(p_world_seed, chunk_info->ambient_loot_table, salt, p_pos, p_bubble);
}

static void apply_dungeon_ambient_loot(
    uint32_t p_world_seed,
    const DungeonInfo& p_dungeon_info,
    const Vector3i& p_pos3,
    uint16_t p_tile_id,
    const Vector2i& p_pos,
    WorldBubble& p_bubble
) {
    if (p_dungeon_info.ambient_loot_table == 0 || p_dungeon_info.ambient_loot_chance <= 0.0f) return;
    if (!tile_has_ground_tag(p_tile_id)) return;

    uint64_t salt = get_dungeon_ambient_loot_salt(p_dungeon_info.id, p_dungeon_info.ambient_loot_table, p_pos3.z);
    Rng::Seeded chance_rng = Rng::at(p_world_seed, p_pos, Rng::LOOT, salt);
    if (!chance_rng.chance(p_dungeon_info.ambient_loot_chance)) return;

    roll_loot_table_at(p_world_seed, p_dungeon_info.ambient_loot_table, salt, p_pos, p_bubble);
}

static bool apply_ambient_chunk_entity(
    uint32_t p_world_seed,
    float p_spawn_turn_time,
    const Vector3i& p_pos3,
    uint16_t p_biome_id,
    uint16_t p_tile_id,
    const Vector2i& p_pos,
    WorldBubble& p_bubble,
    const EntityArchive& p_entity_archive,
    EntityLedger& p_ledger,
    EntityTracker& p_tracker,
    TurnScheduler& p_scheduler
) {
    ChunkDb* chunk_db = ChunkDb::get_singleton();
    const ChunkInfo* chunk_info = chunk_db ? chunk_db->get_chunk_info(p_biome_id) : nullptr;
    if (!chunk_info || chunk_info->ambient_entity_group.is_empty() || chunk_info->ambient_entity_chance <= 0.0f) return false;
    if (!tile_has_ground_tag(p_tile_id)) return false;
    if (p_bubble.get_entity_at(p_pos.x, p_pos.y) != nullptr) return false;
    if (p_entity_archive.has_frozen_entity(WorldCoords::pack_coords_3d(p_pos.x, p_pos.y, p_pos3.z))) return false;

    uint64_t salt = get_ambient_entity_salt(p_biome_id, chunk_info->ambient_entity_group, p_pos3.z);
    Rng::Seeded chance_rng = Rng::at(p_world_seed, p_pos, Rng::SPAWN, salt);
    if (!chance_rng.chance(chunk_info->ambient_entity_chance)) return false;

    EntityGroupDb* group_db = EntityGroupDb::get_singleton();
    if (!group_db) return false;

    Rng::Seeded group_rng = Rng::at(p_world_seed, p_pos, Rng::SPAWN_RULE, salt);
    const EntityGroupEntry* entry = group_db->pick_weighted_entry(chunk_info->ambient_entity_group, group_rng);
    if (entry && entry->none) return false;
    if (!entry || entry->entity.is_empty()) return false;
    if (!tile_allows_entity_spawn(entry->entity, p_tile_id)) return false;

    return spawn_npc_at(
        entry->entity,
        entry->job,
        entry->dialogue_id,
        entry->attitude,
        entry->ai_state,
        p_world_seed,
        p_spawn_turn_time,
        p_pos,
        p_ledger,
        p_tracker,
        p_bubble,
        p_scheduler
    );
}

static bool apply_dungeon_ambient_entity(
    uint32_t p_world_seed,
    float p_spawn_turn_time,
    const DungeonInfo& p_dungeon_info,
    const Vector3i& p_pos3,
    uint16_t p_tile_id,
    const Vector2i& p_pos,
    WorldBubble& p_bubble,
    const EntityArchive& p_entity_archive,
    EntityLedger& p_ledger,
    EntityTracker& p_tracker,
    TurnScheduler& p_scheduler
) {
    if (p_dungeon_info.ambient_entity_group.is_empty() || p_dungeon_info.ambient_entity_chance <= 0.0f) return false;
    if (!tile_has_ground_tag(p_tile_id)) return false;
    if (p_bubble.get_entity_at(p_pos.x, p_pos.y) != nullptr) return false;
    if (p_entity_archive.has_frozen_entity(WorldCoords::pack_coords_3d(p_pos.x, p_pos.y, p_pos3.z))) return false;

    uint64_t salt = get_dungeon_ambient_entity_salt(p_dungeon_info.id, p_dungeon_info.ambient_entity_group, p_pos3.z);
    Rng::Seeded chance_rng = Rng::at(p_world_seed, p_pos, Rng::SPAWN, salt);
    if (!chance_rng.chance(p_dungeon_info.ambient_entity_chance)) return false;

    EntityGroupDb* group_db = EntityGroupDb::get_singleton();
    if (!group_db) return false;

    Rng::Seeded group_rng = Rng::at(p_world_seed, p_pos, Rng::SPAWN_RULE, salt);
    const EntityGroupEntry* entry = group_db->pick_weighted_entry(p_dungeon_info.ambient_entity_group, group_rng);
    if (entry && entry->none) return false;
    if (!entry || entry->entity.is_empty()) return false;
    if (!tile_allows_entity_spawn(entry->entity, p_tile_id)) return false;

    return spawn_npc_at(
        entry->entity,
        entry->job,
        entry->dialogue_id,
        entry->attitude,
        entry->ai_state,
        p_world_seed,
        p_spawn_turn_time,
        p_pos,
        p_ledger,
        p_tracker,
        p_bubble,
        p_scheduler
    );
}

static bool apply_webbed_floor_spider_spawn(
    uint32_t p_world_seed,
    float p_spawn_turn_time,
    const Vector3i& p_pos3,
    uint16_t p_tile_id,
    const Vector2i& p_pos,
    WorldBubble& p_bubble,
    const EntityArchive& p_entity_archive,
    EntityLedger& p_ledger,
    EntityTracker& p_tracker,
    TurnScheduler& p_scheduler
) {
    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (!id_reg) return false;

    const uint16_t web_floor_id = id_reg->get_id("dungeon_floor_web");
    const uint16_t thick_web_floor_id = id_reg->get_id("dungeon_floor_web_thick");
    if (p_tile_id != web_floor_id && p_tile_id != thick_web_floor_id) return false;
    if (p_bubble.get_entity_at(p_pos.x, p_pos.y) != nullptr) return false;
    if (p_entity_archive.has_frozen_entity(WorldCoords::pack_coords_3d(p_pos.x, p_pos.y, p_pos3.z))) return false;

    const float chance = p_tile_id == thick_web_floor_id ? 0.17f : 0.10f;
    const uint64_t salt = WEB_SPIDER_ROLL_SALT
        ^ (static_cast<uint64_t>(p_tile_id) << 32)
        ^ static_cast<uint32_t>(p_pos3.z);
    Rng::Seeded spawn_rng = Rng::at(p_world_seed, p_pos, Rng::SPAWN, salt);
    if (!spawn_rng.chance(chance)) return false;

    const String pale_spider = "pale_spider";
    if (!tile_allows_entity_spawn(pale_spider, p_tile_id)) return false;
    return spawn_npc_at(
        pale_spider,
        "monster",
        "",
        "",
        "",
        p_world_seed,
        p_spawn_turn_time,
        p_pos,
        p_ledger,
        p_tracker,
        p_bubble,
        p_scheduler
    );
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
    EntityTracker& p_tracker,
    TurnScheduler& p_scheduler
) {
    if (p_rule.entity.is_empty()) return false;

    if (p_bubble.get_entity_at(p_pos.x, p_pos.y) != nullptr
        || p_entity_archive.has_frozen_entity(WorldCoords::pack_coords_3d(p_pos.x, p_pos.y, p_bubble.get_active_z()))) {
        return false;
    }

    if (!spawn_with_structure_rule(p_world_seed, p_spawn_turn_time, p_pos, p_rule, p_bubble, p_ledger, p_tracker, p_scheduler)) {
        UtilityFunctions::push_error("[WorldSpawner] Failed structure spawn rule: ", p_structure_id, " type=", rule_type_to_string(p_rule.type), " pos=", p_rule.pos, " entity=", p_rule.entity);
        return false;
    }

    return true;
}

static bool apply_structure_spawn_group_rule(
    uint32_t p_world_seed,
    float p_spawn_turn_time,
    const String& p_structure_id,
    const StructureRuleInfo& p_rule,
    const Vector2i& p_pos,
    WorldBubble& p_bubble,
    const EntityArchive& p_entity_archive,
    EntityLedger& p_ledger,
    EntityTracker& p_tracker,
    TurnScheduler& p_scheduler
) {
    if (p_rule.entity_group.is_empty()) return false;

    if (p_bubble.get_entity_at(p_pos.x, p_pos.y) != nullptr
        || p_entity_archive.has_frozen_entity(WorldCoords::pack_coords_3d(p_pos.x, p_pos.y, p_bubble.get_active_z()))) {
        return false;
    }

    EntityGroupDb* group_db = EntityGroupDb::get_singleton();
    if (!group_db) return false;

    Rng::Seeded group_rng = Rng::at(p_world_seed, p_pos, Rng::SPAWN, get_structure_group_salt(p_structure_id, p_rule));
    const EntityGroupEntry* entry = group_db->pick_weighted_entry(p_rule.entity_group, group_rng);
    if (entry && entry->none) return false;
    if (!entry || entry->entity.is_empty()) return false;

    uint16_t tile_id = p_bubble.query_tile_id(p_pos.x, p_pos.y);
    if (!tile_allows_entity_spawn(entry->entity, tile_id)) return false;

    if (!spawn_npc_at(
        entry->entity,
        entry->job,
        entry->dialogue_id,
        entry->attitude,
        entry->ai_state,
        p_world_seed,
        p_spawn_turn_time,
        p_pos,
        p_ledger,
        p_tracker,
        p_bubble,
        p_scheduler
    )) {
        UtilityFunctions::push_error(
            "[WorldSpawner] Failed structure spawn group rule: ",
            p_structure_id,
            " group=",
            p_rule.entity_group,
            " picked_entity=",
            entry->entity,
            " pos=",
            p_rule.pos
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
    EntityTracker& p_tracker,
    TurnScheduler& p_scheduler,
    WorldSpawnState& p_spawn_state
) {
    for (uint64_t packed : p_newly_seen_cells) {
        Vector3i pos3 = WorldCoords::unpack_coords_3d(packed);
        Vector2i pos(pos3.x, pos3.y);

        DungeonStructureContext dungeon_structure = p_generator.get_dungeon_structure_context(pos.x, pos.y, pos3.z, static_cast<int>(p_world_seed));
        if (dungeon_structure.valid) {
            if (p_spawn_state.has_attempted(packed)) continue;

            StructureDb* structure_db = StructureDb::get_singleton();
            const StructureInfo* structure = structure_db ? structure_db->get_structure_info(dungeon_structure.structure_id) : nullptr;
            if (structure) {
                auto level_it = structure->levels.find(dungeon_structure.local_z);
                if (level_it != structure->levels.end()) {
                    const StructureLevelInfo& level = level_it->second;

                    bool spawned_from_rule = false;
                    bool custom_loot_rule_matched = false;
                    for (const StructureRuleInfo& rule : level.rules) {
                        if (rule.pos != dungeon_structure.local_pos) continue;

                        switch (rule.type) {
                            case RuleType::SPAWN_LOOT_TABLE:
                                custom_loot_rule_matched = true;
                                apply_structure_loot_rule(p_world_seed, dungeon_structure.structure_id, rule, pos, p_bubble);
                                break;
                            case RuleType::SPAWN_ITEM:
                                custom_loot_rule_matched = true;
                                apply_structure_spawn_item(pos, rule, p_bubble);
                                break;
                            case RuleType::SPAWN_ENTITY:
                                if (!spawned_from_rule) {
                                    spawned_from_rule = apply_structure_spawn_rule(p_world_seed, p_spawn_turn_time, dungeon_structure.structure_id, rule, pos, p_bubble, p_entity_archive, p_ledger, p_tracker, p_scheduler);
                                }
                                break;
                            case RuleType::SPAWN_ENTITY_GROUP:
                                if (!spawned_from_rule) {
                                    spawned_from_rule = apply_structure_spawn_group_rule(p_world_seed, p_spawn_turn_time, dungeon_structure.structure_id, rule, pos, p_bubble, p_entity_archive, p_ledger, p_tracker, p_scheduler);
                                }
                                break;
                            case RuleType::SET_METADATA:
                                p_bubble.set_tile_metadata(pos, rule.params);
                                break;
                        }
                    }

                    const uint16_t tile_id = p_bubble.query_tile_id_at_z(pos.x, pos.y, pos3.z);
                    const DungeonInfo* dungeon_info = get_dungeon_info_for_cell(p_generator, pos3, p_world_seed);
                    if (!custom_loot_rule_matched) {
                        apply_tile_spawn_loot(p_world_seed, dungeon_structure.structure_id, tile_id, pos, p_bubble);

                        if (dungeon_info) {
                            apply_dungeon_ambient_loot(p_world_seed, *dungeon_info, pos3, tile_id, pos, p_bubble);
                        } else {
                            const uint16_t biome_id = p_generator.get_biome_id_for_cell(pos.x, pos.y, pos3.z, static_cast<int>(p_world_seed));
                            apply_ambient_chunk_loot(p_world_seed, pos3, biome_id, tile_id, pos, p_bubble);
                        }
                    }
                    if (!spawned_from_rule) {
                        const bool spawned_web_spider = apply_webbed_floor_spider_spawn(p_world_seed, p_spawn_turn_time, pos3, tile_id, pos, p_bubble, p_entity_archive, p_ledger, p_tracker, p_scheduler);
                        if (!spawned_web_spider) {
                            if (dungeon_info) {
                                apply_dungeon_ambient_entity(p_world_seed, p_spawn_turn_time, *dungeon_info, pos3, tile_id, pos, p_bubble, p_entity_archive, p_ledger, p_tracker, p_scheduler);
                            } else {
                                const uint16_t biome_id = p_generator.get_biome_id_for_cell(pos.x, pos.y, pos3.z, static_cast<int>(p_world_seed));
                                apply_ambient_chunk_entity(p_world_seed, p_spawn_turn_time, pos3, biome_id, tile_id, pos, p_bubble, p_entity_archive, p_ledger, p_tracker, p_scheduler);
                            }
                        }
                    }
                }

                p_spawn_state.mark_attempted(packed);
            }
            continue;
        }

        SurfaceFeatureContext surface_feature = p_generator.get_surface_feature_context(pos.x, pos.y, pos3.z, static_cast<int>(p_world_seed));
        if (surface_feature.valid) {
            if (p_spawn_state.has_attempted(packed)) continue;

            StructureDb* structure_db = StructureDb::get_singleton();
            const StructureInfo* structure = structure_db ? structure_db->get_structure_info(surface_feature.structure_id) : nullptr;
            if (structure) {
                auto level_it = structure->levels.find(surface_feature.local_z);
                if (level_it != structure->levels.end()) {
                    const StructureLevelInfo& level = level_it->second;

                    bool spawned_from_rule = false;
                    bool custom_loot_rule_matched = false;
                    for (const StructureRuleInfo& rule : level.rules) {
                        if (rule.pos != surface_feature.local_pos) continue;

                        switch (rule.type) {
                            case RuleType::SPAWN_LOOT_TABLE:
                                custom_loot_rule_matched = true;
                                apply_structure_loot_rule(p_world_seed, surface_feature.structure_id, rule, pos, p_bubble);
                                break;
                            case RuleType::SPAWN_ITEM:
                                custom_loot_rule_matched = true;
                                apply_structure_spawn_item(pos, rule, p_bubble);
                                break;
                            case RuleType::SPAWN_ENTITY:
                                if (!spawned_from_rule) {
                                    spawned_from_rule = apply_structure_spawn_rule(p_world_seed, p_spawn_turn_time, surface_feature.structure_id, rule, pos, p_bubble, p_entity_archive, p_ledger, p_tracker, p_scheduler);
                                }
                                break;
                            case RuleType::SPAWN_ENTITY_GROUP:
                                if (!spawned_from_rule) {
                                    spawned_from_rule = apply_structure_spawn_group_rule(p_world_seed, p_spawn_turn_time, surface_feature.structure_id, rule, pos, p_bubble, p_entity_archive, p_ledger, p_tracker, p_scheduler);
                                }
                                break;
                            case RuleType::SET_METADATA:
                                p_bubble.set_tile_metadata(pos, rule.params);
                                break;
                        }
                    }

                    if (!custom_loot_rule_matched) {
                        uint16_t tile_id = p_bubble.query_tile_id_at_z(pos.x, pos.y, pos3.z);
                        apply_tile_spawn_loot(p_world_seed, surface_feature.structure_id, tile_id, pos, p_bubble);
                    }
                }

                p_spawn_state.mark_attempted(packed);
            }
            continue;
        }

        String structure_id = p_generator.get_structure_id_for_cell(pos.x, pos.y, pos3.z, static_cast<int>(p_world_seed));
        if (!structure_id.is_empty()) {
            if (p_spawn_state.has_attempted(packed)) continue;

            StructureDb* structure_db = StructureDb::get_singleton();
            const StructureInfo* structure = structure_db ? structure_db->get_structure_info(structure_id) : nullptr;
            if (structure) {
                auto level_it = structure->levels.find(pos3.z);
                if (level_it == structure->levels.end()) {
                    p_spawn_state.mark_attempted(packed);
                    continue;
                }
                const StructureLevelInfo& level = level_it->second;

                int chunk_x = floor_div_chunk(pos.x);
                int chunk_y = floor_div_chunk(pos.y);
                Vector2i local_pos(
                    pos.x - chunk_x * WorldCoords::CHUNK_SIZE,
                    pos.y - chunk_y * WorldCoords::CHUNK_SIZE
                );
                uint8_t rotation = p_generator.get_chunk_rotation_for_cell(pos.x, pos.y);

                bool spawned_from_rule = false;
                bool custom_loot_rule_matched = false;
                for (const StructureRuleInfo& rule : level.rules) {
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
                                spawned_from_rule = apply_structure_spawn_rule(p_world_seed, p_spawn_turn_time, structure_id, rule, pos, p_bubble, p_entity_archive, p_ledger, p_tracker, p_scheduler);
                            }
                            break;
                        case RuleType::SPAWN_ENTITY_GROUP:
                            if (!spawned_from_rule) {
                                spawned_from_rule = apply_structure_spawn_group_rule(p_world_seed, p_spawn_turn_time, structure_id, rule, pos, p_bubble, p_entity_archive, p_ledger, p_tracker, p_scheduler);
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

        if (p_spawn_state.has_attempted(packed)) continue;
        p_spawn_state.mark_attempted(packed);

        uint16_t tile_id = p_bubble.query_tile_id_at_z(pos.x, pos.y, pos3.z);
        const DungeonInfo* dungeon_info = get_dungeon_info_for_cell(p_generator, pos3, p_world_seed);
        if (dungeon_info) {
            apply_dungeon_ambient_loot(p_world_seed, *dungeon_info, pos3, tile_id, pos, p_bubble);
        } else {
            uint16_t biome_id = p_generator.get_biome_id_for_cell(pos.x, pos.y, pos3.z, static_cast<int>(p_world_seed));
            apply_ambient_chunk_loot(p_world_seed, pos3, biome_id, tile_id, pos, p_bubble);
        }
        const bool spawned_web_spider = apply_webbed_floor_spider_spawn(p_world_seed, p_spawn_turn_time, pos3, tile_id, pos, p_bubble, p_entity_archive, p_ledger, p_tracker, p_scheduler);
        if (!spawned_web_spider) {
            if (dungeon_info) {
                apply_dungeon_ambient_entity(p_world_seed, p_spawn_turn_time, *dungeon_info, pos3, tile_id, pos, p_bubble, p_entity_archive, p_ledger, p_tracker, p_scheduler);
            } else {
                uint16_t biome_id = p_generator.get_biome_id_for_cell(pos.x, pos.y, pos3.z, static_cast<int>(p_world_seed));
                apply_ambient_chunk_entity(p_world_seed, p_spawn_turn_time, pos3, biome_id, tile_id, pos, p_bubble, p_entity_archive, p_ledger, p_tracker, p_scheduler);
            }
        }
    }
}

}
