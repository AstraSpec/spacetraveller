#include "world_generation.h"
#include "data/structure_db.h"
#include "core/id_registry.h"

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
    
    ClassDB::bind_static_method("WorldGeneration", D_METHOD("pack_coords", "x", "y"), &WorldGeneration::pack_coords);
    ClassDB::bind_static_method("WorldGeneration", D_METHOD("unpack_coords", "key"), &WorldGeneration::unpack_coords);

    ClassDB::bind_integer_constant(get_class_static(), "", "ROTATION_MASK", WorldCoords::ROTATION_MASK);
    ClassDB::bind_integer_constant(get_class_static(), "", "ORIENTATION_SHIFT", WorldCoords::ORIENTATION_SHIFT);
    ClassDB::bind_integer_constant(get_class_static(), "", "ID_MASK", WorldCoords::ID_MASK);

    ClassDB::bind_integer_constant(get_class_static(), "Rotation", "ROT_SOUTH", WorldCoords::ROT_SOUTH);
    ClassDB::bind_integer_constant(get_class_static(), "Rotation", "ROT_WEST", WorldCoords::ROT_WEST);
    ClassDB::bind_integer_constant(get_class_static(), "Rotation", "ROT_NORTH", WorldCoords::ROT_NORTH);
    ClassDB::bind_integer_constant(get_class_static(), "Rotation", "ROT_EAST", WorldCoords::ROT_EAST);

    // Method bindings
    ClassDB::bind_method(D_METHOD("setup_renderer"), &WorldGeneration::setup_renderer);
    ClassDB::bind_method(D_METHOD("get_renderer"), &WorldGeneration::get_renderer);
    ClassDB::bind_method(D_METHOD("update_world_bubble", "playerPos"), &WorldGeneration::update_world_bubble);
    ClassDB::bind_method(D_METHOD("init_region", "regionPos"), &WorldGeneration::init_region);
    ClassDB::bind_method(D_METHOD("drop_item", "pos", "item_id", "amount"), &WorldGeneration::drop_item);
    ClassDB::bind_method(D_METHOD("get_items_at", "pos"), &WorldGeneration::get_items_at);
    ClassDB::bind_method(D_METHOD("pickup_item_specific", "pos", "item_id", "amount", "inventory"), &WorldGeneration::pickup_item_specific);
    ClassDB::bind_method(D_METHOD("has_item", "pos"), &WorldGeneration::has_item);

    ClassDB::bind_method(D_METHOD("get_save_data"), &WorldGeneration::get_save_data);
    ClassDB::bind_method(D_METHOD("load_save_data", "data"), &WorldGeneration::load_save_data);
}

WorldGeneration::WorldGeneration() {
    cell_data = std::make_unique<CellData>();
}

WorldGeneration::~WorldGeneration() = default;

