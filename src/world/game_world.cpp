#include "game_world.h"
#include "path/path_request.h"
#include "path/path_result.h"
#include "data/structure_db.h"
#include "data/tile_db.h"
#include "data/quest_db.h"
#include "components/action_resolver.h"
#include "world_spawner.h"
#include "entity_lifecycle.h"
#include "world_save_serializer.h"
#include "core/id_registry.h"
#include "entities/entity_factory.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/json.hpp>
#include <vector>

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
    BIND_ENUM_CONSTANT(INTENT_CLOSE);
    BIND_ENUM_CONSTANT(INTENT_OPEN);

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

    ClassDB::bind_method(D_METHOD("add_entity_inventory_item", "entity_id", "item_id", "amount"), &GameWorld::add_entity_inventory_item);
    ClassDB::bind_method(D_METHOD("remove_entity_inventory_item", "entity_id", "item_id", "amount"), &GameWorld::remove_entity_inventory_item);
    ClassDB::bind_method(D_METHOD("get_entity_inventory_item_amount", "entity_id", "item_id"), &GameWorld::get_entity_inventory_item_amount);
    ClassDB::bind_method(D_METHOD("get_entity_inventory", "entity_id"), &GameWorld::get_entity_inventory);
    ClassDB::bind_method(D_METHOD("get_entity_equipment", "entity_id"), &GameWorld::get_entity_equipment);
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

    ClassDB::bind_method(D_METHOD("spawn_entity", "x", "y", "race_id"), &GameWorld::spawn_entity);
    ClassDB::bind_method(D_METHOD("despawn_entity", "entity_id"), &GameWorld::despawn_entity);

    ClassDB::bind_method(D_METHOD("get_entity_anatomy", "entity_id"), &GameWorld::get_entity_anatomy);
    ClassDB::bind_method(D_METHOD("get_entity_gender", "entity_id"), &GameWorld::get_entity_gender);
    ClassDB::bind_method(D_METHOD("entity_has_sapient", "entity_id"), &GameWorld::entity_has_sapient);
    ClassDB::bind_method(D_METHOD("get_entity_friendship", "entity_id"), &GameWorld::get_entity_friendship);
    ClassDB::bind_method(D_METHOD("get_entity_romance", "entity_id"), &GameWorld::get_entity_romance);
    ClassDB::bind_method(D_METHOD("set_entity_friendship", "entity_id", "value"), &GameWorld::set_entity_friendship);
    ClassDB::bind_method(D_METHOD("set_entity_romance", "entity_id", "value"), &GameWorld::set_entity_romance);
    ClassDB::bind_method(D_METHOD("get_entity_social_cooldown", "entity_id"), &GameWorld::get_entity_social_cooldown);
    ClassDB::bind_method(D_METHOD("set_entity_social_cooldown", "entity_id", "turn"), &GameWorld::set_entity_social_cooldown);
    ClassDB::bind_method(D_METHOD("get_entity_social_state", "entity_id"), &GameWorld::get_entity_social_state);
    ClassDB::bind_method(D_METHOD("set_entity_social_state", "entity_id", "state"), &GameWorld::set_entity_social_state);
    ClassDB::bind_method(D_METHOD("clear_entity_social_state", "entity_id"), &GameWorld::clear_entity_social_state);
    ClassDB::bind_method(D_METHOD("get_entity_name", "entity_id"), &GameWorld::get_entity_name);
    ClassDB::bind_method(D_METHOD("get_entity_clothing", "entity_id"), &GameWorld::get_entity_clothing);
    ClassDB::bind_method(D_METHOD("get_entity_anatomy_part_name", "entity_id", "part_index"), &GameWorld::get_entity_anatomy_part_name);
    ClassDB::bind_method(D_METHOD("equip_entity_clothing", "entity_id", "part_index", "item_id", "layer"), &GameWorld::equip_entity_clothing);
    ClassDB::bind_method(D_METHOD("unequip_entity_clothing", "entity_id", "item_id"), &GameWorld::unequip_entity_clothing);
    ClassDB::bind_method(D_METHOD("get_entity_armor_rating", "entity_id"), &GameWorld::get_entity_armor_rating);
    ClassDB::bind_method(D_METHOD("equip_entity_clothing_by_string", "entity_id", "item_id"), &GameWorld::equip_entity_clothing_by_string);
    ClassDB::bind_method(D_METHOD("unequip_entity_clothing_by_string", "entity_id", "item_id"), &GameWorld::unequip_entity_clothing_by_string);
    ClassDB::bind_method(D_METHOD("wield_entity_weapon", "entity_id", "slot_name", "item_id"), &GameWorld::wield_entity_weapon);
    ClassDB::bind_method(D_METHOD("unwield_entity_weapon", "entity_id", "slot_name"), &GameWorld::unwield_entity_weapon);
    ClassDB::bind_method(D_METHOD("wield_entity_weapon_by_string", "entity_id", "item_id"), &GameWorld::wield_entity_weapon_by_string);

    ClassDB::bind_method(D_METHOD("get_entity_health", "entity_id"), &GameWorld::get_entity_health);
    ClassDB::bind_method(D_METHOD("get_player_health"), &GameWorld::get_player_health);

    ClassDB::bind_method(D_METHOD("get_entity_stamina", "entity_id"), &GameWorld::get_entity_stamina);
    ClassDB::bind_method(D_METHOD("get_player_stamina"), &GameWorld::get_player_stamina);

    ClassDB::bind_method(D_METHOD("get_entity_effects", "entity_id"), &GameWorld::get_entity_effects);
    ClassDB::bind_method(D_METHOD("get_player_effects"), &GameWorld::get_player_effects);
    ClassDB::bind_method(D_METHOD("get_entity_social_profile", "entity_id"), &GameWorld::get_entity_social_profile);

    ClassDB::bind_method(D_METHOD("add_overlay", "x", "y", "atlas_x", "atlas_y", "color", "lifetime"), &GameWorld::add_overlay, DEFVAL(-1.0f));
    ClassDB::bind_method(D_METHOD("remove_overlay", "x", "y"), &GameWorld::remove_overlay);
    ClassDB::bind_method(D_METHOD("clear_overlays"), &GameWorld::clear_overlays);

    ClassDB::bind_method(D_METHOD("generate_quest_offers", "giver_entity_id", "count"), &GameWorld::generate_quest_offers, DEFVAL(1));
    ClassDB::bind_method(D_METHOD("generate_quest_offer", "giver_entity_id", "kind"), &GameWorld::generate_quest_offer);
    ClassDB::bind_method(D_METHOD("accept_quest", "quest_id"), &GameWorld::accept_quest);
    ClassDB::bind_method(D_METHOD("decline_quest", "quest_id"), &GameWorld::decline_quest);
    ClassDB::bind_method(D_METHOD("can_complete_quest", "quest_id"), &GameWorld::can_complete_quest);
    ClassDB::bind_method(D_METHOD("complete_quest", "quest_id"), &GameWorld::complete_quest);
    ClassDB::bind_method(D_METHOD("get_quest_offers_for_giver", "giver_entity_id"), &GameWorld::get_quest_offers_for_giver);
    ClassDB::bind_method(D_METHOD("get_active_quests"), &GameWorld::get_active_quests);
    ClassDB::bind_method(D_METHOD("get_completed_quests"), &GameWorld::get_completed_quests);
    ClassDB::bind_method(D_METHOD("get_offered_quests"), &GameWorld::get_offered_quests);
    ClassDB::bind_method(D_METHOD("get_quest", "quest_id"), &GameWorld::get_quest);
    ClassDB::bind_method(D_METHOD("is_quest_active", "quest_id"), &GameWorld::is_quest_active);
    ClassDB::bind_method(D_METHOD("is_quest_completed", "quest_id"), &GameWorld::is_quest_completed);

    ADD_SIGNAL(MethodInfo("quest_updated",
        PropertyInfo(Variant::STRING, "quest_id")));

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
        PropertyInfo(Variant::STRING, "result"),
        PropertyInfo(Variant::STRING, "verb"),
        PropertyInfo(Variant::STRING, "part")));
    ADD_SIGNAL(MethodInfo("player_died",
        PropertyInfo(Variant::STRING, "cause")));
    ADD_SIGNAL(MethodInfo("smash_event",
        PropertyInfo(Variant::INT, "entity_id"),
        PropertyInfo(Variant::STRING, "tile_id"),
        PropertyInfo(Variant::STRING, "result")));
    ADD_SIGNAL(MethodInfo("effect_event",
        PropertyInfo(Variant::INT, "entity_id"),
        PropertyInfo(Variant::STRING, "effect_type"),
        PropertyInfo(Variant::STRING, "note"),
        PropertyInfo(Variant::STRING, "part")));
    ADD_SIGNAL(MethodInfo("interact_event",
        PropertyInfo(Variant::INT, "entity_id"),
        PropertyInfo(Variant::INT, "target_id")));
}

