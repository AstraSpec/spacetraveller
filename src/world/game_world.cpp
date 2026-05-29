#include "game_world.h"
#include "path/path_request.h"
#include "path/path_result.h"
#include "data/structure_db.h"
#include "data/tile_db.h"
#include "core/id_registry.h"
#include <godot_cpp/variant/utility_functions.hpp>

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

    BIND_ENUM_CONSTANT(INTENT_NONE);
    BIND_ENUM_CONSTANT(INTENT_MOVE);
    BIND_ENUM_CONSTANT(INTENT_ATTACK);
    BIND_ENUM_CONSTANT(INTENT_SMASH);
    BIND_ENUM_CONSTANT(INTENT_PICKUP);

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

    ClassDB::bind_method(D_METHOD("initialize_entity_inventory", "entity_id"), &GameWorld::initialize_entity_inventory);
    ClassDB::bind_method(D_METHOD("add_entity_inventory_item", "entity_id", "item_id", "amount"), &GameWorld::add_entity_inventory_item);
    ClassDB::bind_method(D_METHOD("remove_entity_inventory_item", "entity_id", "item_id", "amount"), &GameWorld::remove_entity_inventory_item);
    ClassDB::bind_method(D_METHOD("get_entity_inventory_item_amount", "entity_id", "item_id"), &GameWorld::get_entity_inventory_item_amount);
    ClassDB::bind_method(D_METHOD("get_entity_inventory", "entity_id"), &GameWorld::get_entity_inventory);
    ClassDB::bind_method(D_METHOD("get_entity_inventory_weight", "entity_id"), &GameWorld::get_entity_inventory_weight);
    ClassDB::bind_method(D_METHOD("get_entity_inventory_volume", "entity_id"), &GameWorld::get_entity_inventory_volume);

    ClassDB::bind_method(D_METHOD("drop_item", "pos", "item_id", "amount"), &GameWorld::drop_item);
    ClassDB::bind_method(D_METHOD("get_items_at", "pos"), &GameWorld::get_items_at);
    ClassDB::bind_method(D_METHOD("pickup_item_specific", "pos", "item_id", "amount", "entity_id"), &GameWorld::pickup_item_specific);
    ClassDB::bind_method(D_METHOD("has_item", "pos"), &GameWorld::has_item);

    ClassDB::bind_method(D_METHOD("is_cell_seen", "pos"), &GameWorld::is_cell_seen);
    ClassDB::bind_method(D_METHOD("has_entity_at_cell", "x", "y"), &GameWorld::has_entity_at_cell);
    ClassDB::bind_method(D_METHOD("request_player_path", "start", "goal"), &GameWorld::request_player_path);
    ClassDB::bind_method(D_METHOD("find_path", "start", "goal"), &GameWorld::find_path);

    ClassDB::bind_method(D_METHOD("get_save_data"), &GameWorld::get_save_data);
    ClassDB::bind_method(D_METHOD("load_save_data", "data"), &GameWorld::load_save_data);

    ClassDB::bind_method(D_METHOD("spawn_entity", "x", "y", "race_id", "ai_tier"), &GameWorld::spawn_entity, DEFVAL("raycast"));
    ClassDB::bind_method(D_METHOD("despawn_entity", "entity_id"), &GameWorld::despawn_entity);

    ClassDB::bind_method(D_METHOD("initialize_entity_anatomy", "entity_id", "race_id"), &GameWorld::initialize_entity_anatomy);
    ClassDB::bind_method(D_METHOD("get_entity_anatomy", "entity_id"), &GameWorld::get_entity_anatomy);
    ClassDB::bind_method(D_METHOD("get_entity_clothing", "entity_id"), &GameWorld::get_entity_clothing);
    ClassDB::bind_method(D_METHOD("get_entity_anatomy_part_name", "entity_id", "part_index"), &GameWorld::get_entity_anatomy_part_name);
    ClassDB::bind_method(D_METHOD("equip_entity_clothing", "entity_id", "part_index", "item_id", "layer"), &GameWorld::equip_entity_clothing);
    ClassDB::bind_method(D_METHOD("unequip_entity_clothing", "entity_id", "item_id"), &GameWorld::unequip_entity_clothing);
    ClassDB::bind_method(D_METHOD("get_entity_armor_rating", "entity_id"), &GameWorld::get_entity_armor_rating);
    ClassDB::bind_method(D_METHOD("unequip_entity_clothing_by_string", "entity_id", "item_id"), &GameWorld::unequip_entity_clothing_by_string);

    ClassDB::bind_method(D_METHOD("spawn_player", "x", "y", "race_id"), &GameWorld::spawn_player);
    ClassDB::bind_method(D_METHOD("get_entity_position", "entity_id"), &GameWorld::get_entity_position);
    ClassDB::bind_method(D_METHOD("get_entity_chunk", "entity_id"), &GameWorld::get_entity_chunk);
    ClassDB::bind_method(D_METHOD("get_player_position"), &GameWorld::get_player_position);
    ClassDB::bind_method(D_METHOD("get_player_chunk"), &GameWorld::get_player_chunk);
    ClassDB::bind_method(D_METHOD("submit_player_intent", "intent_type", "target_x", "target_y", "param"), &GameWorld::submit_player_intent);

    ADD_SIGNAL(MethodInfo("entity_moved",
        PropertyInfo(Variant::INT, "entity_id"),
        PropertyInfo(Variant::VECTOR2I, "new_pos"),
        PropertyInfo(Variant::VECTOR2I, "new_chunk")));
    ADD_SIGNAL(MethodInfo("entity_died",
        PropertyInfo(Variant::INT, "entity_id"),
        PropertyInfo(Variant::STRING, "cause")));
    ADD_SIGNAL(MethodInfo("player_turn_ready",
        PropertyInfo(Variant::INT, "entity_id")));
    ADD_SIGNAL(MethodInfo("player_action_resolved",
        PropertyInfo(Variant::INT, "entity_id"),
        PropertyInfo(Variant::FLOAT, "cost"),
        PropertyInfo(Variant::FLOAT, "next_turn_time")));
    ADD_SIGNAL(MethodInfo("combat_event",
        PropertyInfo(Variant::INT, "attacker_id"),
        PropertyInfo(Variant::INT, "defender_id"),
        PropertyInfo(Variant::FLOAT, "damage"),
        PropertyInfo(Variant::STRING, "result")));
}

