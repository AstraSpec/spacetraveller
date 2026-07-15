#include "world_generator.h"
#include "data/structure_db.h"
#include "core/id_registry.h"
#include "data/chunk_db.h"
#include "data/dungeon_db.h"
#include "data/feature_db.h"
#include "data/tile_db.h"
#include "data/tile_group_db.h"
#include "core/tag_registry.h"
#include "city_generation.h"
#include "cave_generator.h"
#include "dungeon_generator.h"
#include "gen_grid.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>
#include <cmath>

using namespace godot;

static int floor_div_chunk(int p_value) {
    return (p_value >= 0)
        ? (p_value / WorldCoords::CHUNK_SIZE)
        : ((p_value - (WorldCoords::CHUNK_SIZE - 1)) / WorldCoords::CHUNK_SIZE);
}

static Vector2i variant_to_vector2i(const Variant& p_value, const Vector2i& p_fallback = Vector2i()) {
    if (p_value.get_type() != Variant::ARRAY) {
        return p_fallback;
    }
    Array values = p_value;
    if (values.size() != 2) {
        return p_fallback;
    }
    return Vector2i(static_cast<int>(values[0]), static_cast<int>(values[1]));
}

static bool city_spawn_zone_contains(
    int p_city_distance_sq,
    int p_max_city_distance_sq,
    float p_city_zone_min,
    float p_city_zone_max
) {
    const float city_distance_sq = static_cast<float>(p_city_distance_sq);
    const float max_city_distance_sq = static_cast<float>(p_max_city_distance_sq);
    if (p_max_city_distance_sq > 0) {
        const float min_distance_sq = p_city_zone_min * p_city_zone_min * max_city_distance_sq;
        const float max_distance_sq = p_city_zone_max * p_city_zone_max * max_city_distance_sq;
        return city_distance_sq >= min_distance_sq && city_distance_sq <= max_distance_sq;
    }
    return p_city_zone_min <= 0.0f;
}

WorldGenerator::WorldGenerator() {}
WorldGenerator::~WorldGenerator() = default;

static uint64_t dungeon_dynamic_cell_key(int p_x, int p_y) {
    return WorldCoords::pack_coords(p_x, p_y);
}

static Vector2i resolve_surface_feature_source_pos(
    int p_local_x,
    int p_local_y,
    const Vector2i& p_source_size,
    uint8_t p_rotation
) {
    switch (p_rotation) {
        case WorldCoords::ROT_WEST:
            return Vector2i(p_local_y, p_source_size.y - 1 - p_local_x);
        case WorldCoords::ROT_EAST:
            return Vector2i(p_source_size.x - 1 - p_local_y, p_local_x);
        case WorldCoords::ROT_NORTH:
            return Vector2i(p_source_size.x - 1 - p_local_x, p_source_size.y - 1 - p_local_y);
        case WorldCoords::ROT_SOUTH:
        default:
            return Vector2i(p_local_x, p_local_y);
    }
}

static Vector2i resolve_surface_feature_placed_pos(
    int p_source_x,
    int p_source_y,
    const Vector2i& p_source_size,
    uint8_t p_rotation
) {
    switch (p_rotation) {
        case WorldCoords::ROT_WEST:
            return Vector2i(p_source_size.y - 1 - p_source_y, p_source_x);
        case WorldCoords::ROT_EAST:
            return Vector2i(p_source_y, p_source_size.x - 1 - p_source_x);
        case WorldCoords::ROT_NORTH:
            return Vector2i(p_source_size.x - 1 - p_source_x, p_source_size.y - 1 - p_source_y);
        case WorldCoords::ROT_SOUTH:
        default:
            return Vector2i(p_source_x, p_source_y);
    }
}

static bool surface_feature_rotation_swaps_size(uint8_t p_rotation) {
    return p_rotation == WorldCoords::ROT_EAST || p_rotation == WorldCoords::ROT_WEST;
}

static uint8_t compose_surface_feature_rotation(uint8_t p_local_rotation, uint8_t p_chunk_rotation) {
    return static_cast<uint8_t>((p_local_rotation + p_chunk_rotation) & WorldCoords::ROTATION_MASK);
}

static Vector2i get_rotated_surface_feature_size(const Vector2i& p_size, uint8_t p_rotation) {
    return surface_feature_rotation_swaps_size(p_rotation) ? Vector2i(p_size.y, p_size.x) : p_size;
}

static Vector2i rotate_chunk_local_pos(const Vector2i& p_pos, const Vector2i& p_size, uint8_t p_rotation) {
    const int max_coord = WorldCoords::CHUNK_SIZE - 1;
    switch (p_rotation) {
        case WorldCoords::ROT_WEST:
            return Vector2i(max_coord - p_pos.y - p_size.y + 1, p_pos.x);
        case WorldCoords::ROT_NORTH:
            return Vector2i(max_coord - p_pos.x - p_size.x + 1, max_coord - p_pos.y - p_size.y + 1);
        case WorldCoords::ROT_EAST:
            return Vector2i(p_pos.y, max_coord - p_pos.x - p_size.x + 1);
        case WorldCoords::ROT_SOUTH:
        default:
            return p_pos;
    }
}

static Vector2i rotate_layout_pos(
    const Vector2i& p_pos,
    const Vector2i& p_size,
    const Vector2i& p_layout_size,
    uint8_t p_rotation
) {
    switch (p_rotation) {
        case WorldCoords::ROT_WEST:
            return Vector2i(p_layout_size.y - p_pos.y - p_size.y, p_pos.x);
        case WorldCoords::ROT_NORTH:
            return Vector2i(
                p_layout_size.x - p_pos.x - p_size.x,
                p_layout_size.y - p_pos.y - p_size.y
            );
        case WorldCoords::ROT_EAST:
            return Vector2i(p_pos.y, p_layout_size.x - p_pos.x - p_size.x);
        case WorldCoords::ROT_SOUTH:
        default:
            return p_pos;
    }
}

static uint8_t get_center_facing_rotation(
    const Vector2i& p_area_origin,
    const Vector2i& p_area_size,
    const Vector2i& p_layout_size
) {
    const float layout_center_x = (static_cast<float>(p_layout_size.x) - 1.0f) * 0.5f;
    const float layout_center_y = (static_cast<float>(p_layout_size.y) - 1.0f) * 0.5f;
    const float area_center_x = static_cast<float>(p_area_origin.x) + (static_cast<float>(p_area_size.x) - 1.0f) * 0.5f;
    const float area_center_y = static_cast<float>(p_area_origin.y) + (static_cast<float>(p_area_size.y) - 1.0f) * 0.5f;
    const float dx = layout_center_x - area_center_x;
    const float dy = layout_center_y - area_center_y;

    if (std::abs(dx) > std::abs(dy)) {
        return dx >= 0.0f ? WorldCoords::ROT_EAST : WorldCoords::ROT_WEST;
    }
    return dy >= 0.0f ? WorldCoords::ROT_SOUTH : WorldCoords::ROT_NORTH;
}

static uint8_t parse_feature_facing(const String& p_facing, uint8_t p_default) {
    const String facing = p_facing.strip_edges().to_lower();
    if (facing == "south" || facing == "s") return WorldCoords::ROT_SOUTH;
    if (facing == "west" || facing == "w") return WorldCoords::ROT_WEST;
    if (facing == "north" || facing == "n") return WorldCoords::ROT_NORTH;
    if (facing == "east" || facing == "e") return WorldCoords::ROT_EAST;
    return p_default;
}

uint32_t WorldGenerator::get_default_biome_chunk_data(int p_z) const {
    uint16_t biome_id = id_void;
    if (p_z == -1) {
        biome_id = id_underground_earth;
    } else if (p_z < -1) {
        biome_id = id_solid_rock;
    }
    return biome_id;
}

BiomeLayer& WorldGenerator::get_or_create_biome_layer(int p_z) {
    auto it = biome_layers.find(p_z);
    if (it != biome_layers.end()) return it->second;

    BiomeLayer layer;
    layer.z = p_z;
    layer.default_chunk_data = get_default_biome_chunk_data(p_z);
    auto inserted = biome_layers.emplace(p_z, std::move(layer));
    return inserted.first->second;
}

const BiomeLayer* WorldGenerator::get_biome_layer(int p_z) const {
    auto it = biome_layers.find(p_z);
    return it != biome_layers.end() ? &it->second : nullptr;
}

std::unordered_map<uint64_t, uint32_t>& WorldGenerator::get_surface_biome_overrides() {
    return get_or_create_biome_layer(0).overrides;
}

uint16_t WorldGenerator::pick_city_spawn_chunk(
    const ChunkDb& p_chunk_db,
    int p_city_distance_sq,
    int p_max_city_distance_sq,
    const Vector2i& p_chunk_pos,
    int p_world_seed,
    const std::unordered_map<uint16_t, int>* p_city_chunk_counts
) const {
    std::vector<const CityChunkSpawnInfo*> eligible_spawns;
    int eligible_total_weight = 0;

    for (const CityChunkSpawnInfo& spawn_info : p_chunk_db.get_city_spawn_chunks()) {
        if (spawn_info.weight <= 0) continue;

        if (!city_spawn_zone_contains(
                p_city_distance_sq,
                p_max_city_distance_sq,
                spawn_info.city_zone_min,
                spawn_info.city_zone_max)) {
            continue;
        }

        if (p_city_chunk_counts && spawn_info.city_max_count >= 0) {
            auto count_it = p_city_chunk_counts->find(spawn_info.id);
            if (count_it != p_city_chunk_counts->end() && count_it->second >= spawn_info.city_max_count) {
                continue;
            }
        }

        eligible_spawns.push_back(&spawn_info);
        eligible_total_weight += spawn_info.weight;
    }

    if (eligible_total_weight <= 0) {
        return id_building;
    }

    const uint64_t h = Rng::hash_pos(static_cast<uint32_t>(p_world_seed), p_chunk_pos, Rng::BIOME);
    const int roll = static_cast<int>(h % static_cast<uint64_t>(eligible_total_weight));
    int cumulative = 0;
    for (const CityChunkSpawnInfo* spawn_info : eligible_spawns) {
        cumulative += spawn_info->weight;
        if (roll < cumulative) {
            return spawn_info->id;
        }
    }

    return id_building;
}

void WorldGenerator::set_biome_chunk_data(int p_chunk_x, int p_chunk_y, int p_z, uint32_t p_data) {
    get_or_create_biome_layer(p_z).overrides[WorldCoords::pack_coords(p_chunk_x, p_chunk_y)] = p_data;
}

void WorldGenerator::stamp_dungeon_layout_biomes(const DungeonLayout& p_layout) {
    if (id_dungeon == 0) return;
    const uint32_t dungeon_data = id_dungeon;

    auto stamp_chunk = [&](int p_chunk_x, int p_chunk_y) {
        set_biome_chunk_data(p_chunk_x, p_chunk_y, p_layout.z, dungeon_data);
    };

    auto stamp_cell = [&](int p_x, int p_y) {
        stamp_chunk(floor_div_chunk(p_x), floor_div_chunk(p_y));
    };

    for (const PlacedDungeonRoom& room : p_layout.rooms) {
        const int min_cx = floor_div_chunk(room.bounds.origin.x);
        const int min_cy = floor_div_chunk(room.bounds.origin.y);
        const int max_cx = floor_div_chunk(room.bounds.origin.x + room.bounds.size.x - 1);
        const int max_cy = floor_div_chunk(room.bounds.origin.y + room.bounds.size.y - 1);
        for (int cy = min_cy; cy <= max_cy; cy++) {
            for (int cx = min_cx; cx <= max_cx; cx++) {
                stamp_chunk(cx, cy);
            }
        }
    }

    auto stamp_cell_set = [&](const std::unordered_set<uint64_t>& p_cells) {
        for (uint64_t key : p_cells) {
            Vector2i cell = WorldCoords::unpack_coords(key);
            stamp_cell(cell.x, cell.y);
        }
    };

    stamp_cell_set(p_layout.corridor_cells);
    stamp_cell_set(p_layout.corridor_wall_cells);
    stamp_cell_set(p_layout.door_cells);
}

void WorldGenerator::reset_dungeon_cache() {
    dungeon_layout_cache.clear();
    dungeon_layout_cache_seed_valid = false;
    dungeon_entrance_cache.clear();
    dungeon_entrance_cache_valid = false;
}

void WorldGenerator::rebuild_dungeon_entrance_cache() {
    dungeon_entrance_cache.clear();
    dungeon_entrance_cache_valid = true;

    ChunkDb* chunk_db = ChunkDb::get_singleton();
    DungeonDb* dungeon_db = DungeonDb::get_singleton();
    if (!chunk_db || !dungeon_db) return;

    const auto& surface_chunks = get_region_chunks();
    for (const auto& pair : surface_chunks) {
        const uint16_t chunk_id = static_cast<uint16_t>(pair.second & WorldCoords::ID_MASK);
        const ChunkInfo* chunk_info = chunk_db->get_chunk_info(chunk_id);
        if (!chunk_info || chunk_info->dungeon_type.is_empty()) continue;

        const DungeonInfo* dungeon_info = dungeon_db->get_dungeon_info(chunk_info->dungeon_type);
        if (!dungeon_info) continue;

        DungeonEntranceRef ref;
        ref.dungeon_type = chunk_info->dungeon_type;
        ref.entrance_chunk = WorldCoords::unpack_coords(pair.first);
        ref.start_z = dungeon_info->start_z;
        dungeon_entrance_cache.push_back(ref);
    }
}

void WorldGenerator::setup_biome_rules() {
    if (!biome_rules.empty()) return;

    id_reg = IdRegistry::get_singleton();
    s_db = StructureDb::get_singleton();
    if (!id_reg) return;

    id_void = id_reg->register_string("void");
    id_air = id_reg->register_string("air");
    id_building = id_reg->register_string("building");
    id_road = id_reg->register_string("road");
    id_alley = id_reg->register_string("alley");
    id_forest = id_reg->register_string("forest");
    id_plains = id_reg->register_string("plains");
    id_underground_earth = id_reg->register_string("underground_earth");
    id_solid_rock = id_reg->register_string("solid_rock");
    id_road_bricks = id_reg->register_string("road_bricks");
    id_road_flagstone = id_reg->register_string("road_flagstone");
    id_alley_bricks = id_reg->register_string("alley_bricks");
    id_alley_flagstone = id_reg->register_string("alley_flagstone");
    id_dirt = id_reg->register_string("dirt");
    id_crypt_entrance = id_reg->register_string("crypt_entrance");
    id_dungeon = id_reg->register_string("dungeon");
    id_stone_brick_floor = id_reg->register_string("stone_brick_floor");
    id_stone_brick_wall = id_reg->register_string("stone_brick_wall");
    id_dungeon_door = id_reg->register_string("w_door_c");
    id_spider_eggs = id_reg->register_string("spider_eggs");
    id_stone_brick_wall_web = id_reg->register_string("stone_brick_wall_web");
    id_stone_brick_wall_web_thick = id_reg->register_string("stone_brick_wall_web_thick");
    id_stone_brick_floor_web = id_reg->register_string("stone_brick_floor_web");
    id_stone_brick_floor_web_thick = id_reg->register_string("stone_brick_floor_web_thick");

    TagRegistry* tag_reg = TagRegistry::get_singleton();
    tag_road = tag_reg ? tag_reg->get_tag_id("ROAD") : 0;

    auto reg_tile_group = [&](const String& name, const String& tile_group_id) {
        uint16_t b_id = id_reg->register_string(name);
        TileGroupDb* tile_group_db = TileGroupDb::get_singleton();
        const TileGroupInfo* tile_group = tile_group_db ? tile_group_db->get_tile_group(tile_group_id) : nullptr;
        if (!tile_group || tile_group->entries.empty() || tile_group->total_weight <= 0) {
            UtilityFunctions::push_error("[WorldGenerator] Missing tile group for biome ", name, ": ", tile_group_id);
            return;
        }

        BiomeInfo info;
        for (const TileGroupEntryInfo& entry : tile_group->entries) {
            if (entry.tile_id == 0 || entry.weight <= 0) continue;
            info.ground_tiles.push_back({entry.tile_id, entry.weight});
            info.total_weight += entry.weight;
        }
        if (info.ground_tiles.empty() || info.total_weight <= 0) return;
        if (name == String("alley")) {
            alley_gap_tiles = info;
        }
        biome_rules[b_id] = info;
    };

    ChunkDb* chunk_db = ChunkDb::get_singleton();
    if (chunk_db) {
        Array chunk_ids = chunk_db->get_ids();
        for (int i = 0; i < chunk_ids.size(); i++) {
            String chunk_id = String(chunk_ids[i]);
            const ChunkInfo* chunk_info = chunk_db->get_chunk_info(chunk_id);
            if (!chunk_info || chunk_info->tile_group.is_empty()) continue;
            reg_tile_group(chunk_id, chunk_info->tile_group);
        }
    }

    auto reg_tiled = [&](const String& name, const String& tile, const String& border) {
        uint16_t b_id = id_reg->register_string(name);
        BiomeInfo info;
        info.ground_tiles.push_back({id_reg->register_string(tile), 100});
        info.total_weight = 100;
        info.auto_tiled = true;
        info.border_tile_id = id_reg->register_string(border);
        biome_rules[b_id] = info;
    };

    reg_tiled("road", "road_bricks", "road_flagstone");
    reg_tiled("alley", "alley_bricks", "alley_flagstone");
}