GameWorld::GameWorld() {
    generator = std::make_unique<WorldGenerator>();
    pathfinder = std::make_unique<AStarGridPathfinder>();
    quest_tracker = std::make_unique<QuestTracker>();
    bubble.set_entity_pool(&entity_ledger.get_entity_pool());

    quest_tracker->configure(&entity_ledger, QuestDb::get_singleton(), player_entity_id, &world_seed);
    quest_tracker->set_emit_callback(&GameWorld::_quest_updated_trampoline, this);

    SimulationDirectorDeps deps;
    deps.ledger = &entity_ledger;
    deps.bubble = &bubble;
    deps.pathfinder = pathfinder.get();
    deps.scheduler = &turn_scheduler;
    deps.sink = static_cast<ISimulationEventSink*>(this);
    deps.event_listener = quest_tracker.get();
    deps.player_entity_id = player_entity_id;
    deps.world_seed = &world_seed;
    sim_director.configure(deps);
}

void GameWorld::_quest_updated_trampoline(void* userdata, const String& quest_id) {
    if (!userdata) return;
    GameWorld* self = static_cast<GameWorld*>(userdata);
    if (self->is_inside_tree()) {
        self->emit_signal("quest_updated", quest_id);
    }
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
        std::vector<uint64_t> offset_keys = renderer->get_render_offset_keys();
        bubble.update_visibility(playerPos, offset_keys, renderer->is_occlusion_enabled());
        sync_entity_streaming(playerPos);
        renderer->update_visuals(playerPos);
        return;
    }
    sync_entity_streaming(playerPos);
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
    auto inv_it = entity_ledger.inventory_data.find(entity_id);
    if (inv_it == entity_ledger.inventory_data.end()) return false;

    PickupResult r = ActionResolver::resolve_pickup(
        entity_id, pos, item_id, amount, bubble, inv_it->second, quest_tracker.get());
    return r.success;
}

