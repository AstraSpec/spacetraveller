#include "game_world.h"
#include "path/path_request.h"
#include "path/path_result.h"
#include "data/structure_db.h"
#include "core/id_registry.h"

using namespace godot;

void GameWorld::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_biome_noise", "noise"), &GameWorld::set_biome_noise);
    ClassDB::bind_method(D_METHOD("get_biome_noise"), &GameWorld::get_biome_noise);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "biome_noise", PROPERTY_HINT_RESOURCE_TYPE, "FastNoiseLite"), "set_biome_noise", "get_biome_noise");

    ClassDB::bind_method(D_METHOD("set_world_seed", "seed"), &GameWorld::set_world_seed);
    ClassDB::bind_method(D_METHOD("get_world_seed"), &GameWorld::get_world_seed);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "world_seed"), "set_world_seed", "get_world_seed");

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

    ClassDB::bind_method(D_METHOD("setup_renderer"), &GameWorld::setup_renderer);
    ClassDB::bind_method(D_METHOD("get_renderer"), &GameWorld::get_renderer);

    ClassDB::bind_method(D_METHOD("init_world_bubble", "player_pos", "is_square"), &GameWorld::init_world_bubble, DEFVAL(false));
    ClassDB::bind_method(D_METHOD("update_world_bubble", "playerPos"), &GameWorld::update_world_bubble);
    ClassDB::bind_method(D_METHOD("init_region", "regionPos"), &GameWorld::init_region);

    ClassDB::bind_method(D_METHOD("place_tile", "x", "y", "tile_id", "layer"), &GameWorld::place_tile, DEFVAL(LAYER_TILE));
    ClassDB::bind_method(D_METHOD("get_tile_at", "x", "y", "layer"), &GameWorld::get_tile_at, DEFVAL(LAYER_TILE));
    ClassDB::bind_method(D_METHOD("fill_tiles", "x", "y", "tile_id", "player_pos", "mask", "invert_mask", "contiguous", "layer"), &GameWorld::fill_tiles, DEFVAL(Rect2i()), DEFVAL(false), DEFVAL(true), DEFVAL(LAYER_TILE));
    ClassDB::bind_method(D_METHOD("clear_cache", "layer"), &GameWorld::clear_cache, DEFVAL(LAYER_TILE));
    ClassDB::bind_method(D_METHOD("clear_all_caches"), &GameWorld::clear_all_caches);
    ClassDB::bind_method(D_METHOD("get_tile_id_cache", "layer"), &GameWorld::get_tile_id_cache, DEFVAL(LAYER_TILE));
    ClassDB::bind_method(D_METHOD("set_tile_id_cache", "cache", "layer"), &GameWorld::set_tile_id_cache, DEFVAL(LAYER_TILE));
    ClassDB::bind_method(D_METHOD("merge_tile_id_cache", "cache", "layer"), &GameWorld::merge_tile_id_cache, DEFVAL(LAYER_TILE));
    ClassDB::bind_method(D_METHOD("get_seen_cells"), &GameWorld::get_seen_cells);
    ClassDB::bind_method(D_METHOD("set_seen_cells", "seen"), &GameWorld::set_seen_cells);
    ClassDB::bind_method(D_METHOD("invalidate_tile_cache", "world_x", "world_y", "layer"), &GameWorld::invalidate_tile_cache, DEFVAL(LAYER_TILE));
    ClassDB::bind_method(D_METHOD("invalidate_region_cache", "rect", "layer"), &GameWorld::invalidate_region_cache, DEFVAL(LAYER_TILE));

    BIND_ENUM_CONSTANT(LAYER_TILE);
    BIND_ENUM_CONSTANT(LAYER_INDICATOR);
    BIND_ENUM_CONSTANT(LAYER_MAX);
    ClassDB::bind_method(D_METHOD("drop_item", "pos", "item_id", "amount"), &GameWorld::drop_item);
    ClassDB::bind_method(D_METHOD("get_items_at", "pos"), &GameWorld::get_items_at);
    ClassDB::bind_method(D_METHOD("pickup_item_specific", "pos", "item_id", "amount", "inventory"), &GameWorld::pickup_item_specific);
    ClassDB::bind_method(D_METHOD("has_item", "pos"), &GameWorld::has_item);

    ClassDB::bind_method(D_METHOD("is_cell_seen", "pos"), &GameWorld::is_cell_seen);
    ClassDB::bind_method(D_METHOD("request_player_path", "start", "goal"), &GameWorld::request_player_path);
    ClassDB::bind_method(D_METHOD("find_path", "start", "goal"), &GameWorld::find_path);

    ClassDB::bind_method(D_METHOD("get_save_data"), &GameWorld::get_save_data);
    ClassDB::bind_method(D_METHOD("load_save_data", "data"), &GameWorld::load_save_data);
}

GameWorld::GameWorld() {
    generator = std::make_unique<WorldGenerator>();
    pathfinder = std::make_unique<AStarGridPathfinder>();
}

GameWorld::~GameWorld() = default;

void GameWorld::setup_renderer() {
    if (renderer) return;
    renderer = memnew(FastTileMap);
    renderer->set_name("Renderer");
    add_child(renderer);

    bubble.set_tile_source([this](int x, int y) {
        return generator->get_tile(x, y, world_seed);
    });
    renderer->set_bubble(&bubble);
    renderer->set_occlusion_enabled(true);
}

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

void GameWorld::init_world_bubble(const Vector2i& player_pos, bool is_square) {
    bubble.clear_all_caches();
    if (renderer) {
        bubble.set_world_bubble_radius(renderer->get_world_bubble_radius());
        renderer->init_world_bubble(player_pos, is_square);
    }
}