void WorldGeneration::setup_renderer() {
    if (renderer) return;
    renderer = memnew(FastTileMap);
    renderer->set_name("Renderer");
    add_child(renderer);
    
    renderer->set_tile_source([this](int x, int y){ return get_tile(x, y); });
    renderer->set_cell_data(cell_data.get());
    renderer->set_occlusion_enabled(true);
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
    if (renderer) {
        renderer->set_world_seed(seed);
    }
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
    std::vector<uint16_t> grid(WorldCoords::REGION_SIZE * WorldCoords::REGION_SIZE, 0);
    std::vector<uint64_t> chunk_keys;
    chunk_keys.reserve(region_chunks.size());

    for (auto& pair : region_chunks) {
        chunk_keys.push_back(pair.first);
        Vector2i pos = WorldCoords::unpack_coords(pair.first);
        
        // Only interested in chunks within the current region for the grid
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
        if (it_rule == biome_rules.end() || !it_rule->second.auto_tiled) {
            continue;
        }

        Vector2i pos = WorldCoords::unpack_coords(key);
        int rel_x = pos.x - p_region_pos.x * WorldCoords::REGION_SIZE;
        int rel_y = pos.y - p_region_pos.y * WorldCoords::REGION_SIZE;

        uint32_t mask = 0;
        bool current_is_road = (chunk_db && road_tag_id != 0) ? chunk_db->has_tag(chunk_id, road_tag_id) : false;

        auto get_grid_id = [&](int nx, int ny) -> uint16_t {
            if (nx < 0 || nx >= WorldCoords::REGION_SIZE || ny < 0 || ny >= WorldCoords::REGION_SIZE) {
                // Fallback to region_chunks for out-of-bounds
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

        check_neighbor(0, -1, WorldCoords::NEIGH_NORTH);
        check_neighbor(1, 0, WorldCoords::NEIGH_EAST);
        check_neighbor(0, 1, WorldCoords::NEIGH_SOUTH);
        check_neighbor(-1, 0, WorldCoords::NEIGH_WEST);

        region_chunks[key] = (packed & ~(WorldCoords::NEIGHBOR_MASK << WorldCoords::NEIGHBOR_SHIFT)) | (mask << WorldCoords::NEIGHBOR_SHIFT);
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

    // 0. Auto-Tiling Path (Fast Border Check)
    if (last_biome_ptr && last_biome_ptr->auto_tiled) {
        int lx = x % WorldCoords::CHUNK_SIZE; if (lx < 0) lx += WorldCoords::CHUNK_SIZE;
        int ly = y % WorldCoords::CHUNK_SIZE; if (ly < 0) ly += WorldCoords::CHUNK_SIZE;

        bool west = lx < 3;
        bool east = lx >= WorldCoords::CHUNK_SIZE - 3;
        bool north = ly < 3;
        bool south = ly >= WorldCoords::CHUNK_SIZE - 3;

        // Corners (3x3) are always borders for road/alley chunks
        if ((west || east) && (north || south)) {
            return last_biome_ptr->border_tile_id;
        }

        // Edges are borders if the corresponding cardinal neighbor is missing
        if ((west && !(last_chunk_neighbors & WorldCoords::NEIGH_WEST)) ||
            (east && !(last_chunk_neighbors & WorldCoords::NEIGH_EAST)) ||
            (north && !(last_chunk_neighbors & WorldCoords::NEIGH_NORTH)) ||
            (south && !(last_chunk_neighbors & WorldCoords::NEIGH_SOUTH))) {
            return last_biome_ptr->border_tile_id;
        }
    }

    // 1. Structure Lookup Path (Hot Path)
    if (chunk_id == id_building && s_db) {
        int lx = x % WorldCoords::CHUNK_SIZE; if (lx < 0) lx += WorldCoords::CHUNK_SIZE;
        int ly = y % WorldCoords::CHUNK_SIZE; if (ly < 0) ly += WorldCoords::CHUNK_SIZE;
        
        int rx = lx, ry = ly;
        int max_coord = WorldCoords::CHUNK_SIZE - 1;
        switch (last_chunk_rotation) {
            case WorldCoords::ROT_WEST: rx = ly; ry = max_coord - lx; break; // 1
            case WorldCoords::ROT_NORTH: rx = max_coord - lx; ry = max_coord - ly; break; // 2
            case WorldCoords::ROT_EAST: rx = max_coord - ly; ry = lx; break; // 3
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
    if (renderer) {
        renderer->update_visuals(playerPos);
    }
}

// Initialize world bubble
Dictionary WorldGeneration::init_region(const Vector2i& regionPos) {
    setup_biome_rules();
    
    region_chunks.clear();
    last_chunk_valid = false;

    Canvas cityCanvas(WorldCoords::REGION_SIZE);
    CityGeneration::spawn_city(cityCanvas, 127, 128, world_seed);

    Dictionary result;
    for (int y = 0; y < WorldCoords::REGION_SIZE; y++) {
        for (int x = 0; x < WorldCoords::REGION_SIZE; x++) {
            CityPixel pixel = cityCanvas.getPixel(x, y);
            uint16_t chunk_id = pixel.id;
            
            // Fallback to biome
            if (chunk_id == id_void) {
                int gx = regionPos.x * WorldCoords::REGION_SIZE + x;
                int gy = regionPos.y * WorldCoords::REGION_SIZE + y;
                uint32_t h = get_hash(gx, gy, static_cast<uint32_t>(world_seed));
                chunk_id = (h % 100 < 50) ? id_forest : id_plains;
            }

            uint8_t rot = pixel.meta & WorldCoords::ROTATION_MASK;

            // Store the chunk type using packed coordinates relative to regionPos
            int gx = regionPos.x * WorldCoords::REGION_SIZE + x;
            int gy = regionPos.y * WorldCoords::REGION_SIZE + y;
            uint64_t key = WorldCoords::pack_coords(gx, gy);

            // Pack rotation (8-bit) and chunk_id (16-bit) into 32-bit map value
            region_chunks[key] = (static_cast<uint32_t>(rot) << WorldCoords::ORIENTATION_SHIFT) | chunk_id;
            result[key] = id_reg->get_string(chunk_id);
        }
    }

    apply_auto_tiling(regionPos);

    if (renderer) {
        renderer->invalidate_region_cache(Rect2i(regionPos.x * WorldCoords::REGION_SIZE, regionPos.y * WorldCoords::REGION_SIZE, WorldCoords::REGION_SIZE, WorldCoords::REGION_SIZE));
    }

    return result;
}

void WorldGeneration::drop_item(const Vector2i& pos, const String& item_id, int amount) {
    IdRegistry* reg = IdRegistry::get_singleton();
    if (!reg) return;

    uint64_t key = WorldCoords::pack_coords(pos.x, pos.y);
    cell_data->add_item(key, reg->get_id(item_id), amount);
}

Array WorldGeneration::get_items_at(const Vector2i& pos) const {
    Array list;
    uint64_t key = WorldCoords::pack_coords(pos.x, pos.y);
    const std::vector<DroppedItem>* items = cell_data->get_items(key);
    if (!items) return list;

    IdRegistry* reg = IdRegistry::get_singleton();
    for (const auto& item : *items) {
        Dictionary d;
        d["id"] = reg ? reg->get_string(item.id) : String::num_int64(item.id);
        d["amount"] = item.amount;
        list.push_back(d);
    }
    return list;
}

bool WorldGeneration::pickup_item_specific(const Vector2i& pos, const String& item_id, int amount, Inventory* p_inventory) {
    if (!p_inventory) return false;

    IdRegistry* reg = IdRegistry::get_singleton();
    if (!reg) return false;
    uint16_t numeric_id = reg->get_id(item_id);

    uint64_t key = WorldCoords::pack_coords(pos.x, pos.y);
    int available = cell_data->peek_item_amount(key, numeric_id);
    if (available <= 0) return false;

    int to_pickup = MIN(amount, available);
    if (!p_inventory->add_item_numeric(numeric_id, to_pickup)) return false;

    cell_data->remove_item(key, numeric_id, to_pickup);
    return true;
}

bool WorldGeneration::has_item(const Vector2i& pos) const {
    uint64_t key = WorldCoords::pack_coords(pos.x, pos.y);
    return cell_data->has_items(key);
}

Dictionary WorldGeneration::get_save_data() const {
    Dictionary data;
    data["seed"] = world_seed;
    
    Dictionary chunks;
    for (auto const& [key, val] : region_chunks) {
        chunks[key] = (int)val;
    }
    data["region_chunks"] = chunks;

    data["dropped_items"] = cell_data->serialize();
    if (renderer) {
        data["tile_id_cache"] = renderer->get_tile_id_cache(FastTileMap::LAYER_TILE);
        data["seen_cells"] = renderer->get_seen_cells();
    } else {
        data["tile_id_cache"] = Dictionary();
        data["seen_cells"] = Array();
    }

    return data;
}

void WorldGeneration::load_save_data(const Dictionary &p_data) {
    world_seed = p_data.get("seed", 0);
    
    region_chunks.clear();
    Dictionary chunks = p_data.get("region_chunks", Dictionary());
    Array chunk_keys = chunks.keys();
    for (int i = 0; i < chunk_keys.size(); i++) {
        Variant key_var = chunk_keys[i];
        uint64_t key;
        if (key_var.get_type() == Variant::STRING) {
            key = ((String)key_var).to_int();
        } else {
            key = key_var;
        }
        region_chunks[key] = (uint32_t)((int)chunks[key_var]);
    }

    cell_data->deserialize(p_data.get("dropped_items", Dictionary()));

    if (renderer) {
        renderer->set_world_seed(world_seed);
        renderer->set_tile_id_cache(p_data.get("tile_id_cache", Dictionary()), FastTileMap::LAYER_TILE);
        renderer->set_seen_cells(p_data.get("seen_cells", Array()));
    }
    
    last_chunk_valid = false;
}