Dictionary WorldGenerator::init_region(const Vector2i& regionPos, int world_seed, const Ref<FastNoiseLite>& biome_noise) {
    setup_biome_rules();
    
    biome_layers.clear();
    auto& surface_chunks = get_surface_biome_overrides();
    last_chunk_valid = false;
    reset_dungeon_cache();

    const Vector2i city_center(127, 128);
    GenGrid cityGenGrid(WorldCoords::REGION_SIZE);
    CityGeneration::spawn_city(cityGenGrid, city_center.x, city_center.y, world_seed);

    std::vector<int> city_distance_sq_by_cell(WorldCoords::REGION_SIZE * WorldCoords::REGION_SIZE, 0);
    int max_city_distance_sq = 0;

    for (int y = 0; y < WorldCoords::REGION_SIZE; y++) {
        for (int x = 0; x < WorldCoords::REGION_SIZE; x++) {
            if (cityGenGrid.getPixel(x, y).id != id_building) continue;

            const int dx = x - city_center.x;
            const int dy = y - city_center.y;
            const int distance_sq = dx * dx + dy * dy;
            city_distance_sq_by_cell[y * WorldCoords::REGION_SIZE + x] = distance_sq;
            if (distance_sq > max_city_distance_sq) {
                max_city_distance_sq = distance_sq;
            }
        }
    }

    struct CityLot {
        Vector2i global_pos;
        int distance_sq = 0;
        uint64_t key = 0;
    };

    ChunkDb* city_chunk_db = ChunkDb::get_singleton();
    std::vector<CityLot> city_lots;
    std::unordered_map<uint64_t, uint16_t> assigned_city_chunks;
    std::unordered_map<uint16_t, int> city_chunk_counts;
    std::vector<Vector2i> selected_city_lots;

    if (city_chunk_db) {
        for (int y = 0; y < WorldCoords::REGION_SIZE; y++) {
            for (int x = 0; x < WorldCoords::REGION_SIZE; x++) {
                if (cityGenGrid.getPixel(x, y).id != id_building) continue;
                const int gx = regionPos.x * WorldCoords::REGION_SIZE + x;
                const int gy = regionPos.y * WorldCoords::REGION_SIZE + y;
                city_lots.push_back({
                    Vector2i(gx, gy),
                    city_distance_sq_by_cell[y * WorldCoords::REGION_SIZE + x],
                    WorldCoords::pack_coords(gx, gy)
                });
            }
        }

        auto choose_spread_lot = [&](const CityChunkSpawnInfo& rule) -> const CityLot* {
            const CityLot* best_lot = nullptr;
            int best_distance = -1;
            uint64_t best_hash = 0;

            for (const CityLot& lot : city_lots) {
                if (assigned_city_chunks.find(lot.key) != assigned_city_chunks.end()) continue;
                if (!city_spawn_zone_contains(
                        lot.distance_sq,
                        max_city_distance_sq,
                        rule.city_zone_min,
                        rule.city_zone_max)) {
                    continue;
                }

                int distance = selected_city_lots.empty() ? 0 : 0x7FFFFFFF;
                for (const Vector2i& selected : selected_city_lots) {
                    const int dx = lot.global_pos.x - selected.x;
                    const int dy = lot.global_pos.y - selected.y;
                    distance = std::min(distance, dx * dx + dy * dy);
                }

                const uint64_t hash = Rng::hash_pos(
                    static_cast<uint32_t>(world_seed),
                    lot.global_pos,
                    Rng::BIOME
                ) ^ static_cast<uint64_t>(rule.id);
                if (!best_lot || distance > best_distance ||
                    (distance == best_distance && hash > best_hash)) {
                    best_lot = &lot;
                    best_distance = distance;
                    best_hash = hash;
                }
            }
            return best_lot;
        };

        for (const CityChunkSpawnInfo& rule : city_chunk_db->get_city_spawn_chunks()) {
            if (rule.city_min_count <= 0) continue;
            int& count = city_chunk_counts[rule.id];
            while (count < rule.city_min_count) {
                const CityLot* lot = choose_spread_lot(rule);
                if (!lot) break;
                assigned_city_chunks[lot->key] = rule.id;
                selected_city_lots.push_back(lot->global_pos);
                count++;
            }
        }

        std::sort(city_lots.begin(), city_lots.end(), [&](const CityLot& a, const CityLot& b) {
            const uint64_t hash_a = Rng::hash_pos(static_cast<uint32_t>(world_seed), a.global_pos, Rng::BIOME);
            const uint64_t hash_b = Rng::hash_pos(static_cast<uint32_t>(world_seed), b.global_pos, Rng::BIOME);
            return hash_a != hash_b ? hash_a < hash_b : a.key < b.key;
        });

        for (const CityLot& lot : city_lots) {
            if (assigned_city_chunks.find(lot.key) != assigned_city_chunks.end()) continue;
            const uint16_t chunk_id = pick_city_spawn_chunk(
                *city_chunk_db,
                lot.distance_sq,
                max_city_distance_sq,
                lot.global_pos,
                world_seed,
                &city_chunk_counts
            );
            assigned_city_chunks[lot.key] = chunk_id;
            city_chunk_counts[chunk_id]++;
        }
    }

    city_structure_instances.clear();
    city_structure_by_chunk.clear();
    struct CityPlacementCandidate {
        Vector2i anchor_chunk;
        Vector2i origin_chunk;
        uint16_t chunk_id = 0;
        String structure_id;
        Vector2i source_size;
        Vector2i placed_size;
        uint8_t rotation = WorldCoords::ROT_SOUTH;
        int footprint_area = 0;
        bool requires_palace_lot = false;
    };
    std::vector<CityPlacementCandidate> placement_candidates;
    const uint16_t city_wall_id = id_reg->get_id("wall");
    const uint16_t city_gate_id = id_reg->get_id("gate");
    const uint16_t city_water_id = id_reg->get_id("water");
    const uint16_t city_palace_id = id_reg->get_id("palace");
    const uint16_t city_plaza_id = id_reg->get_id("plaza");
    if (city_chunk_db && s_db) {
        for (int y = 0; y < WorldCoords::REGION_SIZE; y++) {
            for (int x = 0; x < WorldCoords::REGION_SIZE; x++) {
                if (cityGenGrid.getPixel(x, y).id != id_building) continue;

                const int gx = regionPos.x * WorldCoords::REGION_SIZE + x;
                const int gy = regionPos.y * WorldCoords::REGION_SIZE + y;
                const int city_distance_sq = city_distance_sq_by_cell[y * WorldCoords::REGION_SIZE + x];
                const uint64_t city_lot_key = WorldCoords::pack_coords(gx, gy);
                auto assignment_it = assigned_city_chunks.find(city_lot_key);
                const uint16_t chunk_id = assignment_it != assigned_city_chunks.end()
                    ? assignment_it->second
                    : pick_city_spawn_chunk(*city_chunk_db, city_distance_sq, max_city_distance_sq, Vector2i(gx, gy), world_seed);
                const ChunkInfo* chunk_info = city_chunk_db->get_chunk_info(chunk_id);
                if (!chunk_info || chunk_info->structure_type.is_empty()) continue;

                const std::vector<String>* structures = s_db->get_structure_ids_by_type(chunk_info->structure_type);
                if (!structures || structures->empty()) continue;
                const uint64_t structure_hash = Rng::hash_pos(static_cast<uint32_t>(world_seed), Vector2i(gx, gy), Rng::BIOME);
                const String structure_id = (*structures)[structure_hash % structures->size()];
                const StructureInfo* structure = s_db->get_structure_info(structure_id);
                if (!structure || structure->size.x <= 0 || structure->size.y <= 0) continue;

                const uint8_t preferred_rotation = static_cast<uint8_t>(cityGenGrid.getPixel(x, y).meta & WorldCoords::ROTATION_MASK);
                uint8_t rotation = preferred_rotation;
                if (std::find(structure->placement_rotations.begin(), structure->placement_rotations.end(), rotation) == structure->placement_rotations.end()) {
                    rotation = structure->placement_rotations[structure_hash % structure->placement_rotations.size()];
                }
                const Vector2i placed_size = get_rotated_surface_feature_size(structure->size, rotation);
                const int footprint_width = (placed_size.x + WorldCoords::CHUNK_SIZE - 1) / WorldCoords::CHUNK_SIZE;
                const int footprint_height = (placed_size.y + WorldCoords::CHUNK_SIZE - 1) / WorldCoords::CHUNK_SIZE;
                Vector2i origin_chunk(x, y);
                switch (rotation) {
                    case WorldCoords::ROT_SOUTH:
                        origin_chunk.y -= footprint_height - 1;
                        break;
                    case WorldCoords::ROT_EAST:
                        origin_chunk.x -= footprint_width - 1;
                        break;
                    case WorldCoords::ROT_NORTH:
                    case WorldCoords::ROT_WEST:
                    default:
                        break;
                }
                placement_candidates.push_back({
                    Vector2i(x, y), origin_chunk, chunk_id, structure_id, structure->size, placed_size, rotation,
                    footprint_width * footprint_height, false
                });
            }
        }

        const std::vector<String>* palace_structures = s_db->get_structure_ids_by_type("palace");
        if (palace_structures && !palace_structures->empty()) {
            for (int y = 0; y < WorldCoords::REGION_SIZE; y++) {
                for (int x = 0; x < WorldCoords::REGION_SIZE; x++) {
                    if (cityGenGrid.getPixel(x, y).id != city_palace_id) continue;
                    if ((x > 0 && cityGenGrid.getPixel(x - 1, y).id == city_palace_id) ||
                        (y > 0 && cityGenGrid.getPixel(x, y - 1).id == city_palace_id)) {
                        continue;
                    }

                    int palace_width = 0;
                    while (x + palace_width < WorldCoords::REGION_SIZE &&
                           cityGenGrid.getPixel(x + palace_width, y).id == city_palace_id) {
                        palace_width++;
                    }
                    int palace_height = 0;
                    while (y + palace_height < WorldCoords::REGION_SIZE &&
                           cityGenGrid.getPixel(x, y + palace_height).id == city_palace_id) {
                        palace_height++;
                    }

                    const Vector2i palace_center(x + palace_width / 2, y + palace_height / 2);
                    const Vector2i global_center(
                        regionPos.x * WorldCoords::REGION_SIZE + palace_center.x,
                        regionPos.y * WorldCoords::REGION_SIZE + palace_center.y
                    );
                    const uint64_t palace_hash = Rng::hash_pos(static_cast<uint32_t>(world_seed), global_center, Rng::BIOME);
                    const String structure_id = (*palace_structures)[palace_hash % palace_structures->size()];
                    const StructureInfo* structure = s_db->get_structure_info(structure_id);
                    if (!structure || structure->size.x <= 0 || structure->size.y <= 0) continue;

                    const uint8_t rotation = WorldCoords::ROT_SOUTH;
                    const Vector2i placed_size = get_rotated_surface_feature_size(structure->size, rotation);
                    const int footprint_width = (placed_size.x + WorldCoords::CHUNK_SIZE - 1) / WorldCoords::CHUNK_SIZE;
                    const int footprint_height = (placed_size.y + WorldCoords::CHUNK_SIZE - 1) / WorldCoords::CHUNK_SIZE;
                    if (footprint_width > palace_width || footprint_height > palace_height) continue;

                    const Vector2i origin_chunk(
                        x + (palace_width - footprint_width) / 2,
                        y + (palace_height - footprint_height) / 2
                    );
                    placement_candidates.push_back({
                        palace_center, origin_chunk, city_palace_id, structure_id, structure->size, placed_size, rotation,
                        footprint_width * footprint_height, true
                    });
                }
            }
        }

        // Multi-chunk wilderness structures need to be planned as one instance
        // before the remaining wilderness cells are filled with forest/plains.
        // This keeps their footprint contiguous and lets the normal structure
        // context/rendering and save data handle them just like palace layouts.
        for (const CityChunkSpawnInfo& rule : city_chunk_db->get_wilderness_spawn_chunks()) {
            const Vector2i requested_footprint = rule.wilderness_footprint;
            if (requested_footprint.x <= 1 && requested_footprint.y <= 1) continue;

            const ChunkInfo* chunk_info = city_chunk_db->get_chunk_info(rule.id);
            if (!chunk_info || chunk_info->structure_type.is_empty() ||
                chunk_info->wilderness_spawn_chance <= 0.0f) {
                continue;
            }

            const std::vector<String>* structures = s_db->get_structure_ids_by_type(chunk_info->structure_type);
            if (!structures || structures->empty()) continue;

            for (int y = 0; y + requested_footprint.y <= WorldCoords::REGION_SIZE; y += requested_footprint.y) {
                for (int x = 0; x + requested_footprint.x <= WorldCoords::REGION_SIZE; x += requested_footprint.x) {
                    const Vector2i global_anchor(
                        regionPos.x * WorldCoords::REGION_SIZE + x,
                        regionPos.y * WorldCoords::REGION_SIZE + y
                    );
                    Rng::Seeded spawn_rng = Rng::at(
                        static_cast<uint32_t>(world_seed),
                        global_anchor,
                        Rng::BIOME,
                        0x42414E4449544341ULL + static_cast<uint64_t>(rule.id) // "BANDITCA"
                    );
                    if (!spawn_rng.chance(chunk_info->wilderness_spawn_chance)) continue;

                    const uint64_t structure_hash = Rng::hash_pos(
                        static_cast<uint32_t>(world_seed),
                        global_anchor,
                        Rng::BIOME
                    ) ^ static_cast<uint64_t>(rule.id);
                    const String structure_id = (*structures)[structure_hash % structures->size()];
                    const StructureInfo* structure = s_db->get_structure_info(structure_id);
                    if (!structure || structure->size.x <= 0 || structure->size.y <= 0 ||
                        structure->placement_rotations.empty()) {
                        continue;
                    }

                    const uint8_t rotation = structure->placement_rotations[
                        structure_hash % structure->placement_rotations.size()
                    ];
                    const Vector2i placed_size = get_rotated_surface_feature_size(structure->size, rotation);
                    const int footprint_width = (placed_size.x + WorldCoords::CHUNK_SIZE - 1) / WorldCoords::CHUNK_SIZE;
                    const int footprint_height = (placed_size.y + WorldCoords::CHUNK_SIZE - 1) / WorldCoords::CHUNK_SIZE;
                    if (footprint_width != requested_footprint.x || footprint_height != requested_footprint.y) {
                        continue;
                    }

                    bool open_wilderness = true;
                    for (int dy = 0; dy < footprint_height && open_wilderness; dy++) {
                        for (int dx = 0; dx < footprint_width; dx++) {
                            if (cityGenGrid.getPixel(x + dx, y + dy).id != id_void) {
                                open_wilderness = false;
                                break;
                            }
                        }
                    }
                    if (!open_wilderness) continue;

                    placement_candidates.push_back({
                        Vector2i(x, y),
                        Vector2i(x, y),
                        rule.id,
                        structure_id,
                        structure->size,
                        placed_size,
                        rotation,
                        footprint_width * footprint_height,
                        false
                    });
                }
            }
        }
    }

    std::sort(placement_candidates.begin(), placement_candidates.end(), [](const CityPlacementCandidate& a, const CityPlacementCandidate& b) {
        if (a.footprint_area != b.footprint_area) return a.footprint_area > b.footprint_area;
        if (a.anchor_chunk.y != b.anchor_chunk.y) return a.anchor_chunk.y < b.anchor_chunk.y;
        return a.anchor_chunk.x < b.anchor_chunk.x;
    });

    std::unordered_map<uint64_t, uint32_t> planned_city_chunks;
    for (const CityPlacementCandidate& candidate : placement_candidates) {
        const int footprint_width = (candidate.placed_size.x + WorldCoords::CHUNK_SIZE - 1) / WorldCoords::CHUNK_SIZE;
        const int footprint_height = (candidate.placed_size.y + WorldCoords::CHUNK_SIZE - 1) / WorldCoords::CHUNK_SIZE;
        bool can_place = true;
        for (int dy = 0; dy < footprint_height && can_place; dy++) {
            for (int dx = 0; dx < footprint_width; dx++) {
                const int x = candidate.origin_chunk.x + dx;
                const int y = candidate.origin_chunk.y + dy;
                if (x < 0 || x >= WorldCoords::REGION_SIZE || y < 0 || y >= WorldCoords::REGION_SIZE ||
                    planned_city_chunks.find(WorldCoords::pack_coords(
                        regionPos.x * WorldCoords::REGION_SIZE + x,
                        regionPos.y * WorldCoords::REGION_SIZE + y
                    )) != planned_city_chunks.end()) {
                    can_place = false;
                    break;
                }

                const uint16_t lot_id = cityGenGrid.getPixel(x, y).id;
                const bool wrong_lot = candidate.requires_palace_lot
                    ? lot_id != city_palace_id
                    : lot_id == id_road || lot_id == id_alley || lot_id == city_wall_id ||
                        lot_id == city_gate_id || lot_id == city_water_id || lot_id == city_palace_id || lot_id == city_plaza_id ||
                        lot_id == id_forest || lot_id == id_plains;
                if (wrong_lot) {
                    can_place = false;
                    break;
                }
            }
        }
        if (!can_place) continue;

        CityStructureInstance instance;
        instance.structure_id = candidate.structure_id;
        instance.origin = Vector2i(
            (regionPos.x * WorldCoords::REGION_SIZE + candidate.origin_chunk.x) * WorldCoords::CHUNK_SIZE,
            (regionPos.y * WorldCoords::REGION_SIZE + candidate.origin_chunk.y) * WorldCoords::CHUNK_SIZE
        );
        instance.source_size = candidate.source_size;
        instance.placed_size = candidate.placed_size;
        instance.rotation = candidate.rotation;
        const size_t instance_index = city_structure_instances.size();
        city_structure_instances.push_back(instance);

        const uint32_t packed_chunk = (static_cast<uint32_t>(candidate.rotation) << WorldCoords::ORIENTATION_SHIFT) | candidate.chunk_id;
        for (int dy = 0; dy < footprint_height; dy++) {
            for (int dx = 0; dx < footprint_width; dx++) {
                const int gx = regionPos.x * WorldCoords::REGION_SIZE + candidate.origin_chunk.x + dx;
                const int gy = regionPos.y * WorldCoords::REGION_SIZE + candidate.origin_chunk.y + dy;
                const uint64_t key = WorldCoords::pack_coords(gx, gy);
                planned_city_chunks[key] = packed_chunk;
                city_structure_by_chunk[key] = instance_index;
            }
        }
    }

    Dictionary result;
    for (int y = 0; y < WorldCoords::REGION_SIZE; y++) {
        for (int x = 0; x < WorldCoords::REGION_SIZE; x++) {
            CityPixel pixel = cityGenGrid.getPixel(x, y);
            uint16_t chunk_id = pixel.id;
            const int gx = regionPos.x * WorldCoords::REGION_SIZE + x;
            const int gy = regionPos.y * WorldCoords::REGION_SIZE + y;
            const uint64_t planned_key = WorldCoords::pack_coords(gx, gy);
            auto planned_it = planned_city_chunks.find(planned_key);
            
            if (planned_it != planned_city_chunks.end()) {
                chunk_id = static_cast<uint16_t>(planned_it->second & WorldCoords::ID_MASK);
                pixel.meta = static_cast<uint8_t>(planned_it->second >> WorldCoords::ORIENTATION_SHIFT);
            } else if (chunk_id == id_void) {
                uint32_t h = get_hash(gx, gy, static_cast<uint32_t>(world_seed));

                ChunkDb* chunk_db = ChunkDb::get_singleton();
                if (chunk_db) {
                    for (const CityChunkSpawnInfo& spawn_info : chunk_db->get_wilderness_spawn_chunks()) {
                        if (spawn_info.wilderness_footprint.x > 1 || spawn_info.wilderness_footprint.y > 1) continue;
                        const ChunkInfo* spawn_chunk = chunk_db->get_chunk_info(spawn_info.id);
                        if (!spawn_chunk || spawn_chunk->wilderness_spawn_chance <= 0.0f) continue;

                        Rng::Seeded spawn_rng = Rng::at(
                            static_cast<uint32_t>(world_seed),
                            Vector2i(gx, gy),
                            Rng::BIOME,
                            0x44554E47454F4E53ULL + static_cast<uint64_t>(spawn_info.id) // "DUNGEONS"
                        );
                        if (spawn_rng.chance(spawn_chunk->wilderness_spawn_chance)) {
                            chunk_id = spawn_info.id;
                            break;
                        }
                    }
                }

                if (chunk_id == id_void) {
                    chunk_id = (h % 100 < 50) ? id_forest : id_plains;
                }
            } else if (chunk_id == id_building) {
                ChunkDb* chunk_db = ChunkDb::get_singleton();
                if (chunk_db) {
                    const int city_distance_sq = city_distance_sq_by_cell[y * WorldCoords::REGION_SIZE + x];
                    auto assignment_it = assigned_city_chunks.find(planned_key);
                    if (assignment_it != assigned_city_chunks.end()) {
                        chunk_id = assignment_it->second;
                    } else {
                        chunk_id = pick_city_spawn_chunk(*chunk_db, city_distance_sq, max_city_distance_sq, Vector2i(gx, gy), world_seed);
                    }
                }
            }

            uint8_t rot = pixel.meta & WorldCoords::ROTATION_MASK;

            uint64_t key = planned_key;

            const uint32_t packed_chunk = (static_cast<uint32_t>(rot) << WorldCoords::ORIENTATION_SHIFT) | chunk_id;
            surface_chunks[key] = packed_chunk;
            if (chunk_id == id_crypt_entrance) {
                set_biome_chunk_data(gx, gy, -1, packed_chunk);
                set_biome_chunk_data(gx, gy, -2, packed_chunk);
            }
            result[key] = id_reg->get_string(chunk_id);
        }
    }

    apply_auto_tiling(regionPos);
    return result;
}