bool GameWorld::has_item(const Vector2i& pos) const {
    return bubble.has_items(pos);
}

Array GameWorld::generate_quest_offers(int giver_entity_id, int count) {
    if (!quest_tracker) return Array();
    return quest_tracker->generate_offers((uint32_t)giver_entity_id, count);
}

Dictionary GameWorld::generate_quest_offer(int giver_entity_id, const String& kind) {
    if (!quest_tracker) return Dictionary();
    return quest_tracker->generate_offer((uint32_t)giver_entity_id, kind);
}

bool GameWorld::accept_quest(const String& quest_id) {
    return quest_tracker ? quest_tracker->accept(quest_id) : false;
}

bool GameWorld::decline_quest(const String& quest_id) {
    return quest_tracker ? quest_tracker->decline(quest_id) : false;
}

bool GameWorld::can_complete_quest(const String& quest_id) const {
    return quest_tracker ? quest_tracker->can_complete(quest_id) : false;
}

bool GameWorld::complete_quest(const String& quest_id) {
    return quest_tracker ? quest_tracker->complete(quest_id) : false;
}

Array GameWorld::get_quest_offers_for_giver(int giver_entity_id) const {
    return quest_tracker ? quest_tracker->get_offers_for((uint32_t)giver_entity_id) : Array();
}

