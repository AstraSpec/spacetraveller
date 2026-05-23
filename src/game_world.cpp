#include "game_world.h"
#include "data/structure_db.h"
#include "core/id_registry.h"

using namespace godot;

void GameWorld::_bind_methods() {
    // Property bindings
    ClassDB::bind_method(D_METHOD("set_biome_noise", "noise"), &GameWorld::set_biome_noise);
    ClassDB::bind_method(D_METHOD("get_biome_noise"), &GameWorld::get_biome_noise);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "biome_noise", PROPERTY_HINT_RESOURCE_TYPE, "FastNoiseLite"), "set_biome_noise", "get_biome_noise");
    
    ClassDB::bind_method(D_METHOD("set_world_seed", "seed"), &GameWorld::set_world_seed);
    ClassDB::bind_method(D_METHOD("get_world_seed"), &GameWorld::get_world_seed);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "world_seed"), "set_world_seed", "get_world_seed");

    // Expose constants
    ClassDB::bind_static_method("GameWorld", D_METHOD("get_region_size"), &GameWorld::get_region_size);
    ClassDB::bind_static_method("GameWorld", D_METHOD("get_chunk_size"), &GameWorld::get_chunk_size);
    
    ClassDB::bind_static_method("GameWorld", D_METHOD("pack_coords", "x", "y"), &GameWorld::pack_coords);
    ClassDB::bind_static_method("GameWorld", D_METHOD("unpack_coords", "key"), &GameWorld::unpack_coords);

    ClassDB::bind_integer_constant(get_class_static(), "", "ROTATION_MASK", WorldCoords::ROTATION_MASK);
    ClassDB::bind_integer_constant(get_class_static(), "", "ORIENTATION_SHIFT", WorldCoords::ORIENTATION_SHIFT);
    ClassDB::bind_integer_constant(get_class_static(), "", "ID_MASK", WorldCoords::ID_MASK);

    ClassDB::bind_integer_constant(get_class_static(), "Rotation", "ROT_SOUTH", WorldCoords::ROT_SOUTH);
    ClassDB::bind_integer_constant(get_class_static(), "Rotation", "ROT_WEST", WorldCoords::ROT_WEST);
    ClassDB::bind_integer_constant(get_class_static(), "Rotation", "ROT_NORTH", WorldCoords::ROT_NORTH);
    ClassDB::bind_integer_constant(get_class_static(), "Rotation", "ROT_EAST", WorldCoords::ROT_EAST);

    // Method bindings
    ClassDB::bind_method(D_METHOD("setup_renderer"), &GameWorld::setup_renderer);
    ClassDB::bind_method(D_METHOD("get_renderer"), &GameWorld::get_renderer);
    ClassDB::bind_method(D_METHOD("update_world_bubble", "playerPos"), &GameWorld::update_world_bubble);
    ClassDB::bind_method(D_METHOD("init_region", "regionPos"), &GameWorld::init_region);
    ClassDB::bind_method(D_METHOD("drop_item", "pos", "item_id", "amount"), &GameWorld::drop_item);
    ClassDB::bind_method(D_METHOD("get_items_at", "pos"), &GameWorld::get_items_at);
    ClassDB::bind_method(D_METHOD("pickup_item_specific", "pos", "item_id", "amount", "inventory"), &GameWorld::pickup_item_specific);
    ClassDB::bind_method(D_METHOD("has_item", "pos"), &GameWorld::has_item);

    ClassDB::bind_method(D_METHOD("get_save_data"), &GameWorld::get_save_data);
    ClassDB::bind_method(D_METHOD("load_save_data", "data"), &GameWorld::load_save_data);
}

GameWorld::GameWorld() {
    cell_data = std::make_unique<CellData>();
    generator = std::make_unique<WorldGenerator>();
}

GameWorld::~GameWorld() = default;

void GameWorld::setup_renderer() {
    if (renderer) return;
    renderer = memnew(FastTileMap);
    renderer->set_name("Renderer");
    add_child(renderer);
    
    renderer->set_tile_source([this](int x, int y){ 
        return generator->get_tile(x, y, world_seed); 
    });
    renderer->set_cell_data(cell_data.get());
    renderer->set_occlusion_enabled(true);
}

// Property setters/getters
void GameWorld::set_biome_noise(const Ref<FastNoiseLite>& noise) {
    biome_noise = noise;
    if (biome_noise.is_valid()) {
        biome_noise->set_seed(world_seed);
    }
}

Ref<FastNoiseLite> GameWorld::get_biome_noise() const {
    return biome_noise;
}

void GameWorld::set_world_seed(int seed) {
    world_seed = seed;
    if (renderer) {
        renderer->set_world_seed(seed);
    }
    if (biome_noise.is_valid()) {
        biome_noise->set_seed(seed);
    }
    if (generator) {
        generator->invalidate_cache();
    }
}

int GameWorld::get_world_seed() const {
    return world_seed;
}

// Update world bubble - main loop
void GameWorld::update_world_bubble(const Vector2i& playerPos) {
    if (renderer) {
        renderer->update_visuals(playerPos);
    }
}

// Initialize world bubble
Dictionary GameWorld::init_region(const Vector2i& regionPos) {
    Dictionary result = generator->init_region(regionPos, world_seed, biome_noise);

    if (renderer) {
        renderer->invalidate_region_cache(Rect2i(regionPos.x * WorldCoords::REGION_SIZE, regionPos.y * WorldCoords::REGION_SIZE, WorldCoords::REGION_SIZE, WorldCoords::REGION_SIZE));
    }

    return result;
}

void GameWorld::drop_item(const Vector2i& pos, const String& item_id, int amount) {
    IdRegistry* reg = IdRegistry::get_singleton();
    if (!reg) return;

    uint64_t key = WorldCoords::pack_coords(pos.x, pos.y);
    cell_data->add_item(key, reg->get_id(item_id), amount);
}

Array GameWorld::get_items_at(const Vector2i& pos) const {
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

bool GameWorld::pickup_item_specific(const Vector2i& pos, const String& item_id, int amount, Inventory* p_inventory) {
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

bool GameWorld::has_item(const Vector2i& pos) const {
    uint64_t key = WorldCoords::pack_coords(pos.x, pos.y);
    return cell_data->has_items(key);
}

Dictionary GameWorld::get_save_data() const {
    Dictionary data;
    data["seed"] = world_seed;
    
    Dictionary chunks;
    const auto& region_chunks = generator->get_region_chunks();
    for (auto const& pair : region_chunks) {
        chunks[pair.first] = (int)pair.second;
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

void GameWorld::load_save_data(const Dictionary &p_data) {
    world_seed = p_data.get("seed", 0);
    
    std::unordered_map<uint64_t, uint32_t> region_chunks;
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
    generator->set_region_chunks(region_chunks);

    cell_data->deserialize(p_data.get("dropped_items", Dictionary()));

    if (renderer) {
        renderer->set_world_seed(world_seed);
        renderer->set_tile_id_cache(p_data.get("tile_id_cache", Dictionary()), FastTileMap::LAYER_TILE);
        renderer->set_seen_cells(p_data.get("seen_cells", Array()));
    }
}