void WorldGenerator::apply_auto_tiling(const Vector2i& p_region_pos) {
    ChunkDb* chunk_db = ChunkDb::get_singleton();
    TagRegistry* tag_reg = TagRegistry::get_singleton();
    uint16_t road_tag_id = tag_reg ? tag_reg->get_tag_id("ROAD") : 0;

    std::vector<uint16_t> grid(WorldCoords::REGION_SIZE * WorldCoords::REGION_SIZE, 0);
    std::vector<uint64_t> chunk_keys;
    auto& surface_chunks = get_surface_biome_overrides();
    chunk_keys.reserve(surface_chunks.size());

    for (auto& pair : surface_chunks) {
        chunk_keys.push_back(pair.first);
        Vector2i pos = WorldCoords::unpack_coords(pair.first);
        int rel_x = pos.x - p_region_pos.x * WorldCoords::REGION_SIZE;
        int rel_y = pos.y - p_region_pos.y * WorldCoords::REGION_SIZE;
        if (rel_x >= 0 && rel_x < WorldCoords::REGION_SIZE && rel_y >= 0 && rel_y < WorldCoords::REGION_SIZE) {
            grid[rel_y * WorldCoords::REGION_SIZE + rel_x] = static_cast<uint16_t>(pair.second & WorldCoords::ID_MASK);
        }
    }

    for (uint64_t key : chunk_keys) {
        uint32_t packed = surface_chunks[key];
        uint16_t chunk_id = static_cast<uint16_t>(packed & WorldCoords::ID_MASK);

        auto it_rule = biome_rules.find(chunk_id);
        if (it_rule == biome_rules.end() || !it_rule->second.auto_tiled) continue;

        Vector2i pos = WorldCoords::unpack_coords(key);
        int rel_x = pos.x - p_region_pos.x * WorldCoords::REGION_SIZE;
        int rel_y = pos.y - p_region_pos.y * WorldCoords::REGION_SIZE;

        uint32_t mask = 0;
        bool current_is_alley = (chunk_id == id_alley);

        auto get_grid_id = [&](int nx, int ny) -> uint16_t {
            if (nx < 0 || nx >= WorldCoords::REGION_SIZE || ny < 0 || ny >= WorldCoords::REGION_SIZE) {
                int gx = p_region_pos.x * WorldCoords::REGION_SIZE + nx;
                int gy = p_region_pos.y * WorldCoords::REGION_SIZE + ny;
                uint64_t n_key = WorldCoords::pack_coords(gx, gy);
                auto n_it = surface_chunks.find(n_key);
                return (n_it != surface_chunks.end()) ? static_cast<uint16_t>(n_it->second & WorldCoords::ID_MASK) : 0;
            }
            return grid[ny * WorldCoords::REGION_SIZE + nx];
        };

        auto check_neighbor = [&](int dx, int dy, WorldCoords::NeighborBits bit) {
            uint16_t n_id = get_grid_id(rel_x + dx, rel_y + dy);
            if (n_id != 0) {
                bool neighbor_connects = false;
                if (current_is_alley && chunk_db && road_tag_id != 0) {
                    if (chunk_db->has_tag(n_id, road_tag_id)) neighbor_connects = true;
                }
                if (!neighbor_connects && n_id == chunk_id) neighbor_connects = true;
                if (neighbor_connects) mask |= bit;
            }
        };

        check_neighbor(0, -1, WorldCoords::NEIGH_NORTH);
        check_neighbor(1, 0, WorldCoords::NEIGH_EAST);
        check_neighbor(0, 1, WorldCoords::NEIGH_SOUTH);
        check_neighbor(-1, 0, WorldCoords::NEIGH_WEST);

        surface_chunks[key] = (packed & ~(WorldCoords::NEIGHBOR_MASK << WorldCoords::NEIGHBOR_SHIFT)) | (mask << WorldCoords::NEIGHBOR_SHIFT);
    }
}

uint16_t WorldGenerator::pick_weighted_tile(const BiomeInfo& info, uint32_t hash) {
    if (info.ground_tiles.empty()) return id_void;
    if (info.ground_tiles.size() == 1) return info.ground_tiles[0].id;
    if (info.total_weight <= 0) return info.ground_tiles[0].id;

    uint32_t roll = hash % static_cast<uint32_t>(info.total_weight);
    int cumulative = 0;
    for (const auto& tile : info.ground_tiles) {
        if (tile.weight <= 0) continue;
        cumulative += tile.weight;
        if (roll < (uint32_t)cumulative) return tile.id;
    }
    return info.ground_tiles[0].id;
}

uint16_t WorldGenerator::get_alley_surface_tile(int p_local_x, int p_local_y, int p_world_x, int p_world_y, int p_world_seed) {
    static constexpr int EDGE_PAVEMENT_WIDTH = 2;
    static constexpr int EDGE_GARDEN_WIDTH = 3;
    static constexpr int INNER_PAVEMENT_WIDTH = 2;
    static constexpr int EDGE_BAND_WIDTH = EDGE_PAVEMENT_WIDTH + EDGE_GARDEN_WIDTH + INNER_PAVEMENT_WIDTH;

    auto garden_tile = [&]() -> uint16_t {
        if (alley_gap_tiles.ground_tiles.empty() || alley_gap_tiles.total_weight <= 0) {
            return id_alley_bricks;
        }
        uint32_t h = get_hash(p_world_x, p_world_y, static_cast<uint32_t>(p_world_seed));
        return pick_weighted_tile(alley_gap_tiles, h);
    };

    const bool north_connected = (last_chunk_neighbors & WorldCoords::NEIGH_NORTH) != 0;
    const bool east_connected = (last_chunk_neighbors & WorldCoords::NEIGH_EAST) != 0;
    const bool south_connected = (last_chunk_neighbors & WorldCoords::NEIGH_SOUTH) != 0;
    const bool west_connected = (last_chunk_neighbors & WorldCoords::NEIGH_WEST) != 0;
    const bool straight_vertical = north_connected && south_connected && !east_connected && !west_connected;
    const bool straight_horizontal = east_connected && west_connected && !north_connected && !south_connected;
    const bool simple_straight = straight_vertical || straight_horizontal;

    auto in_pavement_strip = [&](bool p_connected, int p_edge_dist) -> bool {
        if (p_connected || p_edge_dist >= EDGE_BAND_WIDTH) return false;
        return p_edge_dist < EDGE_PAVEMENT_WIDTH ||
            p_edge_dist >= EDGE_PAVEMENT_WIDTH + EDGE_GARDEN_WIDTH;
    };

    auto in_garden_strip = [&](bool p_connected, int p_edge_dist) -> bool {
        if (p_connected || p_edge_dist >= EDGE_BAND_WIDTH) return false;
        return p_edge_dist >= EDGE_PAVEMENT_WIDTH &&
            p_edge_dist < EDGE_PAVEMENT_WIDTH + EDGE_GARDEN_WIDTH;
    };

    const int west_dist = p_local_x;
    const int east_dist = WorldCoords::CHUNK_SIZE - 1 - p_local_x;
    const int north_dist = p_local_y;
    const int south_dist = WorldCoords::CHUNK_SIZE - 1 - p_local_y;

    ChunkDb* chunk_db = ChunkDb::get_singleton();
    const BiomeLayer* surface_layer = get_biome_layer(0);
    const int chunk_x = floor_div_chunk(p_world_x);
    const int chunk_y = floor_div_chunk(p_world_y);

    auto get_surface_chunk_id = [&](int p_chunk_x, int p_chunk_y) -> uint16_t {
        if (!surface_layer) return 0;

        const uint64_t key = WorldCoords::pack_coords(p_chunk_x, p_chunk_y);
        auto it = surface_layer->overrides.find(key);
        if (it == surface_layer->overrides.end()) return 0;

        return static_cast<uint16_t>(it->second & WorldCoords::ID_MASK);
    };

    auto is_road_network_chunk = [&](uint16_t p_chunk_id) -> bool {
        if (!chunk_db || tag_road == 0 || p_chunk_id == 0) return false;
        return chunk_db->has_tag(p_chunk_id, tag_road);
    };

    auto has_complete_road_network_corner = [&](int p_offset_x, int p_offset_y) -> bool {
        if (!chunk_db || !surface_layer || tag_road == 0) return false;

        const uint16_t current_id = get_surface_chunk_id(chunk_x, chunk_y);
        const uint16_t side_x_id = get_surface_chunk_id(chunk_x + p_offset_x, chunk_y);
        const uint16_t side_y_id = get_surface_chunk_id(chunk_x, chunk_y + p_offset_y);
        const uint16_t diagonal_id = get_surface_chunk_id(chunk_x + p_offset_x, chunk_y + p_offset_y);

        const bool has_road = current_id == id_road ||
            side_x_id == id_road ||
            side_y_id == id_road ||
            diagonal_id == id_road;
        return has_road &&
            is_road_network_chunk(current_id) &&
            is_road_network_chunk(side_x_id) &&
            is_road_network_chunk(side_y_id) &&
            is_road_network_chunk(diagonal_id);
    };

    auto compact_corner_tile = [&]() -> uint16_t {
        return id_alley_bricks;
    };

    auto corner_tile = [&](int p_x_edge_dist, int p_y_edge_dist) -> uint16_t {
        if (p_x_edge_dist >= EDGE_BAND_WIDTH || p_y_edge_dist >= EDGE_BAND_WIDTH) {
            return id_alley_bricks;
        }

        const int corner_dist = p_x_edge_dist > p_y_edge_dist ? p_x_edge_dist : p_y_edge_dist;
        if (corner_dist < EDGE_PAVEMENT_WIDTH) return id_alley_flagstone;
        if (corner_dist < EDGE_PAVEMENT_WIDTH + EDGE_GARDEN_WIDTH) return garden_tile();
        return id_alley_flagstone;
    };

    if (!simple_straight) {
        if (west_connected && north_connected && west_dist < EDGE_BAND_WIDTH && north_dist < EDGE_BAND_WIDTH) {
            if (has_complete_road_network_corner(-1, -1)) return compact_corner_tile();
            return corner_tile(west_dist, north_dist);
        }
        if (east_connected && north_connected && east_dist < EDGE_BAND_WIDTH && north_dist < EDGE_BAND_WIDTH) {
            if (has_complete_road_network_corner(1, -1)) return compact_corner_tile();
            return corner_tile(east_dist, north_dist);
        }
        if (west_connected && south_connected && west_dist < EDGE_BAND_WIDTH && south_dist < EDGE_BAND_WIDTH) {
            if (has_complete_road_network_corner(-1, 1)) return compact_corner_tile();
            return corner_tile(west_dist, south_dist);
        }
        if (east_connected && south_connected && east_dist < EDGE_BAND_WIDTH && south_dist < EDGE_BAND_WIDTH) {
            if (has_complete_road_network_corner(1, 1)) return compact_corner_tile();
            return corner_tile(east_dist, south_dist);
        }
    }

    if (in_pavement_strip(west_connected, west_dist) ||
        in_pavement_strip(east_connected, east_dist) ||
        in_pavement_strip(north_connected, north_dist) ||
        in_pavement_strip(south_connected, south_dist)) {
        return id_alley_flagstone;
    }

    const bool west_garden = in_garden_strip(west_connected, west_dist);
    const bool east_garden = in_garden_strip(east_connected, east_dist);
    const bool north_garden = in_garden_strip(north_connected, north_dist);
    const bool south_garden = in_garden_strip(south_connected, south_dist);

    const bool has_building_entrance =
        (west_garden && alley_garden_strip_has_building_entrance(p_local_x, p_local_y, p_world_x, p_world_y, WorldCoords::NEIGH_WEST, p_world_seed)) ||
        (east_garden && alley_garden_strip_has_building_entrance(p_local_x, p_local_y, p_world_x, p_world_y, WorldCoords::NEIGH_EAST, p_world_seed)) ||
        (north_garden && alley_garden_strip_has_building_entrance(p_local_x, p_local_y, p_world_x, p_world_y, WorldCoords::NEIGH_NORTH, p_world_seed)) ||
        (south_garden && alley_garden_strip_has_building_entrance(p_local_x, p_local_y, p_world_x, p_world_y, WorldCoords::NEIGH_SOUTH, p_world_seed));
    if (has_building_entrance) {
        uint32_t h = get_hash(p_world_x, p_world_y, static_cast<uint32_t>(p_world_seed) ^ 0xA11E7u);
        if (h % 100 < 66) {
            return id_dirt;
        }
    }

    if (west_garden || east_garden || north_garden || south_garden) {
        return garden_tile();
    }

    return id_alley_bricks;
}

