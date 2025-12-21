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
int WorldGeneration::get_tile_y(int x, int y) {
    if (!biome_noise.is_valid()) {
        return TILE_Y_GROUND;
    }
    float biome = biome_noise->get_noise_2d(static_cast<float>(x), static_cast<float>(y));
    return (biome > 0.3f) ? TILE_Y_WALL : TILE_Y_GROUND;
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
    tile_y_cache.clear();
    seen_cells.clear();
    
    // Create tiles in circular bubble
    for (int i = 0; i < WORLD_BUBBLE_SIZE * WORLD_BUBBLE_SIZE; i++) {
        int ox = (i / WORLD_BUBBLE_SIZE) - WORLD_BUBBLE_RADIUS;
        int oy = (i % WORLD_BUBBLE_SIZE) - WORLD_BUBBLE_RADIUS;
        
        // Check if within radius
        float dist = sqrtf(static_cast<float>(ox * ox + oy * oy));
        if (dist < static_cast<float>(WORLD_BUBBLE_RADIUS)) {
            uint64_t offsetKey = pack_coords(ox, oy);
            
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
    
    // Build tile map for current bubble (for occlusion)
    std::unordered_map<uint64_t, int> bubble_tile_map;
    bubble_tile_map.reserve(tile_rids.size());
    
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
        uint64_t cellKey = pack_coords(cx, cy);
        
        // Get or compute tile_y
        int tile_y;
        auto it = tile_y_cache.find(cellKey);
        if (it != tile_y_cache.end()) {
            tile_y = it->second;
        } else {
            tile_y = get_tile_y(cx, cy);
            tile_y_cache[cellKey] = tile_y;
        }
        
        // Store in bubble map for occlusion
        bubble_tile_map[cellKey] = tile_y;
        
        // Clear and render tile
        rs->canvas_item_clear(tile_rid);
        rs->canvas_item_add_texture_rect_region(
            tile_rid,
            Rect2(ox * TILE_SIZE, oy * TILE_SIZE, TILE_SIZE, TILE_SIZE),
            texture_rid,
            Rect2(1, tile_y, TILE_SIZE, TILE_SIZE)
        );
    }
    
    // Check if all 8 surrounding tiles are walls - if so, skip occlusion computation
    bool all_surrounded = true;
    static const int neighbors[8][2] = {
        {-1, -1}, {0, -1}, {1, -1},
        {-1,  0},          {1,  0},
        {-1,  1}, {0,  1}, {1,  1}
    };
    
    for (int i = 0; i < 8 && all_surrounded; i++) {
        int nx = playerPos.x + neighbors[i][0];
        int ny = playerPos.y + neighbors[i][1];
        auto it = tile_y_cache.find(pack_coords(nx, ny));
        if (it == tile_y_cache.end() || it->second != TILE_Y_WALL) {
            all_surrounded = false;
        }
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
        uint64_t cellKey = pack_coords(cx, cy);
        
        // Fast path: if surrounded by walls, everything except player tile and adjacent walls is occluded
        bool occluded;
        if (all_surrounded) {
            // Player tile is visible, adjacent walls are visible, everything else occluded
            bool is_player_tile = (ox == 0 && oy == 0);
            bool is_adjacent = (abs(ox) <= 1 && abs(oy) <= 1);
            occluded = !is_player_tile && !is_adjacent;
        } else {
            Vector2i cellPos(cx, cy);
            occluded = is_occluded_internal(cellPos, playerPos);
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

// Occlusion check using internal tile_y_cache
bool WorldGeneration::is_occluded_internal(const Vector2i& cellPos, const Vector2i& playerPos) {
    if (cellPos == playerPos) {
        return false;
    }
    
    uint64_t cellKey = pack_coords(cellPos.x, cellPos.y);
    auto cellIt = tile_y_cache.find(cellKey);
    if (cellIt == tile_y_cache.end()) {
        return false;
    }
    
    int tileY = cellIt->second;
    if (tileY != TILE_Y_GROUND && tileY != TILE_Y_WALL) {
        return false;
    }
    
    // Bresenham's line algorithm
    int x0 = playerPos.x, y0 = playerPos.y;
    int x1 = cellPos.x, y1 = cellPos.y;
    
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    
    int x = x0, y = y0;
    
    while (true) {
        if (x != x0 || y != y0) {
            if (x == x1 && y == y1) {
                break;
            }
            
            auto it = tile_y_cache.find(pack_coords(x, y));
            if (it != tile_y_cache.end() && it->second == TILE_Y_WALL) {
                return true;
            }
        }
        
        if (x == x1 && y == y1) {
            break;
        }
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
    
    return false;
}
