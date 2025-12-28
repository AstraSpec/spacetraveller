#include "world_generation.h"
#include "data/structure_db.h"
#include "data/id_registry.h"

using namespace godot;

void WorldGeneration::_bind_methods() {
    // Property bindings
    ClassDB::bind_method(D_METHOD("set_biome_noise", "noise"), &WorldGeneration::set_biome_noise);
    ClassDB::bind_method(D_METHOD("get_biome_noise"), &WorldGeneration::get_biome_noise);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "biome_noise", PROPERTY_HINT_RESOURCE_TYPE, "FastNoiseLite"), "set_biome_noise", "get_biome_noise");
    
    ClassDB::bind_method(D_METHOD("set_world_seed", "seed"), &WorldGeneration::set_world_seed);
    ClassDB::bind_method(D_METHOD("get_world_seed"), &WorldGeneration::get_world_seed);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "world_seed"), "set_world_seed", "get_world_seed");
    
    // Expose constants
    ClassDB::bind_static_method("WorldGeneration", D_METHOD("get_region_size"), &WorldGeneration::get_region_size);
    ClassDB::bind_static_method("WorldGeneration", D_METHOD("get_chunk_size"), &WorldGeneration::get_chunk_size);
    ClassDB::bind_static_method("WorldGeneration", D_METHOD("get_chunk_shift"), &WorldGeneration::get_chunk_shift);
    
    // Method bindings
    ClassDB::bind_method(D_METHOD("update_world_bubble", "playerPos"), &WorldGeneration::update_world_bubble);
    ClassDB::bind_method(D_METHOD("init_region", "regionPos"), &WorldGeneration::init_region);
}

WorldGeneration::WorldGeneration() {
}

WorldGeneration::~WorldGeneration() {
}

// Property setters/getters
void WorldGeneration::set_biome_noise(const Ref<FastNoiseLite>& noise) {
    biome_noise = noise;
    if (biome_noise.is_valid()) {
        biome_noise->set_seed(world_seed);
    }
}

Ref<FastNoiseLite> WorldGeneration::get_biome_noise() const {
    return biome_noise;
}

void WorldGeneration::set_world_seed(int seed) {
    world_seed = seed;
    if (biome_noise.is_valid()) {
        biome_noise->set_seed(seed);
    }
}

int WorldGeneration::get_world_seed() const {
    return world_seed;
}

uint16_t WorldGeneration::pick_weighted_tile(const BiomeInfo& info, uint32_t roll) {
    if (info.ground_tiles.size() == 1) return info.ground_tiles[0].id;

    int cumulative = 0;
    for (const auto& tile : info.ground_tiles) {
        cumulative += tile.weight;
        if (roll < (uint32_t)cumulative) {
            return tile.id;
        }
    }
    return info.ground_tiles.empty() ? id_void : info.ground_tiles[0].id;
}

void WorldGeneration::setup_biome_rules() {
    if (!biome_rules.empty()) return; // Already setup

    id_reg = IdRegistry::get_singleton();
    s_db = StructureDb::get_singleton();
    if (!id_reg) return;

    id_void = id_reg->register_string("void");
    id_building = id_reg->register_string("building");

    // Helper to register a biome
    auto reg_biome = [&](const String& name, const std::vector<std::pair<String, int>>& tiles) {
        uint16_t b_id = id_reg->register_string(name);
        BiomeInfo info;
        for (const auto& t : tiles) {
            info.ground_tiles.push_back({id_reg->register_string(t.first), t.second});
        }
        biome_rules[b_id] = info;
    };

    // 1. Plains
    reg_biome("plains", {
        {"grass1", 35}, {"grass2", 35}, {"dirt", 20}, {"grass3", 10}
    });

    // 2. Forest
    reg_biome("forest", {
        {"tree", 30}, {"grass1", 24}, {"grass2", 24}, {"dirt", 14}, {"grass3", 8}
    });

    // 3. Buildings (Default ground)
    reg_biome("building", {
        {"grass1", 35}, {"grass2", 35}, {"dirt", 20}, {"grass3", 10}
    });

    // 4. Roads/Alleys/Floors (Fixed Overrides)
    auto reg_simple = [&](const String& name, const String& tile) {
        uint16_t b_id = id_reg->register_string(name);
        BiomeInfo info;
        info.ground_tiles.push_back({id_reg->register_string(tile), 100});
        biome_rules[b_id] = info;
    };

    reg_simple("road", "stone_bricks");
    reg_simple("alley", "alley_bricks");
    reg_simple("plaza", "w_floor");
    reg_simple("gate", "gate_floor");
    reg_simple("palace", "palace_floor");
}