bool WorldGenerator::alley_garden_strip_has_building_entrance(
    int p_local_x,
    int p_local_y,
    int p_world_x,
    int p_world_y,
    WorldCoords::NeighborBits p_side,
    int p_world_seed
) const {
    int adjacent_chunk_x = floor_div_chunk(p_world_x);
    int adjacent_chunk_y = floor_div_chunk(p_world_y);
    uint8_t required_rotation = WorldCoords::ROT_SOUTH;
    int edge_coord = 0;

    switch (p_side) {
        case WorldCoords::NEIGH_WEST:
            adjacent_chunk_x -= 1;
            required_rotation = WorldCoords::ROT_EAST;
            edge_coord = p_local_y;
            break;
        case WorldCoords::NEIGH_EAST:
            adjacent_chunk_x += 1;
            required_rotation = WorldCoords::ROT_WEST;
            edge_coord = p_local_y;
            break;
        case WorldCoords::NEIGH_NORTH:
            adjacent_chunk_y -= 1;
            required_rotation = WorldCoords::ROT_SOUTH;
            edge_coord = p_local_x;
            break;
        case WorldCoords::NEIGH_SOUTH:
            adjacent_chunk_y += 1;
            required_rotation = WorldCoords::ROT_NORTH;
            edge_coord = p_local_x;
            break;
        default:
            return false;
    }

    const BiomeLayer* surface_layer = get_biome_layer(0);
    if (!surface_layer) return false;

    uint64_t adjacent_key = WorldCoords::pack_coords(adjacent_chunk_x, adjacent_chunk_y);
    auto chunk_it = surface_layer->overrides.find(adjacent_key);
    if (chunk_it == surface_layer->overrides.end()) return false;

    const uint32_t packed = chunk_it->second;
    const uint16_t adjacent_chunk_id = static_cast<uint16_t>(packed & WorldCoords::ID_MASK);
    const uint8_t adjacent_rotation = static_cast<uint8_t>((packed >> WorldCoords::ORIENTATION_SHIFT) & WorldCoords::ROTATION_MASK);
    if (adjacent_rotation != required_rotation) return false;

    ChunkDb* chunk_db = ChunkDb::get_singleton();
    const ChunkInfo* chunk_info = chunk_db ? chunk_db->get_chunk_info(adjacent_chunk_id) : nullptr;
    if (!chunk_info || chunk_info->structure_type.is_empty()) return false;

    const int adjacent_world_x = adjacent_chunk_x * WorldCoords::CHUNK_SIZE;
    const int adjacent_world_y = adjacent_chunk_y * WorldCoords::CHUNK_SIZE;
    StructureDb* structure_db = StructureDb::get_singleton();
    String structure_id = get_structure_id_for_cell(adjacent_world_x, adjacent_world_y, p_world_seed);
    const StructureInfo* structure = structure_db ? structure_db->get_structure_info(structure_id) : nullptr;
    if (!structure || structure->entrances.empty()) return false;

    const int max_coord = WorldCoords::CHUNK_SIZE - 1;
    for (int entrance : structure->entrances) {
        if (entrance < 0 || entrance > max_coord) continue;

        int rotated_edge_coord = entrance;
        switch (adjacent_rotation) {
            case WorldCoords::ROT_NORTH:
            case WorldCoords::ROT_EAST:
                rotated_edge_coord = max_coord - entrance;
                break;
            default:
                rotated_edge_coord = entrance;
                break;
        }

        if (edge_coord >= rotated_edge_coord - 1 && edge_coord <= rotated_edge_coord + 1) return true;
    }

    return false;
}

uint32_t WorldGenerator::get_biome_chunk_data(int p_chunk_x, int p_chunk_y, int p_z, int) {
    setup_biome_rules();
    const BiomeLayer* layer = get_biome_layer(p_z);
    if (!layer) return get_default_biome_chunk_data(p_z);

    uint64_t chunk_key = WorldCoords::pack_coords(p_chunk_x, p_chunk_y);
    auto it = layer->overrides.find(chunk_key);
    return it != layer->overrides.end() ? it->second : layer->default_chunk_data;
}

uint16_t WorldGenerator::get_biome_id_for_chunk(int p_chunk_x, int p_chunk_y, int p_z, int p_world_seed) {
    return static_cast<uint16_t>(get_biome_chunk_data(p_chunk_x, p_chunk_y, p_z, p_world_seed) & WorldCoords::ID_MASK);
}

uint16_t WorldGenerator::get_biome_id_for_cell(int x, int y, int z, int world_seed) {
    return get_biome_id_for_chunk(floor_div_chunk(x), floor_div_chunk(y), z, world_seed);
}

uint16_t WorldGenerator::get_chunk_id_for_cell(int x, int y) const {
    int cx = floor_div_chunk(x);
    int cy = floor_div_chunk(y);
    uint64_t chunk_key = WorldCoords::pack_coords(cx, cy);
    const BiomeLayer* layer = get_biome_layer(0);
    if (!layer) return id_void;
    auto it = layer->overrides.find(chunk_key);
    if (it == layer->overrides.end()) return id_void;
    return static_cast<uint16_t>(it->second & WorldCoords::ID_MASK);
}

uint8_t WorldGenerator::get_chunk_rotation_for_cell(int x, int y) const {
    int cx = floor_div_chunk(x);
    int cy = floor_div_chunk(y);
    uint64_t chunk_key = WorldCoords::pack_coords(cx, cy);
    const BiomeLayer* layer = get_biome_layer(0);
    if (!layer) return WorldCoords::ROT_SOUTH;
    auto it = layer->overrides.find(chunk_key);
    if (it == layer->overrides.end()) return WorldCoords::ROT_SOUTH;
    return static_cast<uint8_t>((it->second >> WorldCoords::ORIENTATION_SHIFT) & WorldCoords::ROTATION_MASK);
}

String WorldGenerator::get_structure_id_for_chunk(uint16_t p_chunk_id) const {
    StructureDb* structure_db = StructureDb::get_singleton();
    ChunkDb* chunk_db = ChunkDb::get_singleton();
    const ChunkInfo* chunk_info = chunk_db ? chunk_db->get_chunk_info(p_chunk_id) : nullptr;
    if (!structure_db || !chunk_info || chunk_info->structure_type.is_empty()) return "";

    const std::vector<String>* structures = structure_db->get_structure_ids_by_type(chunk_info->structure_type);
    if (!structures || structures->empty()) return "";
    return structures->front();
}

String WorldGenerator::get_structure_id_for_cell(int x, int y, int world_seed) const {
    return get_structure_id_for_cell(x, y, 0, world_seed);
}

String WorldGenerator::get_structure_id_for_cell(int x, int y, int z, int world_seed) const {
    uint16_t chunk_id = get_chunk_id_for_cell(x, y);
    ChunkDb* chunk_db = ChunkDb::get_singleton();
    const ChunkInfo* chunk_info = chunk_db ? chunk_db->get_chunk_info(chunk_id) : nullptr;
    if (!chunk_info || chunk_info->structure_type.is_empty()) return "";

    int cx = (x >= 0) ? (x / WorldCoords::CHUNK_SIZE) : ((x - (WorldCoords::CHUNK_SIZE - 1)) / WorldCoords::CHUNK_SIZE);
    int cy = (y >= 0) ? (y / WorldCoords::CHUNK_SIZE) : ((y - (WorldCoords::CHUNK_SIZE - 1)) / WorldCoords::CHUNK_SIZE);

    StructureDb* structure_db = StructureDb::get_singleton();
    const std::vector<String>* structures = structure_db ? structure_db->get_structure_ids_by_type(chunk_info->structure_type) : nullptr;
    String structure_id;
    if (!structures || structures->empty()) {
        structure_id = "house01";
    } else {
        uint64_t h = Rng::hash_pos(static_cast<uint32_t>(world_seed), Vector2i(cx, cy), Rng::BIOME);
        structure_id = (*structures)[h % structures->size()];
    }

    const StructureInfo* structure = structure_db ? structure_db->get_structure_info(structure_id) : nullptr;
    if (!structure || structure->levels.find(z) == structure->levels.end()) return "";
    return structure_id;
}

uint16_t WorldGenerator::get_base_surface_tile(int x, int y, int world_seed) {
    CityStructureContext city_structure = get_city_structure_context(x, y, 0);

    int cx = floor_div_chunk(x);
    int cy = floor_div_chunk(y);
    uint64_t chunk_key = WorldCoords::pack_coords(cx, cy);

    if (!last_chunk_valid || last_chunk_key != chunk_key) {
        const BiomeLayer* surface_layer = get_biome_layer(0);
        if (!surface_layer) {
            last_chunk_valid = false;
            return id_void;
        }
        auto it = surface_layer->overrides.find(chunk_key);
        if (it == surface_layer->overrides.end()) {
            last_chunk_valid = false;
            return id_void;
        }
        
        uint32_t packed = it->second;
        last_chunk_id = static_cast<uint16_t>(packed & WorldCoords::ID_MASK);
        last_chunk_rotation = static_cast<uint8_t>(packed >> WorldCoords::ORIENTATION_SHIFT);
        last_chunk_neighbors = static_cast<uint8_t>((packed >> WorldCoords::NEIGHBOR_SHIFT) & WorldCoords::NEIGHBOR_MASK);
        last_chunk_key = chunk_key;
        
        auto it_rule = biome_rules.find(last_chunk_id);
        last_biome_ptr = (it_rule != biome_rules.end()) ? &it_rule->second : nullptr;
        last_chunk_valid = true;
    }

    const uint16_t chunk_id = last_chunk_id;

    if (chunk_id == id_alley) {
        int lx = x % WorldCoords::CHUNK_SIZE; if (lx < 0) lx += WorldCoords::CHUNK_SIZE;
        int ly = y % WorldCoords::CHUNK_SIZE; if (ly < 0) ly += WorldCoords::CHUNK_SIZE;
        return get_alley_surface_tile(lx, ly, x, y, world_seed);
    }

    if (last_biome_ptr && last_biome_ptr->auto_tiled) {
        int lx = x % WorldCoords::CHUNK_SIZE; if (lx < 0) lx += WorldCoords::CHUNK_SIZE;
        int ly = y % WorldCoords::CHUNK_SIZE; if (ly < 0) ly += WorldCoords::CHUNK_SIZE;
        static constexpr int BORDER_WIDTH = 2;
        bool west = lx < BORDER_WIDTH;
        bool east = lx >= WorldCoords::CHUNK_SIZE - BORDER_WIDTH;
        bool north = ly < BORDER_WIDTH;
        bool south = ly >= WorldCoords::CHUNK_SIZE - BORDER_WIDTH;
        if ((west || east) && (north || south)) return last_biome_ptr->border_tile_id;
        if ((west && !(last_chunk_neighbors & WorldCoords::NEIGH_WEST)) ||
            (east && !(last_chunk_neighbors & WorldCoords::NEIGH_EAST)) ||
            (north && !(last_chunk_neighbors & WorldCoords::NEIGH_NORTH)) ||
            (south && !(last_chunk_neighbors & WorldCoords::NEIGH_SOUTH))) {
            return last_biome_ptr->border_tile_id;
        }
    }

    if (city_structure.valid && s_db) {
        const uint16_t structure_tile = s_db->get_tile_at(
            city_structure.structure_id,
            city_structure.local_pos.x,
            city_structure.local_pos.y,
            0,
            get_hash(x, y, static_cast<uint32_t>(world_seed))
        );
        if (structure_tile != id_void) return structure_tile;
        if (last_biome_ptr) {
            return pick_weighted_tile(*last_biome_ptr, get_hash(x, y, static_cast<uint32_t>(world_seed)));
        }
        return id_void;
    }

    ChunkDb* chunk_db = ChunkDb::get_singleton();
    const ChunkInfo* chunk_info = chunk_db ? chunk_db->get_chunk_info(chunk_id) : nullptr;
    if (chunk_info && !chunk_info->structure_type.is_empty() && s_db) {
        int lx = x % WorldCoords::CHUNK_SIZE; if (lx < 0) lx += WorldCoords::CHUNK_SIZE;
        int ly = y % WorldCoords::CHUNK_SIZE; if (ly < 0) ly += WorldCoords::CHUNK_SIZE;
        int rx = lx, ry = ly;
        int max_coord = WorldCoords::CHUNK_SIZE - 1;
        switch (last_chunk_rotation) {
            case WorldCoords::ROT_WEST: rx = ly; ry = max_coord - lx; break;
            case WorldCoords::ROT_NORTH: rx = max_coord - lx; ry = max_coord - ly; break;
            case WorldCoords::ROT_EAST: rx = max_coord - ly; ry = lx; break;
        }
        String structure_id = get_structure_id_for_cell(x, y, world_seed);
        uint32_t structure_hash = get_hash(x, y, static_cast<uint32_t>(world_seed));
        uint16_t tile_id = s_db->get_tile_at(structure_id, rx, ry, 0, structure_hash);
        if (tile_id != id_void) return tile_id;
    }

    if (last_biome_ptr) {
        uint32_t h = get_hash(x, y, static_cast<uint32_t>(world_seed));
        return pick_weighted_tile(*last_biome_ptr, h);
    }

    return id_void;
}