Array GameWorld::get_active_quests() const {
    return quest_tracker ? quest_tracker->get_active() : Array();
}

Array GameWorld::get_completed_quests() const {
    return quest_tracker ? quest_tracker->get_completed() : Array();
}

Array GameWorld::get_offered_quests() const {
    return quest_tracker ? quest_tracker->get_offered() : Array();
}

Dictionary GameWorld::get_quest(const String& quest_id) const {
    return quest_tracker ? quest_tracker->get_quest(quest_id) : Dictionary();
}

bool GameWorld::is_quest_active(const String& quest_id) const {
    if (!quest_tracker) return false;
    Dictionary q = quest_tracker->get_quest(quest_id);
    return String(q.get("status", "")) == "active";
}

bool GameWorld::is_quest_completed(const String& quest_id) const {
    if (!quest_tracker) return false;
    Dictionary q = quest_tracker->get_quest(quest_id);
    return String(q.get("status", "")) == "completed";
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
    return EntityFactory::create_player(race_id, Vector2i(x, y), entity_ledger, bubble, turn_scheduler);
}

uint32_t GameWorld::spawn_entity(int x, int y, const String& race_id) {
    float spawn_time = 0.0f;
    const Entity* player_e = entity_ledger.get_entity_pool().get_entity(player_entity_id);
    if (player_e) spawn_time = player_e->next_turn_time;
    EntityFactory::SpawnOverrides overrides;
    return EntityFactory::create_npc(race_id, Vector2i(x, y), world_seed, entity_ledger, bubble, turn_scheduler, overrides, spawn_time);
}