uint16_t WorldGeneration::get_tile(int x, int y) {
    int cx = x >> CHUNK_SHIFT;
    int cy = y >> CHUNK_SHIFT;
    uint64_t chunk_key = Occlusion::pack_coords(cx, cy);

    if (!last_chunk_valid || last_chunk_key != chunk_key) {
        auto it = region_chunks.find(chunk_key);
        if (it == region_chunks.end()) {
            last_chunk_valid = false;
            return id_void;
        }
        
        uint32_t packed = it->second;
        last_chunk_id = static_cast<uint16_t>(packed & 0xFFFF);
        last_chunk_rotation = static_cast<uint8_t>(packed >> 16);
        last_chunk_key = chunk_key;
        
        auto it_rule = biome_rules.find(last_chunk_id);
        last_biome_ptr = (it_rule != biome_rules.end()) ? &it_rule->second : nullptr;
        
        last_chunk_valid = true;
    }

    const uint16_t chunk_id = last_chunk_id;

    // 1. Structure Lookup Path (Hot Path)
    if (chunk_id == id_building && s_db) {
        int lx = x & (CHUNK_SIZE - 1);
        int ly = y & (CHUNK_SIZE - 1);
        
        int rx = lx, ry = ly;
        switch (last_chunk_rotation) {
            case 1: rx = ly; ry = 31 - lx; break; // West
            case 2: rx = 31 - lx; ry = 31 - ly; break; // North
            case 3: rx = 31 - ly; ry = lx; break; // East
        }

        // Optimized StructureDb call
        uint16_t tile_id = s_db->get_tile_at("house01", rx, ry);
        if (tile_id != id_void) return tile_id;
    }

    // 2. Biome Logic Path (Using cached pointer)
    if (last_biome_ptr) {
        const BiomeInfo& info = *last_biome_ptr;
        
        uint32_t h = (static_cast<uint32_t>(x) * 1597334677U) ^ 
                     (static_cast<uint32_t>(y) * 3812015801U) ^ 
                     (static_cast<uint32_t>(world_seed));
        return pick_weighted_tile(info, h % 100);
    }

    return id_void;
}

// Update world bubble - main loop
void WorldGeneration::update_world_bubble(const Vector2i& playerPos) {
    if (!tilesheet.is_valid()) {
        return;
    }
    
    RenderingServer* rs = RenderingServer::get_singleton();
    RID texture_rid = tilesheet->get_rid();
    TileDb* tile_db = TileDb::get_singleton();
    if (!tile_db) return;
    
    // First pass: render tiles and build tile map
    for (auto& pair : tile_rids) {
        uint64_t offsetKey = pair.first;
        
        // Unpack offset
        int ox = static_cast<int>(static_cast<int32_t>(offsetKey >> 32));
        int oy = static_cast<int>(static_cast<int32_t>(offsetKey & 0xFFFFFFFF));
        
        // Calculate cell position
        int cx = ox + playerPos.x;
        int cy = oy + playerPos.y;
        uint64_t cellKey = Occlusion::pack_coords(cx, cy);
        
        // Get or compute tile ID
        uint16_t tile_id;
        auto it = tile_id_cache.find(cellKey);
        if (it != tile_id_cache.end()) {
            tile_id = it->second;
        } else {
            tile_id = get_tile(cx, cy);
            tile_id_cache[cellKey] = tile_id;
        }
        
        update_tile_at(ox, oy, playerPos, tile_id, rs, texture_rid, tile_db);
    }
    
    // Second pass: compute occlusion and apply modulation
    for (auto& pair : tile_rids) {
        uint64_t offsetKey = pair.first;
        RID tile_rid = pair.second;
        
        // Unpack offset
        int ox = static_cast<int>(static_cast<int32_t>(offsetKey >> 32));
        int oy = static_cast<int>(static_cast<int32_t>(offsetKey & 0xFFFFFFFF));
        
        // Calculate cell position
        int cx = ox + playerPos.x;
        int cy = oy + playerPos.y;
        uint64_t cellKey = Occlusion::pack_coords(cx, cy);
        
        bool occluded;
        Vector2i cellPos(cx, cy);
        occluded = Occlusion::is_occluded(cellPos, playerPos, tile_id_cache);
        
        Color color(1.0f, 1.0f, 1.0f, 1.0f);
        if (occluded) {
            if (seen_cells.count(cellKey) > 0) {
                color = Color(0.4f, 0.4f, 0.5f, 1.0f);  // Previously seen
            } else {
                color = Color(0.0f, 0.0f, 0.0f, 1.0f);  // Never seen
            }
        } else {
            seen_cells.insert(cellKey);  // Mark as seen
        }
        
        rs->canvas_item_set_modulate(tile_rid, color);
    }
}

// Initialize world bubble
Dictionary WorldGeneration::init_region(const Vector2i& regionPos) {
    setup_biome_rules();
    
    region_chunks.clear();
    last_chunk_valid = false;

    Canvas cityCanvas(REGION_SIZE);
    CityGeneration::spawn_city(cityCanvas, 127, 128, world_seed);

    Dictionary result;
    for (int y = 0; y < REGION_SIZE; y++) {
        for (int x = 0; x < REGION_SIZE; x++) {
            String cityTile = cityCanvas.getPixel(x, y);
            String chunk_name = CityGeneration::get_chunk_id(cityTile);
            uint16_t chunk_id = id_reg ? id_reg->register_string(chunk_name) : 0;
            
            // Store the chunk type using packed coordinates relative to regionPos
            int gx = regionPos.x * REGION_SIZE + x;
            int gy = regionPos.y * REGION_SIZE + y;
            uint64_t key = Occlusion::pack_coords(gx, gy);

            uint8_t rot = 0;
            // Calculate rotation for buildings
            if (chunk_name == "building") {
                auto is_road = [&](int nx, int ny) {
                    if (nx < 0 || nx >= REGION_SIZE || ny < 0 || ny >= REGION_SIZE) return false;
                    String nt = CityGeneration::get_chunk_id(cityCanvas.getPixel(nx, ny));
                    return nt == "road" || nt == "alley";
                };

                if (is_road(x, y + 1)) rot = 0;      // South
                else if (is_road(x, y - 1)) rot = 2; // North
                else if (is_road(x - 1, y)) rot = 1; // West
                else if (is_road(x + 1, y)) rot = 3; // East
            }

            // Pack rotation (8-bit) and chunk_id (16-bit) into 32-bit map value
            region_chunks[key] = (static_cast<uint32_t>(rot) << 16) | chunk_id;
            result[key] = chunk_name;
        }
    }

    return result;
}