uint16_t WorldGenerator::get_surface_feature_tile_at(
    const String& p_feature_id,
    int p_local_x,
    int p_local_y,
    const Vector2i& p_source_size,
    uint8_t p_rotation,
    uint32_t p_position_hash
) const {
    const Vector2i source_pos = resolve_surface_feature_source_pos(p_local_x, p_local_y, p_source_size, p_rotation);
    return s_db ? s_db->get_tile_at(p_feature_id, source_pos.x, source_pos.y, 0, p_position_hash) : id_void;
}

bool WorldGenerator::validate_surface_feature_anchor(
    const String& p_feature_id,
    const Vector2i& p_origin,
    const Vector2i& p_source_size,
    const Vector2i& p_placed_size,
    uint8_t p_rotation,
    int p_world_seed,
    bool p_require_source_chunk,
    int p_source_chunk_x,
    int p_source_chunk_y
) {
    for (int ly = 0; ly < p_placed_size.y; ly++) {
        for (int lx = 0; lx < p_placed_size.x; lx++) {
            const uint32_t feature_hash = get_hash(p_origin.x + lx, p_origin.y + ly, static_cast<uint32_t>(p_world_seed));
            const uint16_t feature_tile = get_surface_feature_tile_at(p_feature_id, lx, ly, p_source_size, p_rotation, feature_hash);
            if (feature_tile == 0 || feature_tile == id_void) continue;

            const int world_x = p_origin.x + lx;
            const int world_y = p_origin.y + ly;
            if (p_require_source_chunk) {
                if (floor_div_chunk(world_x) != p_source_chunk_x || floor_div_chunk(world_y) != p_source_chunk_y) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool WorldGenerator::find_feature_at(int x, int y, int z, int world_seed, SurfaceFeatureInstance& r_instance, bool p_include_void_tiles) {
    static constexpr int ROAD_SHOULDER_OFFSET = 3;
    static constexpr int ALLEY_GARDEN_START = 2;
    static constexpr int ALLEY_GARDEN_WIDTH = 3;
    static constexpr uint64_t SURFACE_FEATURE_SALT = 0x5355524645415455ULL; // "SURFEATU"
    static const String ROADSIDE_PLACEMENT = "roadside";
    static const String ALLEY_INLINE_PLACEMENT = "alley_inline";
    static const String FIXED_AREAS_PLACEMENT = "fixed_areas";

    r_instance = SurfaceFeatureInstance();
    setup_biome_rules();

    if (!s_db) return false;

    FeatureDb* feature_db = FeatureDb::get_singleton();
    if (!feature_db) return false;

    ChunkDb* chunk_db = ChunkDb::get_singleton();
    TagRegistry* tag_reg = TagRegistry::get_singleton();
    const uint16_t road_tag_id = tag_reg ? tag_reg->get_tag_id("ROAD") : 0;
    if (!chunk_db) return false;

    const BiomeLayer* feature_layer = get_biome_layer(z);
    if (!feature_layer) return false;

    const int current_cx = floor_div_chunk(x);
    const int current_cy = floor_div_chunk(y);

    auto get_chunk_data = [&](int p_chunk_x, int p_chunk_y, uint32_t& r_packed, uint16_t& r_chunk_id, uint8_t& r_neighbor_mask) -> bool {
        const uint64_t chunk_key = WorldCoords::pack_coords(p_chunk_x, p_chunk_y);
        auto chunk_it = feature_layer->overrides.find(chunk_key);
        if (chunk_it != feature_layer->overrides.end()) {
            r_packed = chunk_it->second;
        } else if (feature_layer->default_chunk_data != 0) {
            r_packed = feature_layer->default_chunk_data;
        } else {
            return false;
        }
        r_chunk_id = static_cast<uint16_t>(r_packed & WorldCoords::ID_MASK);
        r_neighbor_mask = static_cast<uint8_t>((r_packed >> WorldCoords::NEIGHBOR_SHIFT) & WorldCoords::NEIGHBOR_MASK);
        return true;
    };

    auto candidates_overlap = [&](const SurfaceFeatureInstance& p_a, const SurfaceFeatureInstance& p_b) {
        const int min_x = p_a.origin.x > p_b.origin.x ? p_a.origin.x : p_b.origin.x;
        const int min_y = p_a.origin.y > p_b.origin.y ? p_a.origin.y : p_b.origin.y;
        const int max_x_a = p_a.origin.x + p_a.placed_size.x;
        const int max_y_a = p_a.origin.y + p_a.placed_size.y;
        const int max_x_b = p_b.origin.x + p_b.placed_size.x;
        const int max_y_b = p_b.origin.y + p_b.placed_size.y;
        const int max_x = max_x_a < max_x_b ? max_x_a : max_x_b;
        const int max_y = max_y_a < max_y_b ? max_y_a : max_y_b;

        if (min_x >= max_x || min_y >= max_y) return false;

        for (int wy = min_y; wy < max_y; wy++) {
            for (int wx = min_x; wx < max_x; wx++) {
                const uint16_t tile_a = get_surface_feature_tile_at(
                    p_a.feature_id,
                    wx - p_a.origin.x,
                    wy - p_a.origin.y,
                    p_a.source_size,
                    p_a.rotation
                );
                if (tile_a == 0 || tile_a == id_void) continue;

                const uint16_t tile_b = get_surface_feature_tile_at(
                    p_b.feature_id,
                    wx - p_b.origin.x,
                    wy - p_b.origin.y,
                    p_b.source_size,
                    p_b.rotation
                );
                if (tile_b != 0 && tile_b != id_void) return true;
            }
        }

        return false;
    };

    // Structure-scoped features are resolved against the complete authored
    // structure layout. This allows one feature to cross chunk boundaries
    // without making the placement rules specific to any one structure type.
    uint32_t current_packed = 0;
    uint16_t current_chunk_id = 0;
    uint8_t current_neighbor_mask = 0;
    if (z == 0 && get_chunk_data(current_cx, current_cy, current_packed, current_chunk_id, current_neighbor_mask)) {
        (void)current_neighbor_mask;
        const ChunkInfo* current_chunk_info = chunk_db->get_chunk_info(current_chunk_id);
        const CityStructureContext owner_context = get_city_structure_context(x, y, z);
        const StructureInfo* owner_structure = owner_context.valid
            ? s_db->get_structure_info(owner_context.structure_id)
            : nullptr;
        bool has_structure_scoped_features = false;
        if (current_chunk_info && owner_structure) {
            for (const ChunkFeatureSpawnInfo& spawn : current_chunk_info->feature_spawns) {
                if (spawn.scope == "structure") {
                    has_structure_scoped_features = true;
                    break;
                }
            }
        }

        if (has_structure_scoped_features) {
            const uint8_t owner_rotation = static_cast<uint8_t>(
                (current_packed >> WorldCoords::ORIENTATION_SHIFT) & WorldCoords::ROTATION_MASK
            );
            const Vector2i owner_local_pos = resolve_surface_feature_placed_pos(
                owner_context.local_pos.x,
                owner_context.local_pos.y,
                owner_structure->size,
                owner_rotation
            );
            const Vector2i owner_origin(x - owner_local_pos.x, y - owner_local_pos.y);
            const Vector2i owner_chunk(floor_div_chunk(owner_origin.x), floor_div_chunk(owner_origin.y));
            const uint64_t owner_key = WorldCoords::pack_coords(owner_chunk.x, owner_chunk.y) ^
                static_cast<uint64_t>(owner_context.structure_id.hash());
            const Vector2i rotated_owner_size = get_rotated_surface_feature_size(owner_structure->size, owner_rotation);
            auto validate_structure_feature_anchor = [&](const SurfaceFeatureInstance& p_candidate) {
                for (int ly = 0; ly < p_candidate.placed_size.y; ly++) {
                    for (int lx = 0; lx < p_candidate.placed_size.x; lx++) {
                        const uint32_t feature_hash = get_hash(
                            p_candidate.origin.x + lx,
                            p_candidate.origin.y + ly,
                            static_cast<uint32_t>(world_seed)
                        );
                        const uint16_t feature_tile = get_surface_feature_tile_at(
                            p_candidate.feature_id,
                            lx,
                            ly,
                            p_candidate.source_size,
                            p_candidate.rotation,
                            feature_hash
                        );
                        if (feature_tile == 0 || feature_tile == id_void) continue;

                        const Vector2i owner_local_pos(
                            p_candidate.origin.x + lx - owner_origin.x,
                            p_candidate.origin.y + ly - owner_origin.y
                        );
                        if (owner_local_pos.x < 0 || owner_local_pos.y < 0 ||
                            owner_local_pos.x >= rotated_owner_size.x || owner_local_pos.y >= rotated_owner_size.y) {
                            return false;
                        }

                        const Vector2i source_pos = resolve_surface_feature_source_pos(
                            owner_local_pos.x,
                            owner_local_pos.y,
                            owner_structure->size,
                            owner_rotation
                        );
                        const uint16_t parent_tile = s_db->get_tile_at(
                            owner_context.structure_id,
                            source_pos.x,
                            source_pos.y,
                            0,
                            feature_hash
                        );
                        if (parent_tile != 0 && parent_tile != id_void) return false;
                    }
                }
                return true;
            };
            std::vector<SurfaceFeatureInstance> structure_candidates;
            std::vector<String> unique_feature_ids;
            int structure_candidate_index = 0;

            for (const ChunkFeatureSpawnInfo& spawn : current_chunk_info->feature_spawns) {
                if (spawn.scope != "structure" || spawn.areas.empty()) continue;
                const FeaturePoolInfo* pool = feature_db->get_feature_pool(spawn.pool);
                if (!pool || pool->type != "structure_stamp") continue;

                for (int area_index = 0; area_index < static_cast<int>(spawn.areas.size()); area_index++) {
                    const ChunkFeatureAreaInfo& area = spawn.areas[area_index];
                    for (int candidate_index = 0; candidate_index < spawn.candidates; candidate_index++) {
                        const int rule_index = structure_candidate_index++;
                        Rng::Seeded rng = Rng::at(
                            static_cast<uint32_t>(world_seed),
                            owner_chunk,
                            Rng::BIOME,
                            SURFACE_FEATURE_SALT ^ owner_key ^
                                (static_cast<uint64_t>(rule_index) * 0x9E3779B97F4A7C15ULL)
                        );
                        if (!rng.chance(spawn.chance)) continue;

                        const bool follows_structure = spawn.rotation_mode == "structure" || spawn.rotation_mode == "center";
                        const uint8_t area_rotation = follows_structure ? owner_rotation : WorldCoords::ROT_SOUTH;
                        const uint8_t local_feature_rotation = !area.facing.is_empty()
                            ? parse_feature_facing(area.facing, WorldCoords::ROT_SOUTH)
                            : spawn.rotation_mode == "center"
                                ? get_center_facing_rotation(
                                    area.origin,
                                    area.size,
                                    owner_structure->size
                                )
                                : WorldCoords::ROT_SOUTH;
                        const uint8_t feature_rotation = follows_structure
                            ? compose_surface_feature_rotation(local_feature_rotation, owner_rotation)
                            : WorldCoords::ROT_SOUTH;
                        const Vector2i rotated_area_size = follows_structure
                            ? get_rotated_surface_feature_size(area.size, area_rotation)
                            : area.size;

                        std::vector<const FeatureEntryInfo*> fitting_entries;
                        int fitting_total_weight = 0;
                        for (const FeatureEntryInfo& pool_entry : pool->entries) {
                            if (pool_entry.structure_id.is_empty() || pool_entry.weight <= 0) continue;
                            if (spawn.unique &&
                                std::find(unique_feature_ids.begin(), unique_feature_ids.end(), pool_entry.structure_id) != unique_feature_ids.end()) {
                                continue;
                            }
                            const Vector2i pool_source_size = s_db->get_structure_size(pool_entry.structure_id);
                            if (pool_source_size.x <= 0 || pool_source_size.y <= 0) continue;
                            const Vector2i pool_placed_size = get_rotated_surface_feature_size(
                                pool_source_size,
                                feature_rotation
                            );
                            if (pool_placed_size.x > rotated_area_size.x ||
                                pool_placed_size.y > rotated_area_size.y) {
                                continue;
                            }
                            fitting_entries.push_back(&pool_entry);
                            fitting_total_weight += pool_entry.weight;
                        }
                        if (fitting_entries.empty() || fitting_total_weight <= 0) continue;

                        int fitting_roll = rng.range(1, fitting_total_weight);
                        const FeatureEntryInfo* entry = nullptr;
                        for (const FeatureEntryInfo* pool_entry : fitting_entries) {
                            fitting_roll -= pool_entry->weight;
                            if (fitting_roll <= 0) {
                                entry = pool_entry;
                                break;
                            }
                        }
                        if (!entry) continue;

                        SurfaceFeatureInstance candidate;
                        candidate.chunk_x = owner_chunk.x;
                        candidate.chunk_y = owner_chunk.y;
                        candidate.index = rule_index;
                        candidate.feature_id = entry->structure_id;
                        candidate.unique = spawn.unique;
                        candidate.source_size = s_db->get_structure_size(candidate.feature_id);
                        if (candidate.source_size.x <= 0 || candidate.source_size.y <= 0) continue;

                        candidate.rotation = feature_rotation;
                        candidate.placed_size = get_rotated_surface_feature_size(candidate.source_size, candidate.rotation);
                        if (candidate.placed_size.x > rotated_area_size.x ||
                            candidate.placed_size.y > rotated_area_size.y) {
                            continue;
                        }
                        const Vector2i rotated_area_origin = follows_structure
                            ? rotate_layout_pos(area.origin, area.size, owner_structure->size, area_rotation)
                            : area.origin;
                        candidate.origin = owner_origin + rotated_area_origin;
                        if (!validate_surface_feature_anchor(
                                candidate.feature_id,
                                candidate.origin,
                                candidate.source_size,
                                candidate.placed_size,
                                candidate.rotation,
                                world_seed,
                                false,
                                owner_chunk.x,
                                owner_chunk.y)) {
                            continue;
                        }
                        if (!validate_structure_feature_anchor(candidate)) continue;

                        if (candidate.unique && std::find(unique_feature_ids.begin(), unique_feature_ids.end(), candidate.feature_id) != unique_feature_ids.end()) {
                            continue;
                        }
                        bool overlaps = false;
                        for (const SurfaceFeatureInstance& prior : structure_candidates) {
                            if (candidates_overlap(candidate, prior)) {
                                overlaps = true;
                                break;
                            }
                        }
                        if (overlaps) continue;

                        if (candidate.unique) unique_feature_ids.push_back(candidate.feature_id);
                        structure_candidates.push_back(candidate);
                    }
                }
            }

            for (const SurfaceFeatureInstance& candidate : structure_candidates) {
                if (x < candidate.origin.x || x >= candidate.origin.x + candidate.placed_size.x ||
                    y < candidate.origin.y || y >= candidate.origin.y + candidate.placed_size.y) {
                    continue;
                }
                const int local_x = x - candidate.origin.x;
                const int local_y = y - candidate.origin.y;
                const uint32_t feature_hash = get_hash(x, y, static_cast<uint32_t>(world_seed));
                const uint16_t tile_id = get_surface_feature_tile_at(
                    candidate.feature_id,
                    local_x,
                    local_y,
                    candidate.source_size,
                    candidate.rotation,
                    feature_hash
                );
                if (p_include_void_tiles || (tile_id != 0 && tile_id != id_void)) {
                    r_instance = candidate;
                    return true;
                }
            }
        }
    }

    auto placement_is_available = [&](int p_chunk_x, int p_chunk_y, const String& p_placement, uint16_t p_chunk_id, uint8_t p_neighbor_mask, const ChunkFeatureSpawnInfo& p_spawn) -> bool {
        const bool north_south = (p_neighbor_mask & WorldCoords::NEIGH_NORTH) || (p_neighbor_mask & WorldCoords::NEIGH_SOUTH);
        const bool east_west = (p_neighbor_mask & WorldCoords::NEIGH_EAST) || (p_neighbor_mask & WorldCoords::NEIGH_WEST);
        if (p_placement == ROADSIDE_PLACEMENT) {
            return road_tag_id != 0 && chunk_db->has_tag(p_chunk_id, road_tag_id) && (north_south || east_west);
        }
        if (p_placement == ALLEY_INLINE_PLACEMENT) {
            return p_chunk_id == id_alley && (north_south || east_west);
        }
        if (p_placement == FIXED_AREAS_PLACEMENT) {
            return !p_spawn.areas.empty();
        }
        return false;
    };

    auto spawn_expanded_count = [&](int p_chunk_x, int p_chunk_y, uint16_t p_chunk_id, uint8_t p_neighbor_mask, const ChunkFeatureSpawnInfo& p_spawn) -> int {
        if (p_spawn.scope == "structure") return 0;
        if (!placement_is_available(p_chunk_x, p_chunk_y, p_spawn.placement, p_chunk_id, p_neighbor_mask, p_spawn)) return 0;
        const int area_count = p_spawn.placement == FIXED_AREAS_PLACEMENT ? static_cast<int>(p_spawn.areas.size()) : 1;
        return p_spawn.candidates * area_count;
    };

    auto feature_candidate_count_for_chunk = [&](int p_chunk_x, int p_chunk_y) -> int {
        uint32_t packed = 0;
        uint16_t chunk_id = 0;
        uint8_t neighbor_mask = 0;
        if (!get_chunk_data(p_chunk_x, p_chunk_y, packed, chunk_id, neighbor_mask)) return 0;

        const ChunkInfo* chunk_info = chunk_db->get_chunk_info(chunk_id);
        if (!chunk_info) return 0;

        int count = 0;
        for (const ChunkFeatureSpawnInfo& spawn : chunk_info->feature_spawns) {
            count += spawn_expanded_count(p_chunk_x, p_chunk_y, chunk_id, neighbor_mask, spawn);
        }
        return count;
    };

    auto build_candidate = [&](int p_chunk_x, int p_chunk_y, int p_index) -> SurfaceFeatureInstance {
        SurfaceFeatureInstance candidate;
        candidate.chunk_x = p_chunk_x;
        candidate.chunk_y = p_chunk_y;
        candidate.index = p_index;

        uint32_t packed = 0;
        uint16_t chunk_id = 0;
        uint8_t neighbor_mask = 0;
        if (!get_chunk_data(p_chunk_x, p_chunk_y, packed, chunk_id, neighbor_mask)) return candidate;
        const uint8_t chunk_rotation = static_cast<uint8_t>((packed >> WorldCoords::ORIENTATION_SHIFT) & WorldCoords::ROTATION_MASK);

        const ChunkInfo* chunk_info = chunk_db->get_chunk_info(chunk_id);
        if (!chunk_info) return candidate;

        const ChunkFeatureSpawnInfo* spawn_info = nullptr;
        int local_index = p_index;
        for (const ChunkFeatureSpawnInfo& spawn : chunk_info->feature_spawns) {
            const int expanded_count = spawn_expanded_count(p_chunk_x, p_chunk_y, chunk_id, neighbor_mask, spawn);
            if (expanded_count <= 0) continue;
            if (local_index < expanded_count) {
                spawn_info = &spawn;
                break;
            }
            local_index -= expanded_count;
        }
        if (!spawn_info) return candidate;

        Rng::Seeded rng = Rng::at(
            static_cast<uint32_t>(world_seed),
            Vector2i(p_chunk_x, p_chunk_y),
            Rng::BIOME,
            SURFACE_FEATURE_SALT + static_cast<uint64_t>(p_index) * 0x9E3779B97F4A7C15ULL
        );
        if (!rng.chance(spawn_info->chance)) return candidate;

        const FeaturePoolInfo* pool = feature_db->get_feature_pool(spawn_info->pool);
        if (!pool || pool->type != "structure_stamp") return candidate;
        const FeatureEntryInfo* entry = feature_db->pick_weighted_entry(*pool, rng);
        if (!entry || entry->structure_id.is_empty()) return candidate;

        candidate.feature_id = entry->structure_id;
        candidate.unique = spawn_info->unique;
        candidate.source_size = s_db->get_structure_size(candidate.feature_id);
        if (candidate.source_size.x <= 0 || candidate.source_size.y <= 0) return candidate;

        const int chunk_world_x = p_chunk_x * WorldCoords::CHUNK_SIZE;
        const int chunk_world_y = p_chunk_y * WorldCoords::CHUNK_SIZE;
        int anchor_x = chunk_world_x;
        int anchor_y = chunk_world_y;

        if (spawn_info->placement == FIXED_AREAS_PLACEMENT) {
            const int area_index = local_index / spawn_info->candidates;
            if (area_index < 0 || area_index >= static_cast<int>(spawn_info->areas.size())) return candidate;
            const ChunkFeatureAreaInfo& area = spawn_info->areas[area_index];

            const bool follows_chunk_area = spawn_info->rotation_mode == "chunk" || spawn_info->rotation_mode == "center";
            const uint8_t area_rotation = follows_chunk_area ? chunk_rotation : WorldCoords::ROT_SOUTH;
            const uint8_t local_feature_rotation = !area.facing.is_empty()
                ? parse_feature_facing(area.facing, WorldCoords::ROT_SOUTH)
                : spawn_info->rotation_mode == "center"
                    ? get_center_facing_rotation(
                        area.origin,
                        area.size,
                        Vector2i(WorldCoords::CHUNK_SIZE, WorldCoords::CHUNK_SIZE)
                    )
                    : WorldCoords::ROT_SOUTH;

            candidate.rotation = compose_surface_feature_rotation(local_feature_rotation, area_rotation);
            const Vector2i rotated_area_origin = follows_chunk_area
                ? rotate_chunk_local_pos(area.origin, area.size, area_rotation)
                : area.origin;
            candidate.placed_size = get_rotated_surface_feature_size(candidate.source_size, candidate.rotation);
            const Vector2i rotated_area_size = follows_chunk_area
                ? get_rotated_surface_feature_size(area.size, area_rotation)
                : area.size;
            if (candidate.placed_size.x > rotated_area_size.x ||
                candidate.placed_size.y > rotated_area_size.y) {
                return candidate;
            }

            anchor_x += rotated_area_origin.x;
            anchor_y += rotated_area_origin.y;
            candidate.require_source_chunk = true;
        } else {
            const bool north_south = (neighbor_mask & WorldCoords::NEIGH_NORTH) || (neighbor_mask & WorldCoords::NEIGH_SOUTH);
            const bool east_west = (neighbor_mask & WorldCoords::NEIGH_EAST) || (neighbor_mask & WorldCoords::NEIGH_WEST);
            const bool west_open = (neighbor_mask & WorldCoords::NEIGH_WEST) == 0;
            const bool east_open = (neighbor_mask & WorldCoords::NEIGH_EAST) == 0;
            const bool north_open = (neighbor_mask & WorldCoords::NEIGH_NORTH) == 0;
            const bool south_open = (neighbor_mask & WorldCoords::NEIGH_SOUTH) == 0;
            const bool can_place_vertical = north_south && (west_open || east_open);
            const bool can_place_horizontal = east_west && (north_open || south_open);
            if (!can_place_vertical && !can_place_horizontal) return candidate;

            const bool place_horizontal = can_place_vertical && can_place_horizontal
                ? rng.range(0, 1) == 1
                : can_place_horizontal;
            const bool flipped = rng.range(0, 1) == 1;
            candidate.rotation = place_horizontal
                ? (flipped ? WorldCoords::ROT_NORTH : WorldCoords::ROT_SOUTH)
                : (flipped ? WorldCoords::ROT_WEST : WorldCoords::ROT_EAST);
            candidate.placed_size = get_rotated_surface_feature_size(candidate.source_size, candidate.rotation);
            if (candidate.placed_size.x > WorldCoords::CHUNK_SIZE || candidate.placed_size.y > WorldCoords::CHUNK_SIZE) return candidate;

            if (spawn_info->placement == ALLEY_INLINE_PLACEMENT) {
                if (!place_horizontal) {
                    const bool use_far_side = west_open && east_open ? rng.range(0, 1) == 1 : east_open;
                    candidate.rotation = use_far_side ? WorldCoords::ROT_WEST : WorldCoords::ROT_EAST;
                    candidate.placed_size = get_rotated_surface_feature_size(candidate.source_size, candidate.rotation);
                    if (candidate.placed_size.x > ALLEY_GARDEN_WIDTH) return candidate;
                    const int centered_offset = (ALLEY_GARDEN_WIDTH - candidate.placed_size.x) / 2;
                    anchor_x += use_far_side
                        ? WorldCoords::CHUNK_SIZE - ALLEY_GARDEN_START - ALLEY_GARDEN_WIDTH + centered_offset
                        : ALLEY_GARDEN_START + centered_offset;
                    anchor_y += rng.range(0, WorldCoords::CHUNK_SIZE - candidate.placed_size.y);
                } else {
                    const bool use_far_side = north_open && south_open ? rng.range(0, 1) == 1 : south_open;
                    candidate.rotation = use_far_side ? WorldCoords::ROT_NORTH : WorldCoords::ROT_SOUTH;
                    candidate.placed_size = get_rotated_surface_feature_size(candidate.source_size, candidate.rotation);
                    if (candidate.placed_size.y > ALLEY_GARDEN_WIDTH) return candidate;
                    const int centered_offset = (ALLEY_GARDEN_WIDTH - candidate.placed_size.y) / 2;
                    anchor_x += rng.range(0, WorldCoords::CHUNK_SIZE - candidate.placed_size.x);
                    anchor_y += use_far_side
                        ? WorldCoords::CHUNK_SIZE - ALLEY_GARDEN_START - ALLEY_GARDEN_WIDTH + centered_offset
                        : ALLEY_GARDEN_START + centered_offset;
                }
                candidate.require_source_chunk = true;
            } else {
                const int shoulder_nudge = rng.range(-1, 1);
                if (!place_horizontal) {
                    const bool use_far_side = west_open && east_open ? rng.range(0, 1) == 1 : east_open;
                    anchor_x += use_far_side
                        ? WorldCoords::CHUNK_SIZE - ROAD_SHOULDER_OFFSET - candidate.placed_size.x - shoulder_nudge
                        : ROAD_SHOULDER_OFFSET + shoulder_nudge;
                    anchor_y += rng.range(0, WorldCoords::CHUNK_SIZE - candidate.placed_size.y);
                } else {
                    const bool use_far_side = north_open && south_open ? rng.range(0, 1) == 1 : south_open;
                    anchor_x += rng.range(0, WorldCoords::CHUNK_SIZE - candidate.placed_size.x);
                    anchor_y += use_far_side
                        ? WorldCoords::CHUNK_SIZE - ROAD_SHOULDER_OFFSET - candidate.placed_size.y - shoulder_nudge
                        : ROAD_SHOULDER_OFFSET + shoulder_nudge;
                }
            }
        }

        candidate.origin = Vector2i(anchor_x, anchor_y);
        candidate.valid = true;
        return candidate;
    };

    auto candidate_has_priority = [](const SurfaceFeatureInstance& p_a, const SurfaceFeatureInstance& p_b) -> bool {
        if (p_a.chunk_y != p_b.chunk_y) return p_a.chunk_y < p_b.chunk_y;
        if (p_a.chunk_x != p_b.chunk_x) return p_a.chunk_x < p_b.chunk_x;
        return p_a.index < p_b.index;
    };

    auto is_blocked_by_prior_candidate = [&](const SurfaceFeatureInstance& p_candidate) -> bool {
        for (int cy = p_candidate.chunk_y - 1; cy <= p_candidate.chunk_y + 1; cy++) {
            for (int cx = p_candidate.chunk_x - 1; cx <= p_candidate.chunk_x + 1; cx++) {
                const int candidate_count = feature_candidate_count_for_chunk(cx, cy);
                for (int index = 0; index < candidate_count; index++) {
                    SurfaceFeatureInstance prior = build_candidate(cx, cy, index);
                    if (!prior.valid || !candidate_has_priority(prior, p_candidate)) continue;
                    if (!validate_surface_feature_anchor(prior.feature_id, prior.origin, prior.source_size, prior.placed_size, prior.rotation, world_seed, prior.require_source_chunk, prior.chunk_x, prior.chunk_y)) continue;
                    if (p_candidate.unique &&
                        prior.chunk_x == p_candidate.chunk_x &&
                        prior.chunk_y == p_candidate.chunk_y &&
                        prior.feature_id == p_candidate.feature_id) {
                        return true;
                    }
                    if (candidates_overlap(p_candidate, prior)) {
                        return true;
                    }
                }
            }
        }

        return false;
    };

    for (int oy = -1; oy <= 1; oy++) {
        for (int ox = -1; ox <= 1; ox++) {
            const int anchor_cx = current_cx + ox;
            const int anchor_cy = current_cy + oy;

            const int candidate_count = feature_candidate_count_for_chunk(anchor_cx, anchor_cy);
            for (int candidate = 0; candidate < candidate_count; candidate++) {
                SurfaceFeatureInstance placed = build_candidate(anchor_cx, anchor_cy, candidate);
                if (!placed.valid) continue;

                if (x >= placed.origin.x && x < placed.origin.x + placed.placed_size.x &&
                    y >= placed.origin.y && y < placed.origin.y + placed.placed_size.y) {
                    if (!validate_surface_feature_anchor(placed.feature_id, placed.origin, placed.source_size, placed.placed_size, placed.rotation, world_seed, placed.require_source_chunk, placed.chunk_x, placed.chunk_y)) {
                        continue;
                    }
                    if (is_blocked_by_prior_candidate(placed)) {
                        continue;
                    }

                    const int local_x = x - placed.origin.x;
                    const int local_y = y - placed.origin.y;
                    const uint32_t feature_hash = get_hash(x, y, static_cast<uint32_t>(world_seed));
                    const uint16_t tile_id = get_surface_feature_tile_at(placed.feature_id, local_x, local_y, placed.source_size, placed.rotation, feature_hash);
                    if (p_include_void_tiles || (tile_id != 0 && tile_id != id_void)) {
                        r_instance = placed;
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

uint16_t WorldGenerator::get_feature_tile(int x, int y, int z, int world_seed) {
    SurfaceFeatureInstance instance;
    if (!find_feature_at(x, y, z, world_seed, instance)) {
        return id_void;
    }

    const int local_x = x - instance.origin.x;
    const int local_y = y - instance.origin.y;
    const uint32_t feature_hash = get_hash(x, y, static_cast<uint32_t>(world_seed));
    return get_surface_feature_tile_at(instance.feature_id, local_x, local_y, instance.source_size, instance.rotation, feature_hash);
}

uint16_t WorldGenerator::get_dungeon_layout_tile(const DungeonLayout& p_layout, int x, int y, bool p_include_dynamic) const {
    if (!p_layout.might_contain(x, y)) {
        return id_void;
    }
    const uint16_t floor_tile = p_layout.floor_tile_id != 0 ? p_layout.floor_tile_id : id_stone_brick_floor;
    const uint16_t wall_tile = p_layout.wall_tile_id != 0 ? p_layout.wall_tile_id : id_stone_brick_wall;

    if (p_include_dynamic) {
        auto dynamic_it = p_layout.dynamic_tiles.find(dungeon_dynamic_cell_key(x, y));
        if (dynamic_it != p_layout.dynamic_tiles.end()) {
            return dynamic_it->second;
        }
    }

    for (int room_index = 0; room_index < (int)p_layout.rooms.size(); room_index++) {
        const PlacedDungeonRoom& room = p_layout.rooms[room_index];
        if (!DungeonGenerator::rect_has_point(room.bounds, x, y)) continue;

        const bool is_door = DungeonGenerator::room_boundary_has_point(room.bounds, x, y) && p_layout.has_door(x, y);
        if (is_door) {
            return id_dungeon_door;
        }
        if (!room.structure_id.is_empty()) {
            const int local_x = x - room.bounds.origin.x;
            const int local_y = y - room.bounds.origin.y;
            StructureDb* structure_db = s_db ? s_db : StructureDb::get_singleton();
            const uint16_t tile_id = structure_db ? structure_db->get_tile_at(room.structure_id, local_x, local_y, 0) : 0;
            return (tile_id != 0 && tile_id != id_void) ? tile_id : id_void;
        }
        if (DungeonGenerator::room_boundary_has_point(room.bounds, x, y)) {
            return wall_tile;
        }
        return floor_tile;
    }

    if (p_layout.has_corridor(x, y)) {
        return floor_tile;
    }
    if (p_layout.has_corridor_wall(x, y)) {
        return wall_tile;
    }
    return id_void;
}

namespace {

static constexpr uint64_t DUNGEON_DYNAMIC_FEATURE_SALT = 0x44594E44554E4654ULL; // "DYNDUNFT"
static constexpr uint64_t SPIDER_NEST_SALT = 0x535049444E455354ULL; // "SPIDNEST"
static constexpr int SPIDER_NEST_CENTER_RADIUS = 4;
static constexpr int SPIDER_NEST_EGG_RADIUS = 2;
static constexpr float SPIDER_NEST_CENTER_THICK_CHANCE = 0.66f;
static constexpr float SPIDER_NEST_CENTER_NORMAL_CHANCE = 0.2f;
static constexpr float SPIDER_NEST_OUTER_THICK_CHANCE = 0.25f;
static constexpr float SPIDER_NEST_OUTER_NORMAL_CHANCE = 0.25f;
static constexpr float SPIDER_NEST_CENTER_WALL_ERODE_CHANCE = 0.6f;

struct SpiderNestArea {
    std::vector<Vector2i> floor_cells;
    std::vector<Vector2i> wall_cells;
    std::vector<Vector2i> center_floor_cells;
};

struct DynamicDungeonFeatureArea {
    Vector2i center;
    int radius = 0;
};

struct DynamicDungeonFeatureCenter {
    Vector2i pos;
    int distance_from_entrance = 0;
};

static float distance_sq_float(int p_dx, int p_dy) {
    return static_cast<float>(p_dx * p_dx + p_dy * p_dy);
}

static int distance_sq_int(const Vector2i& p_a, const Vector2i& p_b) {
    const int dx = p_a.x - p_b.x;
    const int dy = p_a.y - p_b.y;
    return dx * dx + dy * dy;
}

static int max_int(int p_a, int p_b) {
    return p_a > p_b ? p_a : p_b;
}

static Vector2i dungeon_rect_center(const DungeonRect& p_rect) {
    return Vector2i(
        p_rect.origin.x + p_rect.size.x / 2,
        p_rect.origin.y + p_rect.size.y / 2
    );
}

static uint64_t dynamic_feature_instance_salt(int p_feature_index, int p_instance_index) {
    return DUNGEON_DYNAMIC_FEATURE_SALT ^
        (static_cast<uint64_t>(p_feature_index + 1) * 0x9E3779B97F4A7C15ULL) ^
        (static_cast<uint64_t>(p_instance_index + 1) * 0xBF58476D1CE4E5B9ULL);
}

static bool dynamic_area_overlaps(
    const std::vector<DynamicDungeonFeatureArea>& p_placed_areas,
    const Vector2i& p_center,
    int p_radius
) {
    for (const DynamicDungeonFeatureArea& placed : p_placed_areas) {
        const int min_distance = p_radius + placed.radius + 2;
        if (distance_sq_int(p_center, placed.center) <= min_distance * min_distance) {
            return true;
        }
    }
    return false;
}

static bool cave_center_is_reserved_by_room(const DungeonLayout& p_layout, const Vector2i& p_center, int p_radius) {
    for (const PlacedDungeonRoom& room : p_layout.rooms) {
        const Vector2i center = dungeon_rect_center(room.bounds);
        const int room_radius = max_int(room.bounds.size.x, room.bounds.size.y) / 2;
        const int min_distance = p_radius + room_radius + 2;
        if (distance_sq_int(p_center, center) <= min_distance * min_distance) {
            return true;
        }
    }
    return false;
}

}

void WorldGenerator::try_place_dynamic_dungeon_features(
    DungeonLayout& r_layout,
    const DungeonInfo& p_info,
    int p_world_seed
) {
    if (p_info.dynamic_features.empty()) {
        return;
    }

    std::vector<DynamicDungeonFeatureArea> placed_areas;
    const Vector2i entrance_origin(
        r_layout.entrance_chunk.x * WorldCoords::CHUNK_SIZE,
        r_layout.entrance_chunk.y * WorldCoords::CHUNK_SIZE
    );
    const Vector2i entrance_center = entrance_origin + Vector2i(WorldCoords::CHUNK_SIZE / 2, WorldCoords::CHUNK_SIZE / 2);

    auto add_spider_nest = [&](const DungeonDynamicFeatureInfo& p_feature, const Vector2i& p_center, int p_feature_index, int p_instance_index) -> bool {
        if (dynamic_area_overlaps(placed_areas, p_center, p_feature.radius_max)) {
            return false;
        }

        int placed_radius = 0;
        if (!try_place_spider_nest(
                r_layout,
                p_world_seed,
                p_center,
                p_feature.radius_min,
                p_feature.radius_max,
                p_feature.egg_count_min,
                p_feature.egg_count_max,
                dynamic_feature_instance_salt(p_feature_index, p_instance_index),
                placed_radius
            )) {
            return false;
        }

        placed_areas.push_back(DynamicDungeonFeatureArea{p_center, placed_radius});
        return true;
    };

    for (int feature_index = 0; feature_index < static_cast<int>(p_info.dynamic_features.size()); feature_index++) {
        const DungeonDynamicFeatureInfo& feature = p_info.dynamic_features[feature_index];
        if (feature.type != "spider_nest") {
            continue;
        }

        Rng::Seeded rng = Rng::at(
            static_cast<uint32_t>(p_world_seed),
            r_layout.entrance_chunk,
            Rng::BIOME,
            dynamic_feature_instance_salt(feature_index, 0)
        );

        if (feature.placement == "room_random") {
            for (int room_index = 0; room_index < static_cast<int>(r_layout.rooms.size()); room_index++) {
                if (!rng.chance(feature.chance)) continue;

                const PlacedDungeonRoom& room = r_layout.rooms[room_index];
                std::vector<Vector2i> candidate_centers;
                candidate_centers.reserve(room.bounds.size.x * room.bounds.size.y);
                for (int y = room.bounds.origin.y; y < room.bounds.origin.y + room.bounds.size.y; y++) {
                    for (int x = room.bounds.origin.x; x < room.bounds.origin.x + room.bounds.size.x; x++) {
                        const uint16_t floor_tile = r_layout.floor_tile_id != 0 ? r_layout.floor_tile_id : id_stone_brick_floor;
                        if (get_dungeon_layout_tile(r_layout, x, y, false) == floor_tile) {
                            candidate_centers.push_back(Vector2i(x, y));
                        }
                    }
                }
                if (candidate_centers.empty()) {
                    continue;
                }

                const Vector2i center = candidate_centers[rng.range(0, static_cast<int>(candidate_centers.size()) - 1)];
                add_spider_nest(feature, center, feature_index, room_index + 1);
            }
            continue;
        }

        const int target_count = rng.range(feature.count_min, feature.count_max);
        if (target_count <= 0 || !rng.chance(feature.chance)) {
            continue;
        }

        if (feature.placement == "cave_scattered") {
            std::vector<Vector2i> candidate_centers;
            candidate_centers.reserve(r_layout.cave_chamber_centers.size());
            const uint16_t floor_tile = r_layout.floor_tile_id != 0 ? r_layout.floor_tile_id : id_stone_brick_floor;
            for (int i = 0; i < static_cast<int>(r_layout.cave_chamber_centers.size()); i++) {
                if (i == 0) continue;

                const Vector2i center = r_layout.cave_chamber_centers[i];
                if (cave_center_is_reserved_by_room(r_layout, center, feature.radius_max)) {
                    continue;
                }
                if (get_dungeon_layout_tile(r_layout, center.x, center.y, false) != floor_tile) {
                    continue;
                }
                candidate_centers.push_back(center);
            }

            int placed_count = 0;
            int instance_index = 0;
            while (placed_count < target_count && !candidate_centers.empty()) {
                const int picked_index = rng.range(0, static_cast<int>(candidate_centers.size()) - 1);
                const Vector2i center = candidate_centers[picked_index];
                candidate_centers.erase(candidate_centers.begin() + picked_index);

                if (add_spider_nest(feature, center, feature_index, ++instance_index)) {
                    placed_count++;
                }
            }
            continue;
        }

        if (feature.placement == "end") {
            std::vector<DynamicDungeonFeatureCenter> candidate_centers;
            candidate_centers.reserve(r_layout.rooms.size());
            for (const PlacedDungeonRoom& room : r_layout.rooms) {
                const Vector2i center = dungeon_rect_center(room.bounds);
                candidate_centers.push_back(DynamicDungeonFeatureCenter{center, distance_sq_int(center, entrance_center)});
            }

            std::sort(candidate_centers.begin(), candidate_centers.end(), [](const DynamicDungeonFeatureCenter& p_a, const DynamicDungeonFeatureCenter& p_b) {
                return p_a.distance_from_entrance > p_b.distance_from_entrance;
            });

            int placed_count = 0;
            for (int i = 0; i < static_cast<int>(candidate_centers.size()) && placed_count < target_count; i++) {
                if (add_spider_nest(feature, candidate_centers[i].pos, feature_index, i + 1)) {
                    placed_count++;
                }
            }
        }
    }
}

bool WorldGenerator::try_place_spider_nest(
    DungeonLayout& r_layout,
    int p_world_seed,
    const Vector2i& p_center,
    int p_radius_min,
    int p_radius_max,
    int p_egg_count_min,
    int p_egg_count_max,
    uint64_t p_salt,
    int& r_placed_radius
) {
    Rng::Seeded rng = Rng::at(
        static_cast<uint32_t>(p_world_seed),
        r_layout.entrance_chunk,
        Rng::BIOME,
        SPIDER_NEST_SALT ^ p_salt
    );

    const uint16_t floor_tile = r_layout.floor_tile_id != 0 ? r_layout.floor_tile_id : id_stone_brick_floor;
    const uint16_t wall_tile = r_layout.wall_tile_id != 0 ? r_layout.wall_tile_id : id_stone_brick_wall;
    const int radius = rng.range(p_radius_min, p_radius_max);
    r_placed_radius = radius;
    SpiderNestArea area;

    for (int y = p_center.y - radius - 1; y <= p_center.y + radius + 1; y++) {
        for (int x = p_center.x - radius - 1; x <= p_center.x + radius + 1; x++) {
            const int dx = x - p_center.x;
            const int dy = y - p_center.y;
            const float noise = rng.unit() * 1.6f - 0.8f;
            const float rough_radius = static_cast<float>(radius) + noise;
            if (distance_sq_float(dx, dy) > rough_radius * rough_radius) {
                continue;
            }

            const uint16_t tile_id = get_dungeon_layout_tile(r_layout, x, y, false);
            if (tile_id == floor_tile) {
                Vector2i cell(x, y);
                area.floor_cells.push_back(cell);
                if (dx * dx + dy * dy <= SPIDER_NEST_EGG_RADIUS * SPIDER_NEST_EGG_RADIUS) {
                    area.center_floor_cells.push_back(cell);
                }
            } else if (tile_id == wall_tile) {
                const bool near_center = dx * dx + dy * dy <= SPIDER_NEST_CENTER_RADIUS * SPIDER_NEST_CENTER_RADIUS;
                if (near_center && rng.chance(SPIDER_NEST_CENTER_WALL_ERODE_CHANCE)) {
                    Vector2i cell(x, y);
                    area.floor_cells.push_back(cell);
                    if (dx * dx + dy * dy <= SPIDER_NEST_EGG_RADIUS * SPIDER_NEST_EGG_RADIUS) {
                        area.center_floor_cells.push_back(cell);
                    }
                } else {
                    area.wall_cells.push_back(Vector2i(x, y));
                }
            }
        }
    }
    if (area.floor_cells.empty() && area.wall_cells.empty()) {
        return false;
    }

    auto pick_web_tile = [&](uint16_t p_normal_tile_id, uint16_t p_thick_tile_id, bool p_near_center) -> uint16_t {
        const float roll = rng.unit();
        const float thick_chance = p_near_center ? SPIDER_NEST_CENTER_THICK_CHANCE : SPIDER_NEST_OUTER_THICK_CHANCE;
        const float normal_chance = p_near_center ? SPIDER_NEST_CENTER_NORMAL_CHANCE : SPIDER_NEST_OUTER_NORMAL_CHANCE;
        if (roll < thick_chance) {
            return p_thick_tile_id;
        }
        if (roll < thick_chance + normal_chance) {
            return p_normal_tile_id;
        }
        return 0;
    };

    for (const Vector2i& cell : area.wall_cells) {
        const int dx = cell.x - p_center.x;
        const int dy = cell.y - p_center.y;
        const bool near_center = dx * dx + dy * dy <= SPIDER_NEST_CENTER_RADIUS * SPIDER_NEST_CENTER_RADIUS;
        const uint16_t web_tile_id = pick_web_tile(id_stone_brick_wall_web, id_stone_brick_wall_web_thick, near_center);
        if (web_tile_id != 0) {
            r_layout.dynamic_tiles[dungeon_dynamic_cell_key(cell.x, cell.y)] = web_tile_id;
        }
    }

    for (const Vector2i& cell : area.floor_cells) {
        const int dx = cell.x - p_center.x;
        const int dy = cell.y - p_center.y;
        const bool near_center = dx * dx + dy * dy <= SPIDER_NEST_CENTER_RADIUS * SPIDER_NEST_CENTER_RADIUS;
        const uint16_t web_tile_id = pick_web_tile(id_stone_brick_floor_web, id_stone_brick_floor_web_thick, near_center);
        if (web_tile_id != 0) {
            r_layout.dynamic_tiles[dungeon_dynamic_cell_key(cell.x, cell.y)] = web_tile_id;
        }
    }

    std::vector<Vector2i> egg_candidates = area.center_floor_cells.empty() ? area.floor_cells : area.center_floor_cells;
    const int egg_count = std::min(rng.range(p_egg_count_min, p_egg_count_max), static_cast<int>(egg_candidates.size()));
    for (int i = 0; i < egg_count; i++) {
        const int picked_index = rng.range(0, static_cast<int>(egg_candidates.size()) - 1);
        const Vector2i picked = egg_candidates[picked_index];
        r_layout.dynamic_tiles[dungeon_dynamic_cell_key(picked.x, picked.y)] = id_spider_eggs;
        egg_candidates.erase(egg_candidates.begin() + picked_index);
        if (egg_candidates.empty()) break;
    }

    return true;
}

DungeonLayout* WorldGenerator::get_or_create_dungeon_layout(
    const String& p_dungeon_type,
    const Vector2i& p_entrance_chunk,
    int p_world_seed
) {
    if (!dungeon_layout_cache_seed_valid || dungeon_layout_cache_seed != p_world_seed) {
        dungeon_layout_cache.clear();
        dungeon_layout_cache_seed = p_world_seed;
        dungeon_layout_cache_seed_valid = true;
    }

    const uint64_t key = WorldCoords::pack_coords(p_entrance_chunk.x, p_entrance_chunk.y);
    auto it = dungeon_layout_cache.find(key);
    if (it != dungeon_layout_cache.end()) {
        return &it->second;
    }

    DungeonDb* dungeon_db = DungeonDb::get_singleton();
    const DungeonInfo* info = dungeon_db ? dungeon_db->get_dungeon_info(p_dungeon_type) : nullptr;
    if (!info) {
        return nullptr;
    }

    DungeonLayout layout;
    if (info->generator == "room_graph") {
        layout = DungeonGenerator::build_layout(*info, p_entrance_chunk, p_world_seed);
    } else if (info->generator == "cave_graph") {
        layout = CaveGenerator::build_layout(*info, p_entrance_chunk, p_world_seed);
    } else {
        return nullptr;
    }
    try_place_dynamic_dungeon_features(layout, *info, p_world_seed);
    stamp_dungeon_layout_biomes(layout);
    auto inserted = dungeon_layout_cache.emplace(key, layout);
    return &inserted.first->second;
}

uint16_t WorldGenerator::get_dungeon_tile(int x, int y, int z, int world_seed) {
    if (!dungeon_layout_cache_seed_valid || dungeon_layout_cache_seed != world_seed) {
        dungeon_layout_cache.clear();
        dungeon_layout_cache_seed = world_seed;
        dungeon_layout_cache_seed_valid = true;
    }

    auto tile_from_layout = [&](const DungeonLayout& p_layout) -> uint16_t {
        if (p_layout.z != z || !p_layout.might_contain(x, y)) {
            return id_void;
        }
        return get_dungeon_layout_tile(p_layout, x, y, true);
    };

    for (const auto& pair : dungeon_layout_cache) {
        uint16_t tile_id = tile_from_layout(pair.second);
        if (tile_id != id_void) {
            return tile_id;
        }
    }

    if (!dungeon_entrance_cache_valid) {
        rebuild_dungeon_entrance_cache();
    }

    for (const DungeonEntranceRef& entrance : dungeon_entrance_cache) {
        if (entrance.start_z != z) continue;

        DungeonLayout* layout = get_or_create_dungeon_layout(entrance.dungeon_type, entrance.entrance_chunk, world_seed);
        if (!layout) continue;

        uint16_t tile_id = tile_from_layout(*layout);
        if (tile_id != id_void) {
            return tile_id;
        }
    }

    return id_void;
}

DungeonStructureContext WorldGenerator::get_dungeon_structure_context(int x, int y, int z, int world_seed) {
    setup_biome_rules();

    if (!dungeon_layout_cache_seed_valid || dungeon_layout_cache_seed != world_seed) {
        dungeon_layout_cache.clear();
        dungeon_layout_cache_seed = world_seed;
        dungeon_layout_cache_seed_valid = true;
    }

    auto context_from_layout = [&](const DungeonLayout& p_layout) -> DungeonStructureContext {
        DungeonStructureContext context;
        if (p_layout.z != z || !p_layout.might_contain(x, y)) {
            return context;
        }

        for (const PlacedDungeonRoom& room : p_layout.rooms) {
            if (room.structure_id.is_empty()) continue;
            if (!DungeonGenerator::rect_has_point(room.bounds, x, y)) continue;

            context.valid = true;
            context.structure_id = room.structure_id;
            context.local_pos = Vector2i(x - room.bounds.origin.x, y - room.bounds.origin.y);
            context.local_z = 0;
            return context;
        }

        return context;
    };

    for (const auto& pair : dungeon_layout_cache) {
        DungeonStructureContext context = context_from_layout(pair.second);
        if (context.valid) {
            return context;
        }
    }

    if (!dungeon_entrance_cache_valid) {
        rebuild_dungeon_entrance_cache();
    }

    for (const DungeonEntranceRef& entrance : dungeon_entrance_cache) {
        if (entrance.start_z != z) continue;

        DungeonLayout* layout = get_or_create_dungeon_layout(entrance.dungeon_type, entrance.entrance_chunk, world_seed);
        if (!layout) continue;

        DungeonStructureContext context = context_from_layout(*layout);
        if (context.valid) {
            return context;
        }
    }

    return DungeonStructureContext{};
}

SurfaceFeatureContext WorldGenerator::get_surface_feature_context(int x, int y, int z, int world_seed) {
    setup_biome_rules();

    SurfaceFeatureInstance instance;
    if (!find_feature_at(x, y, z, world_seed, instance, true)) {
        return SurfaceFeatureContext{};
    }

    const int local_x = x - instance.origin.x;
    const int local_y = y - instance.origin.y;
    const Vector2i source_pos = resolve_surface_feature_source_pos(local_x, local_y, instance.source_size, instance.rotation);

    SurfaceFeatureContext context;
    context.valid = true;
    context.structure_id = instance.feature_id;
    context.local_pos = source_pos;
    context.local_z = 0;
    return context;
}

CityStructureContext WorldGenerator::get_city_structure_context(int x, int y, int z) const {
    const uint64_t chunk_key = WorldCoords::pack_coords(floor_div_chunk(x), floor_div_chunk(y));
    auto index_it = city_structure_by_chunk.find(chunk_key);
    if (index_it == city_structure_by_chunk.end() || index_it->second >= city_structure_instances.size()) {
        return CityStructureContext{};
    }

    const CityStructureInstance& instance = city_structure_instances[index_it->second];
    const StructureInfo* structure = s_db ? s_db->get_structure_info(instance.structure_id) : nullptr;
    if (!structure || structure->levels.find(z) == structure->levels.end()) {
        return CityStructureContext{};
    }
    const int local_x = x - instance.origin.x;
    const int local_y = y - instance.origin.y;
    CityStructureContext context;
    context.valid = true;
    context.structure_id = instance.structure_id;
    context.local_z = z;
    context.local_pos = Vector2i(-1, -1);
    if (local_x < 0 || local_y < 0 || local_x >= instance.placed_size.x || local_y >= instance.placed_size.y) {
        return context;
    }

    const Vector2i source_pos = resolve_surface_feature_source_pos(local_x, local_y, instance.source_size, instance.rotation);
    if (source_pos.x < 0 || source_pos.y < 0 || source_pos.x >= instance.source_size.x || source_pos.y >= instance.source_size.y) {
        return context;
    }

    context.local_pos = source_pos;
    return context;
}

String WorldGenerator::get_dungeon_type_for_cell(int x, int y, int z, int world_seed) {
    setup_biome_rules();

    if (!dungeon_layout_cache_seed_valid || dungeon_layout_cache_seed != world_seed) {
        dungeon_layout_cache.clear();
        dungeon_layout_cache_seed = world_seed;
        dungeon_layout_cache_seed_valid = true;
    }

    auto type_from_layout = [&](const DungeonLayout& p_layout) -> String {
        if (p_layout.z != z || !p_layout.might_contain(x, y)) {
            return "";
        }
        return get_dungeon_layout_tile(p_layout, x, y, true) != id_void ? p_layout.dungeon_type : String();
    };

    for (const auto& pair : dungeon_layout_cache) {
        String dungeon_type = type_from_layout(pair.second);
        if (!dungeon_type.is_empty()) {
            return dungeon_type;
        }
    }

    if (!dungeon_entrance_cache_valid) {
        rebuild_dungeon_entrance_cache();
    }

    for (const DungeonEntranceRef& entrance : dungeon_entrance_cache) {
        if (entrance.start_z != z) continue;

        DungeonLayout* layout = get_or_create_dungeon_layout(entrance.dungeon_type, entrance.entrance_chunk, world_seed);
        if (layout) {
            String dungeon_type = type_from_layout(*layout);
            if (!dungeon_type.is_empty()) {
                return dungeon_type;
            }
        }
    }

    return "";
}

bool WorldGenerator::is_stone_brick_floor_loot_candidate(int x, int y, int z, int world_seed) {
    setup_biome_rules();
    return get_dungeon_tile(x, y, z, world_seed) == id_stone_brick_floor;
}

uint16_t WorldGenerator::get_tile(int x, int y, int world_seed) {
    uint16_t base_tile_id = get_base_surface_tile(x, y, world_seed);
    uint16_t feature_tile_id = get_feature_tile(x, y, 0, world_seed);
    return feature_tile_id != id_void ? feature_tile_id : base_tile_id;
}

uint16_t WorldGenerator::get_tile(int x, int y, int z, int world_seed) {
    if (z == 0) return get_tile(x, y, world_seed);

    CityStructureContext city_structure = get_city_structure_context(x, y, z);
    if (city_structure.valid && s_db) {
        const uint16_t structure_tile = s_db->get_tile_at(
            city_structure.structure_id,
            city_structure.local_pos.x,
            city_structure.local_pos.y,
            z,
            get_hash(x, y, static_cast<uint32_t>(world_seed))
        );
        if (structure_tile != id_void) return structure_tile;
        return get_base_tile_without_features(x, y, z, world_seed);
    }

    uint16_t base_tile_id = get_base_tile_without_features(x, y, z, world_seed);
    uint16_t feature_tile_id = get_feature_tile(x, y, z, world_seed);
    return feature_tile_id != id_void ? feature_tile_id : base_tile_id;
}

uint16_t WorldGenerator::get_base_tile_without_features(int x, int y, int z, int world_seed) {
    if (z == 0) return get_base_surface_tile(x, y, world_seed);

    uint16_t chunk_id = get_chunk_id_for_cell(x, y);
    ChunkDb* chunk_db = ChunkDb::get_singleton();
    const ChunkInfo* chunk_info = chunk_db ? chunk_db->get_chunk_info(chunk_id) : nullptr;
    if (chunk_info && !chunk_info->structure_type.is_empty() && s_db) {
        int cx = (x >= 0) ? (x / WorldCoords::CHUNK_SIZE) : ((x - (WorldCoords::CHUNK_SIZE - 1)) / WorldCoords::CHUNK_SIZE);
        int cy = (y >= 0) ? (y / WorldCoords::CHUNK_SIZE) : ((y - (WorldCoords::CHUNK_SIZE - 1)) / WorldCoords::CHUNK_SIZE);
        uint64_t chunk_key = WorldCoords::pack_coords(cx, cy);
        const BiomeLayer* surface_layer = get_biome_layer(0);
        if (surface_layer) {
            auto it = surface_layer->overrides.find(chunk_key);
            if (it == surface_layer->overrides.end()) return id_void;
            uint8_t rotation = static_cast<uint8_t>(it->second >> WorldCoords::ORIENTATION_SHIFT);
            int lx = x % WorldCoords::CHUNK_SIZE; if (lx < 0) lx += WorldCoords::CHUNK_SIZE;
            int ly = y % WorldCoords::CHUNK_SIZE; if (ly < 0) ly += WorldCoords::CHUNK_SIZE;
            int rx = lx, ry = ly;
            int max_coord = WorldCoords::CHUNK_SIZE - 1;
            switch (rotation) {
                case WorldCoords::ROT_WEST: rx = ly; ry = max_coord - lx; break;
                case WorldCoords::ROT_NORTH: rx = max_coord - lx; ry = max_coord - ly; break;
                case WorldCoords::ROT_EAST: rx = max_coord - ly; ry = lx; break;
            }

            String structure_id = get_structure_id_for_cell(x, y, z, world_seed);
            uint32_t structure_hash = get_hash(x, y, static_cast<uint32_t>(world_seed));
            uint16_t tile_id = s_db->get_tile_at(structure_id, rx, ry, z, structure_hash);
            if (tile_id != id_void) return tile_id;
        }
    }

    uint16_t dungeon_tile_id = get_dungeon_tile(x, y, z, world_seed);
    if (dungeon_tile_id != id_void) return dungeon_tile_id;

    if (z == -1) return id_underground_earth;
    if (z < -1) return id_solid_rock;
    return id_air;
}

const std::unordered_map<uint64_t, uint32_t>& WorldGenerator::get_region_chunks() const {
    static const std::unordered_map<uint64_t, uint32_t> empty;
    const BiomeLayer* layer = get_biome_layer(0);
    return layer ? layer->overrides : empty;
}

void WorldGenerator::set_region_chunks(const std::unordered_map<uint64_t, uint32_t>& chunks) {
    setup_biome_rules();
    biome_layers.clear();
    BiomeLayer layer;
    layer.z = 0;
    layer.default_chunk_data = get_default_biome_chunk_data(0);
    layer.overrides = chunks;
    biome_layers[0] = std::move(layer);
    city_structure_instances.clear();
    city_structure_by_chunk.clear();
    last_chunk_valid = false;
    reset_dungeon_cache();
}

void WorldGenerator::set_biome_layers(const std::unordered_map<int, BiomeLayer>& layers) {
    setup_biome_rules();
    biome_layers = layers;
    city_structure_instances.clear();
    city_structure_by_chunk.clear();
    last_chunk_valid = false;
    reset_dungeon_cache();
}

Dictionary WorldGenerator::serialize_city_structures() const {
    Array instances;
    for (const CityStructureInstance& instance : city_structure_instances) {
        Dictionary data;
        data["id"] = instance.structure_id;
        data["origin"] = Array::make(instance.origin.x, instance.origin.y);
        data["source_size"] = Array::make(instance.source_size.x, instance.source_size.y);
        data["placed_size"] = Array::make(instance.placed_size.x, instance.placed_size.y);
        data["rotation"] = instance.rotation;
        instances.push_back(data);
    }
    Dictionary result;
    result["instances"] = instances;
    return result;
}

void WorldGenerator::deserialize_city_structures(const Array& p_data) {
    city_structure_instances.clear();
    city_structure_by_chunk.clear();
    for (int i = 0; i < p_data.size(); i++) {
        if (p_data[i].get_type() != Variant::DICTIONARY) continue;
        Dictionary data = p_data[i];
        CityStructureInstance instance;
        instance.structure_id = String(data.get("id", ""));
        instance.origin = variant_to_vector2i(data.get("origin", Array()), Vector2i());
        instance.source_size = variant_to_vector2i(data.get("source_size", Array()), Vector2i());
        instance.placed_size = variant_to_vector2i(data.get("placed_size", Array()), Vector2i());
        instance.rotation = static_cast<uint8_t>(static_cast<int>(data.get("rotation", 0)) & WorldCoords::ROTATION_MASK);
        if (instance.structure_id.is_empty() || instance.source_size.x <= 0 || instance.source_size.y <= 0 || instance.placed_size.x <= 0 || instance.placed_size.y <= 0) continue;

        const size_t index = city_structure_instances.size();
        city_structure_instances.push_back(instance);
        const int min_chunk_x = floor_div_chunk(instance.origin.x);
        const int min_chunk_y = floor_div_chunk(instance.origin.y);
        const int max_chunk_x = floor_div_chunk(instance.origin.x + instance.placed_size.x - 1);
        const int max_chunk_y = floor_div_chunk(instance.origin.y + instance.placed_size.y - 1);
        for (int chunk_y = min_chunk_y; chunk_y <= max_chunk_y; chunk_y++) {
            for (int chunk_x = min_chunk_x; chunk_x <= max_chunk_x; chunk_x++) {
                city_structure_by_chunk[WorldCoords::pack_coords(chunk_x, chunk_y)] = index;
            }
        }
    }
}

void WorldGenerator::clear_region_chunks() {
    biome_layers.clear();
    city_structure_instances.clear();
    city_structure_by_chunk.clear();
    last_chunk_valid = false;
    reset_dungeon_cache();
}
