#include "world_generation.h"
#include "data/structure_db.h"

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

String WorldGeneration::get_plains_tile(uint32_t roll) {
    if (roll < 35) return "grass1";
    if (roll < 70) return "grass2";
    if (roll < 90) return "dirt";
    return "grass3";
}

// Get tile type from noise
String WorldGeneration::get_tile(int x, int y) {
    int cx = x >> CHUNK_SHIFT;
    int cy = y >> CHUNK_SHIFT;

    auto it = region_chunks.find(Occlusion::pack_coords(cx, cy));
    if (it == region_chunks.end()) return "void";

    const String& chunk_id = it->second;

    uint32_t h = (static_cast<uint32_t>(x) * 1597334677U) ^ 
                 (static_cast<uint32_t>(y) * 3812015801U) ^ 
                 (static_cast<uint32_t>(world_seed));
    
    // Normalize hash to 0-99 range once
    uint32_t roll = h % 100;

    if (chunk_id == "forest") {
        if (roll < 30) return "tree";
        return get_plains_tile((roll - 30) * 100 / 70);
    }
    
    if (chunk_id == "plains") {
        return get_plains_tile(roll);
    }

    if (chunk_id == "road")     return "stone_bricks";
    if (chunk_id == "alley")    return "alley_bricks";
    
    if (chunk_id == "building") {
        int lx = x & (CHUNK_SIZE - 1);
        int ly = y & (CHUNK_SIZE - 1);
        
        StructureDb* s_db = StructureDb::get_singleton();
        if (s_db) {
            String tile = s_db->get_tile_at("house01", lx, ly);
            if (tile != "void" && tile != "") return tile;
        }
        return get_plains_tile(roll);
    }

    if (chunk_id == "plaza")    return "w_floor";
    if (chunk_id == "gate")     return "gate_floor";
    if (chunk_id == "palace")   return "palace_floor";

    return "void";
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
        String tile_id;
        auto it = tile_id_cache.find(cellKey);
        if (it != tile_id_cache.end()) {
            tile_id = it->second;
        } else {
            tile_id = get_tile(cx, cy);
            tile_id_cache[cellKey] = tile_id;
        }
        
        update_tile_at(ox, oy, playerPos, tile_id, rs, texture_rid, tile_db);
    }
    
    // Check if all 8 surrounding tiles are walls - if so, skip occlusion computation
    bool all_surrounded = Occlusion::is_surrounded_by_walls(playerPos, tile_id_cache);
    
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
        
        // Fast path: if surrounded by walls, everything except player tile and adjacent walls is occluded
        bool occluded;
        if (all_surrounded) {
            // Player tile is visible, adjacent walls are visible, everything else occluded
            bool is_player_tile = (ox == 0 && oy == 0);
            bool is_adjacent = (abs(ox) <= 1 && abs(oy) <= 1);
            occluded = !is_player_tile && !is_adjacent;
        } else {
            Vector2i cellPos(cx, cy);
            occluded = Occlusion::is_occluded(cellPos, playerPos, tile_id_cache);
        }
        
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
    region_chunks.clear();

    // Create a 256x256 city canvas (one city tile per chunk)
    Canvas cityCanvas(REGION_SIZE);
    CityGeneration cityGen(cityCanvas, world_seed + regionPos.x * 31 + regionPos.y * 7);
    
    cityGen.generateCity(
        48, 8, 3,       // radius, spokes, rings
        45, 4, 5,       // reach, outerDensity, innerDensity
        true, false,    // showInner, showTwin
        30, 4, 6, 2,    // twinRadius, twinDensity, twinSpokes, twinRings
        false, true, true // useRiver, useJitter, useSpecial
    );

    Dictionary result;
    for (int y = 0; y < REGION_SIZE; y++) {
        for (int x = 0; x < REGION_SIZE; x++) {
            String cityTile = cityCanvas.getPixel(x, y);
            String chunk_id = CityGeneration::get_chunk_id(cityTile);
            
            // Store the chunk type using packed coordinates relative to regionPos
            int gx = regionPos.x * REGION_SIZE + x;
            int gy = regionPos.y * REGION_SIZE + y;
            uint64_t key = Occlusion::pack_coords(gx, gy);
            region_chunks[key] = chunk_id;
            result[key] = chunk_id;
        }
    }

    UtilityFunctions::print("Region initialized with city center at (", regionPos.x, ", ", regionPos.y, ")");
    return result;
}