void GameWorld::despawn_entity(uint32_t entity_id) {
    if (entity_id == player_entity_id) return;
    EntityLifecycle::despawn_entity(
        entity_id,
        entity_ledger,
        bubble,
        turn_scheduler,
        static_cast<uint32_t>(world_seed),
        false
    );
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

Dictionary GameWorld::get_entity_equipment(uint32_t entity_id) const {
    return entity_ledger.get_equipment(entity_id);
}

float GameWorld::get_entity_inventory_weight(uint32_t entity_id) const {
    return entity_ledger.get_inventory_weight(entity_id);
}

float GameWorld::get_entity_inventory_volume(uint32_t entity_id) const {
    return entity_ledger.get_inventory_volume(entity_id);
}

Dictionary GameWorld::get_entity_anatomy(uint32_t entity_id) const {
    return entity_ledger.get_anatomy(entity_id);
}

String GameWorld::get_entity_gender(uint32_t entity_id) const {
    auto it = entity_ledger.gender.find(entity_id);
    if (it != entity_ledger.gender.end()) return it->second;
    return "";
}

bool GameWorld::entity_has_sapient(uint32_t entity_id) const {
    Dictionary anat = entity_ledger.get_anatomy(entity_id);
    if (anat.is_empty()) return false;
    String race_id = anat.get("race_id", "");
    RaceDb* race_db = RaceDb::get_singleton();
    if (!race_db) return false;
    return race_db->has_tag(race_id, "SAPIENT");
}

int GameWorld::get_entity_friendship(uint32_t entity_id) const {
    return entity_ledger.get_friendship(entity_id);
}

int GameWorld::get_entity_romance(uint32_t entity_id) const {
    return entity_ledger.get_romance(entity_id);
}

void GameWorld::set_entity_friendship(uint32_t entity_id, int value) {
    entity_ledger.set_friendship(entity_id, value);
}

void GameWorld::set_entity_romance(uint32_t entity_id, int value) {
    entity_ledger.set_romance(entity_id, value);
}

int GameWorld::get_entity_social_cooldown(uint32_t entity_id) const {
    return entity_ledger.get_social_cooldown(entity_id);
}

void GameWorld::set_entity_social_cooldown(uint32_t entity_id, int turn) {
    entity_ledger.set_social_cooldown(entity_id, turn);
}

Dictionary GameWorld::get_entity_social_state(uint32_t entity_id) const {
    String json = entity_ledger.get_social_state_json(entity_id);
    if (json.is_empty()) return Dictionary();
    Variant v = JSON::parse_string(json);
    if (v.get_type() != Variant::DICTIONARY) return Dictionary();
    return v;
}

void GameWorld::set_entity_social_state(uint32_t entity_id, const Dictionary& state) {
    String json = JSON::stringify(state);
    entity_ledger.set_social_state_json(entity_id, json);
}

void GameWorld::clear_entity_social_state(uint32_t entity_id) {
    entity_ledger.clear_social_state(entity_id);
}

String GameWorld::get_entity_name(uint32_t entity_id) const {
    auto it = entity_ledger.entity_name.find(entity_id);
    if (it != entity_ledger.entity_name.end()) return it->second;
    return "";
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

bool GameWorld::equip_entity_clothing_by_string(uint32_t entity_id, const String& item_id) {
    return entity_ledger.equip_clothing_by_string(entity_id, item_id);
}

bool GameWorld::unequip_entity_clothing_by_string(uint32_t entity_id, const String& item_id) {
    return entity_ledger.unequip_clothing_by_string(entity_id, item_id);
}

bool GameWorld::wield_entity_weapon(uint32_t entity_id, const String& slot_name, const String& item_id) {
    return entity_ledger.wield_weapon(entity_id, slot_name, item_id);
}

bool GameWorld::unwield_entity_weapon(uint32_t entity_id, const String& slot_name) {
    return entity_ledger.unwield_weapon(entity_id, slot_name);
}

bool GameWorld::wield_entity_weapon_by_string(uint32_t entity_id, const String& item_id) {
    return entity_ledger.wield_weapon_by_string(entity_id, item_id);
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

void GameWorld::on_combat_event(uint32_t attacker_id, uint32_t defender_id, float damage, const String& result, const String& verb, const String& part) {
    if (result != "miss") {
        Vector2i pos = get_entity_position(defender_id);
        bubble.add_overlay(pos.x, pos.y, 19, 0, Color(1, 1, 1, 1), 0.1f);
    }
    emit_signal("combat_event", attacker_id, defender_id, damage, result, verb, part);
}

void GameWorld::on_player_died(const String& cause) {
    emit_signal("player_died", cause);
}

void GameWorld::on_smash_event(uint32_t entity_id, const String& tile_id, const String& result) {
    emit_signal("smash_event", entity_id, tile_id, result);
}

void GameWorld::on_effect_event(uint32_t entity_id, const String& effect_type, const String& note, const String& part) {
    emit_signal("effect_event", entity_id, effect_type, note, part);
}

void GameWorld::on_interact_event(uint32_t entity_id, uint32_t target_id) {
    emit_signal("interact_event", entity_id, target_id);
}

Dictionary GameWorld::get_entity_health(uint32_t entity_id) const {
    return entity_ledger.get_health(entity_id);
}

Dictionary GameWorld::get_player_health() const {
    return get_entity_health(player_entity_id);
}

Dictionary GameWorld::get_entity_stamina(uint32_t entity_id) const {
    return entity_ledger.get_stamina(entity_id);
}

Dictionary GameWorld::get_player_stamina() const {
    return get_entity_stamina(player_entity_id);
}

Dictionary GameWorld::get_entity_effects(uint32_t entity_id) const {
    return entity_ledger.get_effects(entity_id);
}

Dictionary GameWorld::get_player_effects() const {
    return get_entity_effects(player_entity_id);
}

Dictionary GameWorld::get_entity_social_profile(uint32_t entity_id) const {
    return entity_ledger.get_social_profile(entity_id);
}

void GameWorld::add_overlay(int x, int y, int atlas_x, int atlas_y, const Color& color, float lifetime) {
    bubble.add_overlay(x, y, (uint16_t)atlas_x, (uint16_t)atlas_y, color, lifetime);
}

void GameWorld::remove_overlay(int x, int y) {
    bubble.remove_overlay(x, y);
}

void GameWorld::clear_overlays() {
    bubble.clear_overlays();
}

void GameWorld::sync_entity_streaming(const Vector2i& player_pos) {
    int radius = bubble.get_world_bubble_radius();

    std::vector<uint32_t> to_freeze;
    for (uint32_t id : entity_ledger.get_entity_pool().get_live_ids()) {
        const Entity* entity = entity_ledger.get_entity_pool().get_entity(id);
        if (!entity) continue;
        if (id == player_entity_id) continue;
        if (abs(entity->x - player_pos.x) > radius || abs(entity->y - player_pos.y) > radius) {
            to_freeze.push_back(id);
        }
    }

    for (uint32_t id : to_freeze) {
        EntityLifecycle::freeze_entity(id, entity_archive, entity_ledger, bubble, turn_scheduler);
    }

    std::vector<uint64_t> thaw_keys = entity_archive.get_frozen_keys_in_range(player_pos, radius);
    float player_time = 0.0f;
    const Entity* player_e = entity_ledger.get_entity_pool().get_entity(player_entity_id);
    if (player_e) player_time = player_e->next_turn_time;

    for (uint64_t key : thaw_keys) {
        EntityLifecycle::thaw_entity(key, entity_archive, entity_ledger, bubble, turn_scheduler, player_time);
    }

    std::vector<uint64_t> newly_seen = bubble.consume_newly_seen_cells();
    WorldSpawner::spawn_for_newly_seen_cells(
        static_cast<uint32_t>(world_seed),
        player_time,
        newly_seen,
        *generator,
        bubble,
        entity_archive,
        entity_ledger,
        turn_scheduler,
        spawn_state
    );
}

// --- Save / Load ---

Dictionary GameWorld::get_save_data() const {
    return WorldSaveSerializer::build_save_data(
        world_seed,
        *generator,
        bubble,
        entity_ledger,
        entity_archive,
        spawn_state,
        quest_tracker.get()
    );
}

void GameWorld::load_save_data(const Dictionary &p_data) {
    WorldSaveSerializer::load_save_data(
        p_data,
        world_seed,
        *generator,
        bubble,
        entity_ledger,
        entity_archive,
        spawn_state,
        quest_tracker.get()
    );

    bubble.rebuild_from_pool();

    // Rebuild turn scheduler from loaded entities
    turn_scheduler.clear();
    for (uint32_t id : entity_ledger.get_entity_pool().get_live_ids()) {
        const Entity* entity = entity_ledger.get_entity_pool().get_entity(id);
        if (!entity) continue;
        if (entity_ledger.is_schedulable_actor(id)) {
            turn_scheduler.push(id, entity->next_turn_time);
        } else if (id == player_entity_id || entity_ledger.ai_data.count(id) > 0) {
            UtilityFunctions::printerr(
                String("[GameWorld] Loaded entity ")
                + String::num_int64(id)
                + String(" has incomplete actor components and was not scheduled.")
            );
        }
    }

    if (renderer) {
        renderer->set_world_seed(world_seed);
    }
}
