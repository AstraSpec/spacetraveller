#include "world_generation.h"

using namespace godot;

void WorldGeneration::_bind_methods() {
    // Property bindings
    ClassDB::bind_method(D_METHOD("set_biome_noise", "noise"), &WorldGeneration::set_biome_noise);
    ClassDB::bind_method(D_METHOD("get_biome_noise"), &WorldGeneration::get_biome_noise);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "biome_noise", PROPERTY_HINT_RESOURCE_TYPE, "FastNoiseLite"), "set_biome_noise", "get_biome_noise");
    
    ClassDB::bind_method(D_METHOD("set_tilesheet", "texture"), &WorldGeneration::set_tilesheet);
    ClassDB::bind_method(D_METHOD("get_tilesheet"), &WorldGeneration::get_tilesheet);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "tilesheet", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_tilesheet", "get_tilesheet");
    
    ClassDB::bind_method(D_METHOD("set_world_seed", "seed"), &WorldGeneration::set_world_seed);
    ClassDB::bind_method(D_METHOD("get_world_seed"), &WorldGeneration::get_world_seed);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "world_seed"), "set_world_seed", "get_world_seed");
    
    // Expose constants
    ClassDB::bind_static_method("WorldGeneration", D_METHOD("get_bubble_radius"), &WorldGeneration::get_bubble_radius);
    
    // Method bindings
    ClassDB::bind_method(D_METHOD("init_world_bubble", "playerPos"), &WorldGeneration::init_world_bubble);
    ClassDB::bind_method(D_METHOD("update_world_bubble", "playerPos"), &WorldGeneration::update_world_bubble);
    ClassDB::bind_method(D_METHOD("init_region", "regionPos"), &WorldGeneration::init_region);
}

WorldGeneration::WorldGeneration() {
}

WorldGeneration::~WorldGeneration() {
    // Clean up RIDs
    RenderingServer* rs = RenderingServer::get_singleton();
    for (auto& pair : tile_rids) {
        rs->free_rid(pair.second);
    }
    tile_rids.clear();
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

void WorldGeneration::set_tilesheet(const Ref<Texture2D>& texture) {
    tilesheet = texture;
}

Ref<Texture2D> WorldGeneration::get_tilesheet() const {
    return tilesheet;
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

// Get tile type from noise
String WorldGeneration::get_tile(int x, int y) {
    // divide x and y by chunk size then floor to find chunk
    int cx = static_cast<int>(std::floor(static_cast<float>(x) / CHUNK_SIZE));
    int cy = static_cast<int>(std::floor(static_cast<float>(y) / CHUNK_SIZE));

    // get the chunk type from region_chunks
    auto it = region_chunks.find(Occlusion::pack_coords(cx, cy));
    
    // if chunk out of bounds
    if (it == region_chunks.end()) {
        return "void";
    }

    const String& chunk_id = it->second;

    // Mapping chunk types to tile IDs
    if (chunk_id == "forest") {
        // Use a deterministic pseudo-random check based on position and seed
        uint32_t hash = static_cast<uint32_t>(x) * 73856093 ^ 
                        static_cast<uint32_t>(y) * 19349663 ^ 
                        static_cast<uint32_t>(world_seed) * 83492791;
        
        if ((hash % 100) < 30) {
            return "tree";
        }
        return "grass";
    }
    else if (chunk_id == "plains") {
        return "grass";
    }
    else if (chunk_id == "road") {
        return "stone_bricks";
    }
    else if (chunk_id == "alley") {
        return "alley_bricks";
    }
    else if (chunk_id == "building") {
        return "wooden_floor";
    }
    else if (chunk_id == "plaza") {
        return "plaza_floor";
    }
    else if (chunk_id == "gate") {
        return "gate_floor";
    }
    else if (chunk_id == "palace") {
        return "palace_floor";
    }

    return "void";
}

// Initialize world bubble
void WorldGeneration::init_world_bubble(const Vector2i& playerPos) {
    RenderingServer* rs = RenderingServer::get_singleton();
    RID parent_rid = get_canvas_item();
    
    // Clear any existing tiles
    for (auto& pair : tile_rids) {
        rs->free_rid(pair.second);
    }
    tile_rids.clear();
    tile_id_cache.clear();
    seen_cells.clear();
    
    // Create tiles in circular bubble
    for (int i = 0; i < WORLD_BUBBLE_SIZE * WORLD_BUBBLE_SIZE; i++) {
        int ox = (i / WORLD_BUBBLE_SIZE) - WORLD_BUBBLE_RADIUS;
        int oy = (i % WORLD_BUBBLE_SIZE) - WORLD_BUBBLE_RADIUS;
        
        // Check if within radius
        float dist = sqrtf(static_cast<float>(ox * ox + oy * oy));
        if (dist < static_cast<float>(WORLD_BUBBLE_RADIUS)) {
            uint64_t offsetKey = Occlusion::pack_coords(ox, oy);
            
            // Create canvas item for this tile
            RID tile_rid = rs->canvas_item_create();
            rs->canvas_item_set_parent(tile_rid, parent_rid);
            tile_rids[offsetKey] = tile_rid;
        }
    }
    
    // Initial render
    update_world_bubble(playerPos);
}

// Update world bubble - main loop, all in C++
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
        RID tile_rid = pair.second;
        
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
        
        Vector2i atlas_pos(1, 1);
        const TileInfo* info = tile_db->get_tile_info(tile_id);
        if (info) {
            atlas_pos.x = 1 + info->atlas.x * (TILE_SIZE + 1);
            atlas_pos.y = 1 + info->atlas.y * (TILE_SIZE + 1);
        }
        
        // Clear and render tile
        rs->canvas_item_clear(tile_rid);
        rs->canvas_item_add_texture_rect_region(
            tile_rid,
            Rect2(ox * TILE_SIZE, oy * TILE_SIZE, TILE_SIZE, TILE_SIZE),
            texture_rid,
            Rect2(atlas_pos.x, atlas_pos.y, TILE_SIZE, TILE_SIZE)
        );
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
    Dictionary result;
    return result;
}
