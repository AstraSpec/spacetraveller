#include "world_generator.h"
#include "data/structure_db.h"
#include "core/id_registry.h"
#include "data/chunk_db.h"
#include "data/dungeon_db.h"
#include "data/feature_db.h"
#include "data/tile_group_db.h"
#include "core/tag_registry.h"
#include "city_generation.h"
#include "dungeon_generator.h"
#include "gen_grid.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <cmath>

using namespace godot;

static int floor_div_chunk(int p_value) {
    return (p_value >= 0)
        ? (p_value / WorldCoords::CHUNK_SIZE)
        : ((p_value - (WorldCoords::CHUNK_SIZE - 1)) / WorldCoords::CHUNK_SIZE);
}

WorldGenerator::WorldGenerator() {}
WorldGenerator::~WorldGenerator() = default;

static uint64_t dungeon_dynamic_cell_key(int p_x, int p_y) {
    return WorldCoords::pack_coords(p_x, p_y);
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
    id_forest = id_reg->register_string("forest");
    id_plains = id_reg->register_string("plains");
    id_underground_earth = id_reg->register_string("underground_earth");
    id_solid_rock = id_reg->register_string("solid_rock");
    id_road_bricks = id_reg->register_string("road_bricks");
    id_road_flagstone = id_reg->register_string("road_flagstone");
    id_alley_bricks = id_reg->register_string("alley_bricks");
    id_alley_flagstone = id_reg->register_string("alley_flagstone");
    id_crypt_entrance = id_reg->register_string("crypt_entrance");
    id_dungeon = id_reg->register_string("dungeon");
    id_dungeon_floor = id_reg->register_string("dungeon_floor");
    id_dungeon_wall = id_reg->register_string("dungeon_wall");
    id_dungeon_door = id_reg->register_string("w_door_c");
    id_spider_eggs = id_reg->register_string("spider_eggs");
    id_dungeon_wall_web = id_reg->register_string("dungeon_wall_web");
    id_dungeon_wall_web_thick = id_reg->register_string("dungeon_wall_web_thick");
    id_dungeon_floor_web = id_reg->register_string("dungeon_floor_web");
    id_dungeon_floor_web_thick = id_reg->register_string("dungeon_floor_web_thick");

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

    GenGrid cityGenGrid(WorldCoords::REGION_SIZE);
    CityGeneration::spawn_city(cityGenGrid, 127, 128, world_seed);

    Dictionary result;
    for (int y = 0; y < WorldCoords::REGION_SIZE; y++) {
        for (int x = 0; x < WorldCoords::REGION_SIZE; x++) {
            CityPixel pixel = cityGenGrid.getPixel(x, y);
            uint16_t chunk_id = pixel.id;
            
            if (chunk_id == id_void) {
                int gx = regionPos.x * WorldCoords::REGION_SIZE + x;
                int gy = regionPos.y * WorldCoords::REGION_SIZE + y;
                uint32_t h = get_hash(gx, gy, static_cast<uint32_t>(world_seed));

                ChunkDb* chunk_db = ChunkDb::get_singleton();
                if (chunk_db) {
                    for (const CityChunkSpawnInfo& spawn_info : chunk_db->get_wilderness_spawn_chunks()) {
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
                if (chunk_db && chunk_db->get_city_spawn_total_weight() > 0) {
                    int gx = regionPos.x * WorldCoords::REGION_SIZE + x;
                    int gy = regionPos.y * WorldCoords::REGION_SIZE + y;
                    uint64_t h = Rng::hash_pos(static_cast<uint32_t>(world_seed), Vector2i(gx, gy), Rng::BIOME);
                    int roll = static_cast<int>(h % static_cast<uint64_t>(chunk_db->get_city_spawn_total_weight()));
                    int cumulative = 0;
                    for (const CityChunkSpawnInfo& spawn_info : chunk_db->get_city_spawn_chunks()) {
                        cumulative += spawn_info.weight;
                        if (roll < cumulative) {
                            chunk_id = spawn_info.id;
                            break;
                        }
                    }
                }
            }

            uint8_t rot = pixel.meta & WorldCoords::ROTATION_MASK;
            int gx = regionPos.x * WorldCoords::REGION_SIZE + x;
            int gy = regionPos.y * WorldCoords::REGION_SIZE + y;

            // Temporary dungeon testing hook: place a crypt entrance just east of the default player spawn.
            if (gx == 121 && gy == 120 && id_crypt_entrance != 0) {
                chunk_id = id_crypt_entrance;
                rot = WorldCoords::ROT_SOUTH;
            }

            uint64_t key = WorldCoords::pack_coords(gx, gy);

            surface_chunks[key] = (static_cast<uint32_t>(rot) << WorldCoords::ORIENTATION_SHIFT) | chunk_id;
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
        bool current_is_road = (chunk_db && road_tag_id != 0) ? chunk_db->has_tag(chunk_id, road_tag_id) : false;

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
                if (current_is_road && chunk_db && road_tag_id != 0) {
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

    if (last_biome_ptr && last_biome_ptr->auto_tiled) {
        int lx = x % WorldCoords::CHUNK_SIZE; if (lx < 0) lx += WorldCoords::CHUNK_SIZE;
        int ly = y % WorldCoords::CHUNK_SIZE; if (ly < 0) ly += WorldCoords::CHUNK_SIZE;
        bool west = lx < 3;
        bool east = lx >= WorldCoords::CHUNK_SIZE - 3;
        bool north = ly < 3;
        bool south = ly >= WorldCoords::CHUNK_SIZE - 3;
        if ((west || east) && (north || south)) return last_biome_ptr->border_tile_id;
        if ((west && !(last_chunk_neighbors & WorldCoords::NEIGH_WEST)) ||
            (east && !(last_chunk_neighbors & WorldCoords::NEIGH_EAST)) ||
            (north && !(last_chunk_neighbors & WorldCoords::NEIGH_NORTH)) ||
            (south && !(last_chunk_neighbors & WorldCoords::NEIGH_SOUTH))) {
            return last_biome_ptr->border_tile_id;
        }
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
        uint16_t tile_id = s_db->get_tile_at(structure_id, rx, ry, 0);
        if (tile_id != id_void) return tile_id;
    }

    if (last_biome_ptr) {
        uint32_t h = get_hash(x, y, static_cast<uint32_t>(world_seed));
        return pick_weighted_tile(*last_biome_ptr, h);
    }

    return id_void;
}

bool WorldGenerator::base_allows_surface_feature(uint16_t p_base_tile_id) const {
    return p_base_tile_id == id_road_bricks ||
        p_base_tile_id == id_road_flagstone ||
        p_base_tile_id == id_alley_bricks ||
        p_base_tile_id == id_alley_flagstone;
}

uint16_t WorldGenerator::get_surface_feature_tile_at(
    const String& p_feature_id,
    int p_local_x,
    int p_local_y,
    const Vector2i& p_source_size,
    uint8_t p_rotation
) const {
    int sx = p_local_x;
    int sy = p_local_y;

    switch (p_rotation) {
        case WorldCoords::ROT_EAST:
            sx = p_local_y;
            sy = p_source_size.y - 1 - p_local_x;
            break;
        case WorldCoords::ROT_WEST:
            sx = p_source_size.x - 1 - p_local_y;
            sy = p_local_x;
            break;
        case WorldCoords::ROT_NORTH:
            sx = p_source_size.x - 1 - p_local_x;
            sy = p_source_size.y - 1 - p_local_y;
            break;
        default:
            break;
    }

    return s_db ? s_db->get_tile_at(p_feature_id, sx, sy, 0) : id_void;
}

bool WorldGenerator::validate_surface_feature_anchor(
    const String& p_feature_id,
    const Vector2i& p_origin,
    const Vector2i& p_source_size,
    const Vector2i& p_placed_size,
    uint8_t p_rotation,
    int p_world_seed
) {
    for (int ly = 0; ly < p_placed_size.y; ly++) {
        for (int lx = 0; lx < p_placed_size.x; lx++) {
            const uint16_t feature_tile = get_surface_feature_tile_at(p_feature_id, lx, ly, p_source_size, p_rotation);
            if (feature_tile == 0 || feature_tile == id_void) continue;

            const uint16_t base_tile = get_base_surface_tile(p_origin.x + lx, p_origin.y + ly, p_world_seed);
            if (!base_allows_surface_feature(base_tile)) {
                return false;
            }
        }
    }
    return true;
}

uint16_t WorldGenerator::get_surface_feature_tile(int x, int y, uint16_t base_tile_id, int world_seed) {
    return get_road_surface_feature_tile(x, y, base_tile_id, world_seed);
}

uint16_t WorldGenerator::get_road_surface_feature_tile(int x, int y, uint16_t base_tile_id, int world_seed) {
    static constexpr int ROAD_SHOULDER_OFFSET = 3;
    static constexpr uint64_t ROAD_FEATURE_SALT = 0x524F414446454154ULL; // "ROADFEAT"
    static const String ROADSIDE_PLACEMENT = "roadside";

    struct RoadFeatureCandidate {
        bool valid = false;
        int chunk_x = 0;
        int chunk_y = 0;
        int index = 0;
        String feature_id;
        Vector2i origin;
        Vector2i source_size;
        Vector2i placed_size;
        uint8_t rotation = WorldCoords::ROT_SOUTH;
    };

    if (!base_allows_surface_feature(base_tile_id)) return id_void;
    if (!s_db) return id_void;

    FeatureDb* feature_db = FeatureDb::get_singleton();
    if (!feature_db) return id_void;

    ChunkDb* chunk_db = ChunkDb::get_singleton();
    TagRegistry* tag_reg = TagRegistry::get_singleton();
    const uint16_t road_tag_id = tag_reg ? tag_reg->get_tag_id("ROAD") : 0;
    if (!chunk_db || road_tag_id == 0) return id_void;

    const int current_cx = floor_div_chunk(x);
    const int current_cy = floor_div_chunk(y);

    auto roadside_candidate_count_for_chunk = [&](int p_chunk_x, int p_chunk_y) -> int {
        const uint64_t chunk_key = WorldCoords::pack_coords(p_chunk_x, p_chunk_y);
        const BiomeLayer* surface_layer = get_biome_layer(0);
        if (!surface_layer) return 0;
        auto chunk_it = surface_layer->overrides.find(chunk_key);
        if (chunk_it == surface_layer->overrides.end()) return 0;

        const uint16_t chunk_id = static_cast<uint16_t>(chunk_it->second & WorldCoords::ID_MASK);
        if (!chunk_db->has_tag(chunk_id, road_tag_id)) return 0;

        const uint8_t neighbor_mask = static_cast<uint8_t>((chunk_it->second >> WorldCoords::NEIGHBOR_SHIFT) & WorldCoords::NEIGHBOR_MASK);
        const bool north_south = (neighbor_mask & WorldCoords::NEIGH_NORTH) || (neighbor_mask & WorldCoords::NEIGH_SOUTH);
        const bool east_west = (neighbor_mask & WorldCoords::NEIGH_EAST) || (neighbor_mask & WorldCoords::NEIGH_WEST);
        if (!north_south && !east_west) return 0;

        const ChunkInfo* chunk_info = chunk_db->get_chunk_info(chunk_id);
        if (!chunk_info) return 0;

        int count = 0;
        for (const ChunkFeatureSpawnInfo& spawn : chunk_info->feature_spawns) {
            if (spawn.placement != ROADSIDE_PLACEMENT) continue;
            count += spawn.candidates;
        }
        return count;
    };

    auto build_candidate = [&](int p_chunk_x, int p_chunk_y, int p_index) -> RoadFeatureCandidate {
        RoadFeatureCandidate candidate;
        candidate.chunk_x = p_chunk_x;
        candidate.chunk_y = p_chunk_y;
        candidate.index = p_index;

        const uint64_t chunk_key = WorldCoords::pack_coords(p_chunk_x, p_chunk_y);
        const BiomeLayer* surface_layer = get_biome_layer(0);
        if (!surface_layer) return candidate;
        auto chunk_it = surface_layer->overrides.find(chunk_key);
        if (chunk_it == surface_layer->overrides.end()) return candidate;

        const uint16_t chunk_id = static_cast<uint16_t>(chunk_it->second & WorldCoords::ID_MASK);
        if (!chunk_db->has_tag(chunk_id, road_tag_id)) return candidate;
        const ChunkInfo* chunk_info = chunk_db->get_chunk_info(chunk_id);
        if (!chunk_info) return candidate;

        const uint8_t neighbor_mask = static_cast<uint8_t>((chunk_it->second >> WorldCoords::NEIGHBOR_SHIFT) & WorldCoords::NEIGHBOR_MASK);
        const bool north_south = (neighbor_mask & WorldCoords::NEIGH_NORTH) || (neighbor_mask & WorldCoords::NEIGH_SOUTH);
        const bool east_west = (neighbor_mask & WorldCoords::NEIGH_EAST) || (neighbor_mask & WorldCoords::NEIGH_WEST);
        if (!north_south && !east_west) return candidate;

        const ChunkFeatureSpawnInfo* spawn_info = nullptr;
        int local_index = p_index;
        for (const ChunkFeatureSpawnInfo& spawn : chunk_info->feature_spawns) {
            if (spawn.placement != ROADSIDE_PLACEMENT) continue;
            if (local_index < spawn.candidates) {
                spawn_info = &spawn;
                break;
            }
            local_index -= spawn.candidates;
        }
        if (!spawn_info) return candidate;

        Rng::Seeded rng = Rng::at(
            static_cast<uint32_t>(world_seed),
            Vector2i(p_chunk_x, p_chunk_y),
            Rng::BIOME,
            ROAD_FEATURE_SALT + static_cast<uint64_t>(p_index) * 0x9E3779B97F4A7C15ULL
        );
        if (!rng.chance(spawn_info->chance)) return candidate;

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

        const FeaturePoolInfo* pool = feature_db->get_feature_pool(spawn_info->pool);
        if (!pool || pool->type != "structure_stamp") return candidate;
        const FeatureEntryInfo* entry = feature_db->pick_weighted_entry(*pool, rng);
        if (!entry || entry->structure_id.is_empty()) return candidate;

        candidate.feature_id = entry->structure_id;
        candidate.source_size = s_db->get_structure_size(candidate.feature_id);
        if (candidate.source_size.x <= 0 || candidate.source_size.y <= 0) return candidate;

        const bool flipped = rng.range(0, 1) == 1;
        candidate.rotation = place_horizontal
            ? (flipped ? WorldCoords::ROT_WEST : WorldCoords::ROT_EAST)
            : (flipped ? WorldCoords::ROT_NORTH : WorldCoords::ROT_SOUTH);
        candidate.placed_size = place_horizontal ? Vector2i(candidate.source_size.y, candidate.source_size.x) : candidate.source_size;
        if (candidate.placed_size.x >= WorldCoords::CHUNK_SIZE || candidate.placed_size.y >= WorldCoords::CHUNK_SIZE) return candidate;

        const int shoulder_nudge = rng.range(-1, 1);
        const int chunk_world_x = p_chunk_x * WorldCoords::CHUNK_SIZE;
        const int chunk_world_y = p_chunk_y * WorldCoords::CHUNK_SIZE;
        int anchor_x = chunk_world_x;
        int anchor_y = chunk_world_y;

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

        candidate.origin = Vector2i(anchor_x, anchor_y);
        candidate.valid = true;
        return candidate;
    };

    auto candidate_has_priority = [](const RoadFeatureCandidate& p_a, const RoadFeatureCandidate& p_b) -> bool {
        if (p_a.chunk_y != p_b.chunk_y) return p_a.chunk_y < p_b.chunk_y;
        if (p_a.chunk_x != p_b.chunk_x) return p_a.chunk_x < p_b.chunk_x;
        return p_a.index < p_b.index;
    };

    auto candidates_overlap = [&](const RoadFeatureCandidate& p_a, const RoadFeatureCandidate& p_b) -> bool {
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
                if (tile_b != 0 && tile_b != id_void) {
                    return true;
                }
            }
        }

        return false;
    };

    auto overlaps_prior_candidate = [&](const RoadFeatureCandidate& p_candidate) -> bool {
        for (int cy = p_candidate.chunk_y - 1; cy <= p_candidate.chunk_y + 1; cy++) {
            for (int cx = p_candidate.chunk_x - 1; cx <= p_candidate.chunk_x + 1; cx++) {
                const int candidate_count = roadside_candidate_count_for_chunk(cx, cy);
                for (int index = 0; index < candidate_count; index++) {
                    RoadFeatureCandidate prior = build_candidate(cx, cy, index);
                    if (!prior.valid || !candidate_has_priority(prior, p_candidate)) continue;
                    if (!validate_surface_feature_anchor(prior.feature_id, prior.origin, prior.source_size, prior.placed_size, prior.rotation, world_seed)) continue;
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

            const int candidate_count = roadside_candidate_count_for_chunk(anchor_cx, anchor_cy);
            for (int candidate = 0; candidate < candidate_count; candidate++) {
                RoadFeatureCandidate placed = build_candidate(anchor_cx, anchor_cy, candidate);
                if (!placed.valid) continue;

                if (x >= placed.origin.x && x < placed.origin.x + placed.placed_size.x &&
                    y >= placed.origin.y && y < placed.origin.y + placed.placed_size.y) {
                    if (!validate_surface_feature_anchor(placed.feature_id, placed.origin, placed.source_size, placed.placed_size, placed.rotation, world_seed)) {
                        continue;
                    }
                    if (overlaps_prior_candidate(placed)) {
                        continue;
                    }

                    const int local_x = x - placed.origin.x;
                    const int local_y = y - placed.origin.y;
                    const uint16_t tile_id = get_surface_feature_tile_at(placed.feature_id, local_x, local_y, placed.source_size, placed.rotation);
                    if (tile_id != 0 && tile_id != id_void) {
                        return tile_id;
                    }
                }
            }
        }
    }

    return id_void;
}

uint16_t WorldGenerator::get_dungeon_layout_tile(const DungeonLayout& p_layout, int x, int y, bool p_include_dynamic) const {
    if (!p_layout.might_contain(x, y)) {
        return id_void;
    }

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
            return id_dungeon_wall;
        }
        return id_dungeon_floor;
    }

    if (p_layout.has_corridor(x, y)) {
        return id_dungeon_floor;
    }
    if (p_layout.has_corridor_wall(x, y)) {
        return id_dungeon_wall;
    }
    return id_void;
}

namespace {

static constexpr uint64_t DUNGEON_DYNAMIC_FEATURE_SALT = 0x44594E44554E4654ULL; // "DYNDUNFT"
static constexpr uint64_t SPIDER_NEST_SALT = 0x535049444E455354ULL; // "SPIDNEST"
static constexpr float SPIDER_NEST_CHANCE_PER_ROOM = 0.1f;
static constexpr int SPIDER_NEST_RADIUS_MIN = 8;
static constexpr int SPIDER_NEST_RADIUS_MAX = 10;
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

static float distance_sq_float(int p_dx, int p_dy) {
    return static_cast<float>(p_dx * p_dx + p_dy * p_dy);
}

}

void WorldGenerator::try_place_dynamic_dungeon_features(DungeonLayout& r_layout, int p_world_seed) {
    Rng::Seeded rng = Rng::at(
        static_cast<uint32_t>(p_world_seed),
        r_layout.entrance_chunk,
        Rng::BIOME,
        DUNGEON_DYNAMIC_FEATURE_SALT
    );

    for (int room_index = 0; room_index < static_cast<int>(r_layout.rooms.size()); room_index++) {
        if (!rng.chance(SPIDER_NEST_CHANCE_PER_ROOM)) continue;
        try_place_spider_nest(r_layout, p_world_seed, room_index, r_layout.rooms[room_index]);
    }
}

bool WorldGenerator::try_place_spider_nest(DungeonLayout& r_layout, int p_world_seed, int p_room_index, const PlacedDungeonRoom& p_room) {
    std::vector<Vector2i> candidate_centers;
    candidate_centers.reserve(p_room.bounds.size.x * p_room.bounds.size.y);
    for (int y = p_room.bounds.origin.y; y < p_room.bounds.origin.y + p_room.bounds.size.y; y++) {
        for (int x = p_room.bounds.origin.x; x < p_room.bounds.origin.x + p_room.bounds.size.x; x++) {
            if (get_dungeon_layout_tile(r_layout, x, y, false) == id_dungeon_floor) {
                candidate_centers.push_back(Vector2i(x, y));
            }
        }
    }
    if (candidate_centers.empty()) {
        return false;
    }

    Rng::Seeded rng = Rng::at(
        static_cast<uint32_t>(p_world_seed),
        r_layout.entrance_chunk,
        Rng::BIOME,
        SPIDER_NEST_SALT ^ (static_cast<uint64_t>(p_room_index + 1) * 0x9E3779B97F4A7C15ULL)
    );

    const Vector2i center = candidate_centers[rng.range(0, static_cast<int>(candidate_centers.size()) - 1)];
    const int radius = rng.range(SPIDER_NEST_RADIUS_MIN, SPIDER_NEST_RADIUS_MAX);
    SpiderNestArea area;

    for (int y = center.y - radius - 1; y <= center.y + radius + 1; y++) {
        for (int x = center.x - radius - 1; x <= center.x + radius + 1; x++) {
            const int dx = x - center.x;
            const int dy = y - center.y;
            const float noise = rng.unit() * 1.6f - 0.8f;
            const float rough_radius = static_cast<float>(radius) + noise;
            if (distance_sq_float(dx, dy) > rough_radius * rough_radius) {
                continue;
            }

            const uint16_t tile_id = get_dungeon_layout_tile(r_layout, x, y, false);
            if (tile_id == id_dungeon_floor) {
                Vector2i cell(x, y);
                area.floor_cells.push_back(cell);
                if (dx * dx + dy * dy <= SPIDER_NEST_EGG_RADIUS * SPIDER_NEST_EGG_RADIUS) {
                    area.center_floor_cells.push_back(cell);
                }
            } else if (tile_id == id_dungeon_wall) {
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
        const int dx = cell.x - center.x;
        const int dy = cell.y - center.y;
        const bool near_center = dx * dx + dy * dy <= SPIDER_NEST_CENTER_RADIUS * SPIDER_NEST_CENTER_RADIUS;
        const uint16_t web_tile_id = pick_web_tile(id_dungeon_wall_web, id_dungeon_wall_web_thick, near_center);
        if (web_tile_id != 0) {
            r_layout.dynamic_tiles[dungeon_dynamic_cell_key(cell.x, cell.y)] = web_tile_id;
        }
    }

    for (const Vector2i& cell : area.floor_cells) {
        const int dx = cell.x - center.x;
        const int dy = cell.y - center.y;
        const bool near_center = dx * dx + dy * dy <= SPIDER_NEST_CENTER_RADIUS * SPIDER_NEST_CENTER_RADIUS;
        const uint16_t web_tile_id = pick_web_tile(id_dungeon_floor_web, id_dungeon_floor_web_thick, near_center);
        if (web_tile_id != 0) {
            r_layout.dynamic_tiles[dungeon_dynamic_cell_key(cell.x, cell.y)] = web_tile_id;
        }
    }

    std::vector<Vector2i> egg_candidates = area.center_floor_cells.empty() ? area.floor_cells : area.center_floor_cells;
    const int egg_count = std::min(rng.range(4, 6), static_cast<int>(egg_candidates.size()));
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
    if (!info || info->generator != "room_graph") {
        return nullptr;
    }

    DungeonLayout layout = DungeonGenerator::build_layout(*info, p_entrance_chunk, p_world_seed);
    try_place_dynamic_dungeon_features(layout, p_world_seed);
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

bool WorldGenerator::is_dungeon_floor_loot_candidate(int x, int y, int z, int world_seed) {
    setup_biome_rules();
    return get_dungeon_tile(x, y, z, world_seed) == id_dungeon_floor;
}

uint16_t WorldGenerator::get_tile(int x, int y, int world_seed) {
    uint16_t base_tile_id = get_base_surface_tile(x, y, world_seed);
    uint16_t feature_tile_id = get_surface_feature_tile(x, y, base_tile_id, world_seed);
    return feature_tile_id != id_void ? feature_tile_id : base_tile_id;
}

uint16_t WorldGenerator::get_tile(int x, int y, int z, int world_seed) {
    if (z == 0) return get_tile(x, y, world_seed);

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
            uint16_t tile_id = s_db->get_tile_at(structure_id, rx, ry, z);
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
    last_chunk_valid = false;
    reset_dungeon_cache();
}

void WorldGenerator::set_biome_layers(const std::unordered_map<int, BiomeLayer>& layers) {
    setup_biome_rules();
    biome_layers = layers;
    last_chunk_valid = false;
    reset_dungeon_cache();
}

void WorldGenerator::clear_region_chunks() {
    biome_layers.clear();
    last_chunk_valid = false;
    reset_dungeon_cache();
}