GameWorld::GameWorld() {
    generator = std::make_unique<WorldGenerator>();
    pathfinder = std::make_unique<AStarGridPathfinder>();
    bubble.set_entity_pool(&entity_ledger.get_entity_pool());

    SimulationDirectorDeps deps;
    deps.ledger = &entity_ledger;
    deps.bubble = &bubble;
    deps.pathfinder = pathfinder.get();
    deps.scheduler = &turn_scheduler;
    deps.sink = static_cast<ISimulationEventSink*>(this);
    deps.player_entity_id = player_entity_id;
    sim_director.configure(deps);
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

bool GameWorld::pickup_item_specific(const Vector2i& pos, const String& item_id, int amount, uint32_t entity_id) {
    IdRegistry* reg = IdRegistry::get_singleton();
    if (!reg) return false;

    uint16_t numeric_id = reg->get_id(item_id);
    int available = bubble.peek_item_amount(pos, numeric_id);
    if (available <= 0) return false;

    int to_pickup = MIN(amount, available);

    auto inv_it = entity_ledger.inventory_data.find(entity_id);
    if (inv_it == entity_ledger.inventory_data.end()) return false;

    if (!Inventory::add_item(inv_it->second, numeric_id, to_pickup)) return false;

    bubble.remove_item(pos, numeric_id, to_pickup);
    return true;
}

bool GameWorld::has_item(const Vector2i& pos) const {
    return bubble.has_items(pos);
}

bool GameWorld::is_cell_seen(const Vector2i& pos) const {
    return bubble.is_cell_seen(pos.x, pos.y);
}

bool GameWorld::has_entity_at_cell(int x, int y) const {
    return bubble.get_entity_at(x, y) != nullptr;
}

Array GameWorld::request_player_path(const Vector2i& start, const Vector2i& goal) {
    return sim_director.request_player_path(start, goal);
}

Array GameWorld::find_path(const Vector2i& start, const Vector2i& goal) {
    return sim_director.find_path(start, goal);
}


uint32_t GameWorld::spawn_player(int x, int y, const String& race_id) {
    RaceDb* race_db = RaceDb::get_singleton();
    if (!race_db) return PLAYER_ENTITY_ID;

    Vector2i atlas = race_db->get_atlas_coords(race_id);
    uint32_t id = entity_ledger.spawn_player(Vector2i(x, y), atlas.x, atlas.y);

    // Register in bubble
    bubble.set_entity(x, y, id);

    LocomotionData& loco = entity_ledger.locomotion_data[id];
    Locomotion::init(loco, 1.0f);

    // Schedule the player's first turn so the scheduler state matches a loaded game
    Entity* entity = entity_ledger.get_entity_pool().get_entity(id);
    if (entity) {
        turn_scheduler.push(id, entity->next_turn_time);
    }

    return id;
}

uint32_t GameWorld::spawn_entity(int x, int y, const String& race_id, const String& ai_tier) {
    RaceDb* race_db = RaceDb::get_singleton();
    if (!race_db) return EntityPool::INVALID_ID;

    Vector2i atlas = race_db->get_atlas_coords(race_id);
    uint32_t id = entity_ledger.spawn_entity(Vector2i(x, y), atlas.x, atlas.y, race_id);

    bubble.set_entity(x, y, id);

    LocomotionData& loco = entity_ledger.locomotion_data[id];
    Locomotion::init(loco, 1.0f);

    AIData& ai = entity_ledger.ai_data[id];
    ai.state = AIState::WANDER;
    ai.wander_center = Vector2i(x, y);
    ai.wander_radius = 4.0f;
    ai.stuck_counter = 0;

    if (ai_tier == "full_occlusion") {
        ai.perception_tier = PerceptionTier::FULL_OCCLUSION;
    } else {
        ai.perception_tier = PerceptionTier::RAYCAST;
    }

    entity_ledger.perception_memory[id] = PerceptionMemory{};

    // Give a random starting item
    ItemDb* item_db = ItemDb::get_singleton();
    if (item_db) {
        Array item_ids = item_db->get_ids();
        if (item_ids.size() > 0) {
            int rand_idx = abs((int)(id * 7 + x * 13 + y * 31)) % item_ids.size();
            String random_item = item_ids[rand_idx];
            entity_ledger.add_inventory_item(id, random_item, 1);
        }
    }

    // Schedule first turn
    Entity* entity = entity_ledger.get_entity_pool().get_entity(id);
    if (entity) {
        turn_scheduler.push(id, entity->next_turn_time);
    }

    return id;
}

void GameWorld::despawn_entity(uint32_t entity_id) {
    if (entity_id == player_entity_id) return;

    Entity* entity = entity_ledger.get_entity_pool().get_entity(entity_id);
    if (entity) {
        bubble.remove_entity(entity->x, entity->y);
    }

    entity_ledger.destroy_entity(entity_id);
}

Vector2i GameWorld::get_entity_position(uint32_t entity_id) const {
    const Entity* e = entity_ledger.get_entity_pool().get_entity(entity_id);
    if (e) return Vector2i(e->x, e->y);
    return Vector2i();
}

Vector2i GameWorld::get_entity_chunk(uint32_t entity_id) const {
    const Entity* e = entity_ledger.get_entity_pool().get_entity(entity_id);
    if (e) {
        int cs = WorldCoords::CHUNK_SIZE;
        return Vector2i(floor((float)e->x / cs), floor((float)e->y / cs));
    }
    return Vector2i();
}

Vector2i GameWorld::get_player_position() const {
    return get_entity_position(player_entity_id);
}

Vector2i GameWorld::get_player_chunk() const {
    return get_entity_chunk(player_entity_id);
}

// --- Entity inventory wrappers ---

void GameWorld::initialize_entity_inventory(uint32_t entity_id) {
    entity_ledger.init_inventory(entity_id);
}

bool GameWorld::add_entity_inventory_item(uint32_t entity_id, const String& item_id, int amount) {
    return entity_ledger.add_inventory_item(entity_id, item_id, amount);
}

bool GameWorld::remove_entity_inventory_item(uint32_t entity_id, const String& item_id, int amount) {
    return entity_ledger.remove_inventory_item(entity_id, item_id, amount);
}

int GameWorld::get_entity_inventory_item_amount(uint32_t entity_id, const String& item_id) const {
    return entity_ledger.get_inventory_item_amount(entity_id, item_id);
}

Dictionary GameWorld::get_entity_inventory(uint32_t entity_id) const {
    return entity_ledger.get_inventory(entity_id);
}

float GameWorld::get_entity_inventory_weight(uint32_t entity_id) const {
    return entity_ledger.get_inventory_weight(entity_id);
}

float GameWorld::get_entity_inventory_volume(uint32_t entity_id) const {
    return entity_ledger.get_inventory_volume(entity_id);
}

void GameWorld::initialize_entity_anatomy(uint32_t entity_id, const String& race_id) {
    entity_ledger.init_anatomy(entity_id, race_id);
}

Dictionary GameWorld::get_entity_anatomy(uint32_t entity_id) const {
    return entity_ledger.get_anatomy(entity_id);
}

Dictionary GameWorld::get_entity_clothing(uint32_t entity_id) const {
    return entity_ledger.get_clothing(entity_id);
}

String GameWorld::get_entity_anatomy_part_name(uint32_t entity_id, int part_index) const {
    return entity_ledger.get_anatomy_part_name(entity_id, part_index);
}

bool GameWorld::equip_entity_clothing(uint32_t entity_id, int part_index, const String& item_id, const String& layer) {
    return entity_ledger.equip_clothing(entity_id, part_index, item_id, layer);
}

bool GameWorld::unequip_entity_clothing(uint32_t entity_id, const String& item_id) {
    return entity_ledger.unequip_clothing(entity_id, item_id);
}

float GameWorld::get_entity_armor_rating(uint32_t entity_id) const {
    return entity_ledger.get_armor_rating(entity_id);
}

bool GameWorld::unequip_entity_clothing_by_string(uint32_t entity_id, const String& item_id) {
    return entity_ledger.unequip_clothing_by_string(entity_id, item_id);
}

float GameWorld::submit_player_intent(int intent_type, int target_x, int target_y, const String& param) {
    return sim_director.submit_player_intent(intent_type, target_x, target_y, param);
}

void GameWorld::process_game_turn(float current_time) {
    sim_director.process_game_turn(current_time);
}

void GameWorld::on_entity_moved(uint32_t entity_id, const Vector2i& new_pos, const Vector2i& new_chunk) {
    emit_signal("entity_moved", entity_id, new_pos, new_chunk);
}

void GameWorld::on_entity_died(uint32_t entity_id, const String& cause) {
    emit_signal("entity_died", entity_id, cause);
}

void GameWorld::on_player_turn_ready(uint32_t entity_id) {
    emit_signal("player_turn_ready", entity_id);
}

void GameWorld::on_player_action_resolved(uint32_t entity_id, float cost, float next_turn_time) {
    emit_signal("player_action_resolved", entity_id, cost, next_turn_time);
}

void GameWorld::on_combat_event(uint32_t attacker_id, uint32_t defender_id, float damage, const String& result) {
    emit_signal("combat_event", attacker_id, defender_id, damage, result);
}

// --- Save / Load ---

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

    // Entity positions are not serialized; they are derived from the EntityPool
    // on load (the pool is the single source of truth for entity coordinates).

    // Delegate entity component data to ledger
    data["entity_ledger"] = entity_ledger.serialize();

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

    // Load entity data from ledger
    entity_ledger.deserialize(p_data.get("entity_ledger", Dictionary()));

    bubble.rebuild_from_pool();

    // Rebuild turn scheduler from loaded entities
    turn_scheduler.clear();
    for (const auto& entity : entity_ledger.get_entity_pool().get_all()) {
        if (entity_ledger.ai_data.count(entity.id) > 0 || entity.id == player_entity_id) {
            turn_scheduler.push(entity.id, entity.next_turn_time);
        }
    }

    if (renderer) {
        renderer->set_world_seed(world_seed);
    }
}
