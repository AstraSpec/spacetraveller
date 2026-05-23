#include "world_generator.h"
#include "data/structure_db.h"
#include "core/id_registry.h"
#include "data/chunk_db.h"
#include "core/tag_registry.h"
#include "city_generation.h"
#include "gen_grid.h"

using namespace godot;

WorldGenerator::WorldGenerator() {}
WorldGenerator::~WorldGenerator() = default;

void WorldGenerator::setup_biome_rules() {
    if (!biome_rules.empty()) return;

    id_reg = IdRegistry::get_singleton();
    s_db = StructureDb::get_singleton();
    if (!id_reg) return;

    id_void = id_reg->register_string("void");
    id_building = id_reg->register_string("building");
    id_forest = id_reg->register_string("forest");
    id_plains = id_reg->register_string("plains");

    auto reg_biome = [&](const String& name, const std::vector<std::pair<String, int>>& tiles) {
        uint16_t b_id = id_reg->register_string(name);
        BiomeInfo info;
        for (const auto& t : tiles) {
            info.ground_tiles.push_back({id_reg->register_string(t.first), t.second});
        }
        biome_rules[b_id] = info;
    };

    reg_biome("plains", {{"grass", 80}, {"dirt", 20}});
    reg_biome("forest", {{"tree", 30}, {"grass", 56}, {"dirt", 14}});
    reg_biome("building", {{"grass", 80}, {"dirt", 20}});

    auto reg_simple = [&](const String& name, const String& tile) {
        uint16_t b_id = id_reg->register_string(name);
        BiomeInfo info;
        info.ground_tiles.push_back({id_reg->register_string(tile), 100});
        biome_rules[b_id] = info;
    };

    auto reg_tiled = [&](const String& name, const String& tile, const String& border) {
        uint16_t b_id = id_reg->register_string(name);
        BiomeInfo info;
        info.ground_tiles.push_back({id_reg->register_string(tile), 100});
        info.auto_tiled = true;
        info.border_tile_id = id_reg->register_string(border);
        biome_rules[b_id] = info;
    };

    reg_tiled("road", "road_bricks", "road_flagstone");
    reg_tiled("alley", "alley_bricks", "alley_flagstone");
    reg_simple("plaza", "w_floor");
    reg_simple("gate", "gate_floor");
    reg_simple("palace", "palace_floor");
    reg_simple("wall", "w_wall");
}

Dictionary WorldGenerator::init_region(const Vector2i& regionPos, int world_seed, const Ref<FastNoiseLite>& biome_noise) {
    setup_biome_rules();
    
    region_chunks.clear();
    last_chunk_valid = false;

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
                chunk_id = (h % 100 < 50) ? id_forest : id_plains;
            }

            uint8_t rot = pixel.meta & WorldCoords::ROTATION_MASK;
            int gx = regionPos.x * WorldCoords::REGION_SIZE + x;
            int gy = regionPos.y * WorldCoords::REGION_SIZE + y;
            uint64_t key = WorldCoords::pack_coords(gx, gy);

            region_chunks[key] = (static_cast<uint32_t>(rot) << WorldCoords::ORIENTATION_SHIFT) | chunk_id;
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
    chunk_keys.reserve(region_chunks.size());

    for (auto& pair : region_chunks) {
        chunk_keys.push_back(pair.first);
        Vector2i pos = WorldCoords::unpack_coords(pair.first);
        int rel_x = pos.x - p_region_pos.x * WorldCoords::REGION_SIZE;
        int rel_y = pos.y - p_region_pos.y * WorldCoords::REGION_SIZE;
        if (rel_x >= 0 && rel_x < WorldCoords::REGION_SIZE && rel_y >= 0 && rel_y < WorldCoords::REGION_SIZE) {
            grid[rel_y * WorldCoords::REGION_SIZE + rel_x] = static_cast<uint16_t>(pair.second & WorldCoords::ID_MASK);
        }
    }

    for (uint64_t key : chunk_keys) {
        uint32_t packed = region_chunks[key];
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
                auto n_it = region_chunks.find(n_key);
                return (n_it != region_chunks.end()) ? static_cast<uint16_t>(n_it->second & WorldCoords::ID_MASK) : 0;
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

        region_chunks[key] = (packed & ~(WorldCoords::NEIGHBOR_MASK << WorldCoords::NEIGHBOR_SHIFT)) | (mask << WorldCoords::NEIGHBOR_SHIFT);
    }
}

uint16_t WorldGenerator::pick_weighted_tile(const BiomeInfo& info, uint32_t roll) {
    if (info.ground_tiles.size() == 1) return info.ground_tiles[0].id;
    int cumulative = 0;
    for (const auto& tile : info.ground_tiles) {
        cumulative += tile.weight;
        if (roll < (uint32_t)cumulative) return tile.id;
    }
    return info.ground_tiles.empty() ? id_void : info.ground_tiles[0].id;
}

uint16_t WorldGenerator::get_tile(int x, int y, int world_seed) {
    int cx = (x >= 0) ? (x / WorldCoords::CHUNK_SIZE) : ((x - (WorldCoords::CHUNK_SIZE - 1)) / WorldCoords::CHUNK_SIZE);
    int cy = (y >= 0) ? (y / WorldCoords::CHUNK_SIZE) : ((y - (WorldCoords::CHUNK_SIZE - 1)) / WorldCoords::CHUNK_SIZE);
    uint64_t chunk_key = WorldCoords::pack_coords(cx, cy);

    if (!last_chunk_valid || last_chunk_key != chunk_key) {
        auto it = region_chunks.find(chunk_key);
        if (it == region_chunks.end()) {
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

    if (chunk_id == id_building && s_db) {
        int lx = x % WorldCoords::CHUNK_SIZE; if (lx < 0) lx += WorldCoords::CHUNK_SIZE;
        int ly = y % WorldCoords::CHUNK_SIZE; if (ly < 0) ly += WorldCoords::CHUNK_SIZE;
        int rx = lx, ry = ly;
        int max_coord = WorldCoords::CHUNK_SIZE - 1;
        switch (last_chunk_rotation) {
            case WorldCoords::ROT_WEST: rx = ly; ry = max_coord - lx; break;
            case WorldCoords::ROT_NORTH: rx = max_coord - lx; ry = max_coord - ly; break;
            case WorldCoords::ROT_EAST: rx = max_coord - ly; ry = lx; break;
        }
        uint16_t tile_id = s_db->get_tile_at("house01", rx, ry);
        if (tile_id != id_void) return tile_id;
    }

    if (last_biome_ptr) {
        uint32_t h = get_hash(x, y, static_cast<uint32_t>(world_seed));
        return pick_weighted_tile(*last_biome_ptr, h % 100);
    }

    return id_void;
}
