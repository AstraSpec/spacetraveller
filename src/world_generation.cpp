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

    ClassDB::bind_method(D_METHOD("set_ignore_occlusion", "ignore"), &WorldGeneration::set_ignore_occlusion);
    ClassDB::bind_method(D_METHOD("get_ignore_occlusion"), &WorldGeneration::get_ignore_occlusion);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "ignore_occlusion"), "set_ignore_occlusion", "get_ignore_occlusion");
    
    // Expose constants
    ClassDB::bind_static_method("WorldGeneration", D_METHOD("get_region_size"), &WorldGeneration::get_region_size);
    ClassDB::bind_static_method("WorldGeneration", D_METHOD("get_chunk_size"), &WorldGeneration::get_chunk_size);
    
    ClassDB::bind_static_method("WorldGeneration", D_METHOD("pack_coords", "x", "y"), &WorldGeneration::pack_coords);
    ClassDB::bind_static_method("WorldGeneration", D_METHOD("unpack_coords", "key"), &WorldGeneration::unpack_coords);

    BIND_CONSTANT(ROTATION_MASK);
    BIND_CONSTANT(ORIENTATION_SHIFT);
    BIND_CONSTANT(ID_MASK);

    BIND_CONSTANT(ROT_SOUTH);
    BIND_CONSTANT(ROT_WEST);
    BIND_CONSTANT(ROT_NORTH);
    BIND_CONSTANT(ROT_EAST);

    // Method bindings
    ClassDB::bind_method(D_METHOD("update_world_bubble", "playerPos"), &WorldGeneration::update_world_bubble);
    ClassDB::bind_method(D_METHOD("init_region", "regionPos"), &WorldGeneration::init_region);
    ClassDB::bind_method(D_METHOD("drop_item", "pos", "item_id", "amount"), &WorldGeneration::drop_item);
    ClassDB::bind_method(D_METHOD("get_items_at", "pos"), &WorldGeneration::get_items_at);
    ClassDB::bind_method(D_METHOD("pickup_item_specific", "pos", "item_id", "amount", "inventory"), &WorldGeneration::pickup_item_specific);
    ClassDB::bind_method(D_METHOD("has_item", "pos"), &WorldGeneration::has_item);
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
    FastTileMap::set_world_seed(seed);
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