void GameWorld::update_world_bubble(const Vector2i& playerPos) {
    if (renderer) {
        renderer->update_visuals(playerPos);
    }
}

void GameWorld::place_tile(int x, int y, const String& tile_id, BubbleLayer p_layer) {
    bubble.place_tile(x, y, tile_id, (WorldBubble::Layer)p_layer);
}

String GameWorld::get_tile_at(int x, int y, BubbleLayer p_layer) const {
    return bubble.get_tile_at(x, y, (WorldBubble::Layer)p_layer);
}

void GameWorld::fill_tiles(int x, int y, const String& tile_id, const Vector2i& player_pos, const Rect2i& mask, bool invert_mask, bool contiguous, BubbleLayer p_layer) {
    bubble.fill_tiles(x, y, tile_id, player_pos, mask, invert_mask, contiguous, (WorldBubble::Layer)p_layer);
}

void GameWorld::clear_cache(BubbleLayer p_layer) {
    bubble.clear_cache((WorldBubble::Layer)p_layer);
}

void GameWorld::clear_all_caches() {
    bubble.clear_all_caches();
}

Dictionary GameWorld::get_tile_id_cache(BubbleLayer p_layer) const {
    return bubble.get_tile_id_cache((WorldBubble::Layer)p_layer);
}

void GameWorld::set_tile_id_cache(const Dictionary& p_cache, BubbleLayer p_layer) {
    bubble.set_tile_id_cache(p_cache, (WorldBubble::Layer)p_layer);
}

void GameWorld::merge_tile_id_cache(const Dictionary& p_cache, BubbleLayer p_layer) {
    bubble.merge_tile_id_cache(p_cache, (WorldBubble::Layer)p_layer);
}

Array GameWorld::get_seen_cells() const {
    return bubble.get_seen_cells();
}

void GameWorld::set_seen_cells(const Array& p_seen) {
    bubble.set_seen_cells(p_seen);
}

void GameWorld::invalidate_tile_cache(int world_x, int world_y, BubbleLayer p_layer) {
    bubble.invalidate_tile_cache(world_x, world_y, (WorldBubble::Layer)p_layer);
}

void GameWorld::invalidate_region_cache(const Rect2i& p_rect, BubbleLayer p_layer) {
    bubble.invalidate_region_cache(p_rect, (WorldBubble::Layer)p_layer);
}

Dictionary GameWorld::init_region(const Vector2i& regionPos) {
    Dictionary result = generator->init_region(regionPos, world_seed, biome_noise);

    bubble.invalidate_region_cache(Rect2i(
        regionPos.x * WorldCoords::REGION_SIZE,
        regionPos.y * WorldCoords::REGION_SIZE,
        WorldCoords::REGION_SIZE,
        WorldCoords::REGION_SIZE
    ));

    return result;
}

void GameWorld::drop_item(const Vector2i& pos, const String& item_id, int amount) {
    IdRegistry* reg = IdRegistry::get_singleton();
    if (!reg) return;
    bubble.drop_item(pos, reg->get_id(item_id), amount);
}

Array GameWorld::get_items_at(const Vector2i& pos) const {
    return bubble.get_items_at(pos);
}

bool GameWorld::pickup_item_specific(const Vector2i& pos, const String& item_id, int amount, Inventory* p_inventory) {
    if (!p_inventory) return false;

    IdRegistry* reg = IdRegistry::get_singleton();
    if (!reg) return false;

    uint16_t numeric_id = reg->get_id(item_id);
    int available = bubble.peek_item_amount(pos, numeric_id);
    if (available <= 0) return false;

    int to_pickup = MIN(amount, available);
    if (!p_inventory->add_item_numeric(numeric_id, to_pickup)) return false;

    bubble.remove_item(pos, numeric_id, to_pickup);
    return true;
}

bool GameWorld::has_item(const Vector2i& pos) const {
    return bubble.has_items(pos);
}

bool GameWorld::is_cell_seen(const Vector2i& pos) const {
    return bubble.is_cell_seen(pos.x, pos.y);
}

Array GameWorld::request_player_path(const Vector2i& start, const Vector2i& goal) {
    if (!bubble.is_cell_seen(goal.x, goal.y)) {
        return Array();
    }
    return find_path_with_flags(start, goal, PATH_FLAG_ALLOW_DIAGONAL);
}

Array GameWorld::find_path(const Vector2i& start, const Vector2i& goal) {
    return find_path_with_flags(start, goal, 0);
}

Array GameWorld::find_path_with_flags(const Vector2i& start, const Vector2i& goal, uint32_t flags) {
    if (!pathfinder) {
        return Array();
    }

    std::vector<Vector2i> blocking;
    blocking.push_back(start);

    TraversalSnapshot traversal = bubble.build_traversal_snapshot(start, goal, blocking);
    PathRequest request;
    request.start = start;
    request.goal = goal;
    request.flags = flags;

    PathResult result = pathfinder->find_path(request, traversal);
    return path_result_to_array(result);
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

    data["dropped_items"] = bubble.serialize_ground_items();
    data["tile_id_cache"] = bubble.get_tile_id_cache(WorldBubble::LAYER_TILE);
    data["seen_cells"] = bubble.get_seen_cells();

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

    bubble.deserialize_ground_items(p_data.get("dropped_items", Dictionary()));
    bubble.set_tile_id_cache(p_data.get("tile_id_cache", Dictionary()), WorldBubble::LAYER_TILE);
    bubble.set_seen_cells(p_data.get("seen_cells", Array()));

    if (renderer) {
        renderer->set_world_seed(world_seed);
    }
}