void WorldGeneration::apply_auto_tiling(const Vector2i& p_region_pos) {
    ChunkDb* chunk_db = ChunkDb::get_singleton();
    TagRegistry* tag_reg = TagRegistry::get_singleton();
    uint16_t road_tag_id = tag_reg ? tag_reg->get_tag_id("ROAD") : 0;

    // Use a grid for O(1) lookups during this pass
    std::vector<uint16_t> grid(REGION_SIZE * REGION_SIZE, 0);
    std::vector<uint64_t> chunk_keys;
    chunk_keys.reserve(region_chunks.size());

    for (auto& pair : region_chunks) {
        chunk_keys.push_back(pair.first);
        Vector2i pos = unpack_coords(pair.first);
        
        // Only interested in chunks within the current region for the grid
        int rel_x = pos.x - p_region_pos.x * REGION_SIZE;
        int rel_y = pos.y - p_region_pos.y * REGION_SIZE;
        
        if (rel_x >= 0 && rel_x < REGION_SIZE && rel_y >= 0 && rel_y < REGION_SIZE) {
            grid[rel_y * REGION_SIZE + rel_x] = static_cast<uint16_t>(pair.second & ID_MASK);
        }
    }

    for (uint64_t key : chunk_keys) {
        uint32_t packed = region_chunks[key];
        uint16_t chunk_id = static_cast<uint16_t>(packed & ID_MASK);

        auto it_rule = biome_rules.find(chunk_id);
        if (it_rule == biome_rules.end() || !it_rule->second.auto_tiled) {
            continue;
        }

        Vector2i pos = unpack_coords(key);
        int rel_x = pos.x - p_region_pos.x * REGION_SIZE;
        int rel_y = pos.y - p_region_pos.y * REGION_SIZE;

        uint32_t mask = 0;
        bool current_is_road = (chunk_db && road_tag_id != 0) ? chunk_db->has_tag(chunk_id, road_tag_id) : false;

        auto get_grid_id = [&](int nx, int ny) -> uint16_t {
            if (nx < 0 || nx >= REGION_SIZE || ny < 0 || ny >= REGION_SIZE) {
                // Fallback to region_chunks for out-of-bounds
                int gx = p_region_pos.x * REGION_SIZE + nx;
                int gy = p_region_pos.y * REGION_SIZE + ny;
                uint64_t n_key = pack_coords(gx, gy);
                auto n_it = region_chunks.find(n_key);
                return (n_it != region_chunks.end()) ? static_cast<uint16_t>(n_it->second & ID_MASK) : 0;
            }
            return grid[ny * REGION_SIZE + nx];
        };

        auto check_neighbor = [&](int dx, int dy, NeighborBits bit) {
            uint16_t n_id = get_grid_id(rel_x + dx, rel_y + dy);
            if (n_id != 0) {
                bool neighbor_connects = false;
                if (current_is_road && chunk_db && road_tag_id != 0) {
                    if (chunk_db->has_tag(n_id, road_tag_id)) {
                        neighbor_connects = true;
                    }
                }
                
                if (!neighbor_connects && n_id == chunk_id) {
                    neighbor_connects = true;
                }

                if (neighbor_connects) {
                    mask |= bit;
                }
            }
        };

        check_neighbor(0, -1, NEIGH_NORTH);
        check_neighbor(1, 0, NEIGH_EAST);
        check_neighbor(0, 1, NEIGH_SOUTH);
        check_neighbor(-1, 0, NEIGH_WEST);

        region_chunks[key] = (packed & ~(NEIGHBOR_MASK << NEIGHBOR_SHIFT)) | (mask << NEIGHBOR_SHIFT);
    }
}

void WorldGeneration::setup_biome_rules() {
    if (!biome_rules.empty()) return; // Already setup

    id_reg = IdRegistry::get_singleton();
    s_db = StructureDb::get_singleton();
    if (!id_reg) return;

    id_void = id_reg->register_string("void");
    id_building = id_reg->register_string("building");
    id_forest = id_reg->register_string("forest");
    id_plains = id_reg->register_string("plains");

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
        {"grass", 80}, {"dirt", 20}
    });

    // 2. Forest
    reg_biome("forest", {
        {"tree", 30}, {"grass", 56}, {"dirt", 14}
    });

    // 3. Buildings (Default ground)
    reg_biome("building", {
        {"grass", 80}, {"dirt", 20}
    });

    // 4. Roads/Alleys/Floors (Fixed Overrides)
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

uint16_t WorldGeneration::get_tile(int x, int y) {
    int cx = (x >= 0) ? (x / CHUNK_SIZE) : ((x - (CHUNK_SIZE - 1)) / CHUNK_SIZE);
    int cy = (y >= 0) ? (y / CHUNK_SIZE) : ((y - (CHUNK_SIZE - 1)) / CHUNK_SIZE);
    uint64_t chunk_key = Occlusion::pack_coords(cx, cy);

    if (!last_chunk_valid || last_chunk_key != chunk_key) {
        auto it = region_chunks.find(chunk_key);
        if (it == region_chunks.end()) {
            last_chunk_valid = false;
            return id_void;
        }
        
        uint32_t packed = it->second;
        last_chunk_id = static_cast<uint16_t>(packed & ID_MASK);
        last_chunk_rotation = static_cast<uint8_t>(packed >> ORIENTATION_SHIFT);
        last_chunk_neighbors = static_cast<uint8_t>((packed >> NEIGHBOR_SHIFT) & NEIGHBOR_MASK);
        last_chunk_key = chunk_key;
        
        auto it_rule = biome_rules.find(last_chunk_id);
        last_biome_ptr = (it_rule != biome_rules.end()) ? &it_rule->second : nullptr;
        
        last_chunk_valid = true;
    }

    const uint16_t chunk_id = last_chunk_id;

    // 0. Auto-Tiling Path (Fast Border Check)
    if (last_biome_ptr && last_biome_ptr->auto_tiled) {
        int lx = x % CHUNK_SIZE; if (lx < 0) lx += CHUNK_SIZE;
        int ly = y % CHUNK_SIZE; if (ly < 0) ly += CHUNK_SIZE;

        bool west = lx < 3;
        bool east = lx >= CHUNK_SIZE - 3;
        bool north = ly < 3;
        bool south = ly >= CHUNK_SIZE - 3;

        // Corners (3x3) are always borders for road/alley chunks
        if ((west || east) && (north || south)) {
            return last_biome_ptr->border_tile_id;
        }

        // Edges are borders if the corresponding cardinal neighbor is missing
        if ((west && !(last_chunk_neighbors & NEIGH_WEST)) ||
            (east && !(last_chunk_neighbors & NEIGH_EAST)) ||
            (north && !(last_chunk_neighbors & NEIGH_NORTH)) ||
            (south && !(last_chunk_neighbors & NEIGH_SOUTH))) {
            return last_biome_ptr->border_tile_id;
        }
    }

    // 1. Structure Lookup Path (Hot Path)
    if (chunk_id == id_building && s_db) {
        int lx = x % CHUNK_SIZE; if (lx < 0) lx += CHUNK_SIZE;
        int ly = y % CHUNK_SIZE; if (ly < 0) ly += CHUNK_SIZE;
        
        int rx = lx, ry = ly;
        int max_coord = CHUNK_SIZE - 1;
        switch (last_chunk_rotation) {
            case ROT_WEST: rx = ly; ry = max_coord - lx; break; // 1
            case ROT_NORTH: rx = max_coord - lx; ry = max_coord - ly; break; // 2
            case ROT_EAST: rx = max_coord - ly; ry = lx; break; // 3
        }

        // Optimized StructureDb call
        uint16_t tile_id = s_db->get_tile_at("house01", rx, ry);
        if (tile_id != id_void) return tile_id;
    }

    // 2. Biome Logic Path (Using cached pointer)
    if (last_biome_ptr) {
        uint32_t h = get_hash(x, y, static_cast<uint32_t>(world_seed));
        return pick_weighted_tile(*last_biome_ptr, h % 100);
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
    ItemDb* item_db = ItemDb::get_singleton();
    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (!tile_db || !item_db || !id_reg) return;
    
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
        
        // Check for items first
        auto it_item = dropped_items.find(cellKey);
        if (it_item != dropped_items.end() && !it_item->second.empty()) {
            uint16_t item_id_numeric = it_item->second[0].id;
            const ItemInfo* info = item_db->get_item_info(item_id_numeric);
            
            if (info) {
                Vector2i atlas_pos;
                atlas_pos.x = 1 + info->atlas.x * (FastTileMap::get_tile_size() + 1);
                atlas_pos.y = 1 + info->atlas.y * (FastTileMap::get_tile_size() + 1);

                RID tile_rid = pair.second;
                rs->canvas_item_clear(tile_rid);
                rs->canvas_item_add_texture_rect_region(
                    tile_rid,
                    Rect2(ox * get_cell_size(), oy * get_cell_size(), FastTileMap::get_tile_size(), FastTileMap::get_tile_size()),
                    texture_rid,
                    Rect2(atlas_pos.x, atlas_pos.y, FastTileMap::get_tile_size(), FastTileMap::get_tile_size())
                );
                // Skip normal tile rendering
                continue;
            }
        }
        
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
        
        if (ignore_occlusion) {
            occluded = false;
        } else {
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
    setup_biome_rules();
    
    region_chunks.clear();
    last_chunk_valid = false;

    Canvas cityCanvas(REGION_SIZE);
    CityGeneration::spawn_city(cityCanvas, 127, 128, world_seed);

    Dictionary result;
    for (int y = 0; y < REGION_SIZE; y++) {
        for (int x = 0; x < REGION_SIZE; x++) {
            CityPixel pixel = cityCanvas.getPixel(x, y);
            uint16_t chunk_id = pixel.id;
            
            // Fallback to biome
            if (chunk_id == id_void) {
                int gx = regionPos.x * REGION_SIZE + x;
                int gy = regionPos.y * REGION_SIZE + y;
                uint32_t h = get_hash(gx, gy, static_cast<uint32_t>(world_seed));
                chunk_id = (h % 100 < 50) ? id_forest : id_plains;
            }

            uint8_t rot = pixel.meta & ROTATION_MASK;

            // Store the chunk type using packed coordinates relative to regionPos
            int gx = regionPos.x * REGION_SIZE + x;
            int gy = regionPos.y * REGION_SIZE + y;
            uint64_t key = Occlusion::pack_coords(gx, gy);

            // Pack rotation (8-bit) and chunk_id (16-bit) into 32-bit map value
            region_chunks[key] = (static_cast<uint32_t>(rot) << ORIENTATION_SHIFT) | chunk_id;
            result[key] = id_reg->get_string(chunk_id);
        }
    }

    apply_auto_tiling(regionPos);

    return result;
}

void WorldGeneration::drop_item(const Vector2i& pos, const String& item_id, int amount) {
    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (!id_reg) return;

    uint16_t id = id_reg->get_id(item_id);
    uint64_t key = Occlusion::pack_coords(pos.x, pos.y);
    
    // Stack items if they already exist
    auto& items = dropped_items[key];
    for (auto& item : items) {
        if (item.id == id) {
            item.amount += amount;
            return;
        }
    }
    
    items.push_back({id, amount});
}

Array WorldGeneration::get_items_at(const Vector2i& pos) const {
    Array list;
    uint64_t key = Occlusion::pack_coords(pos.x, pos.y);
    auto it = dropped_items.find(key);
    
    if (it != dropped_items.end()) {
        IdRegistry* id_reg = IdRegistry::get_singleton();
        for (const auto& item : it->second) {
            Dictionary d;
            d["id"] = id_reg ? id_reg->get_string(item.id) : String::num_int64(item.id);
            d["amount"] = item.amount;
            list.push_back(d);
        }
    }
    return list;
}

bool WorldGeneration::pickup_item_specific(const Vector2i& pos, const String& item_id, int amount, Inventory* p_inventory) {
    if (!p_inventory) return false;
    
    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (!id_reg) return false;
    uint16_t numeric_id = id_reg->get_id(item_id);

    uint64_t key = Occlusion::pack_coords(pos.x, pos.y);
    auto it = dropped_items.find(key);
    
    if (it != dropped_items.end()) {
        for (auto item_it = it->second.begin(); item_it != it->second.end(); ++item_it) {
            if (item_it->id == numeric_id) {
                int to_pickup = MIN(amount, item_it->amount);
                if (p_inventory->add_item_numeric(numeric_id, to_pickup)) {
                    item_it->amount -= to_pickup;
                    if (item_it->amount <= 0) {
                        it->second.erase(item_it);
                    }
                    if (it->second.empty()) {
                        dropped_items.erase(it);
                    }
                    return true;
                }
                return false;
            }
        }
    }
    
    return false;
}

bool WorldGeneration::has_item(const Vector2i& pos) const {
    uint64_t key = Occlusion::pack_coords(pos.x, pos.y);
    auto it = dropped_items.find(key);
    return it != dropped_items.end() && !it->second.empty();
}
