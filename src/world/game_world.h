#ifndef SPACETRAVELLER_GAME_WORLD_H
#define SPACETRAVELLER_GAME_WORLD_H

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/rect2i.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/color.hpp>
#include <memory>
#include "core/world_coords.h"
#include "data/item_db.h"
#include "data/race_db.h"
#include "fast_tilemap.h"
#include "world_bubble.h"
#include "world_generator.h"
#include "quest_tracker.h"
#include "world_spawn_state.h"
#include "path/a_star_grid.h"
#include "turn_scheduler.h"
#include "entities/entity_ledger.h"
#include "sim/simulation_event_sink.h"
#include "sim/simulation_director.h"

namespace godot {

class GameWorld : public Node2D, public ISimulationEventSink {
    GDCLASS(GameWorld, Node2D)

public:
    enum BubbleLayer {
        LAYER_TILE = WorldBubble::LAYER_TILE,
        LAYER_INDICATOR = WorldBubble::LAYER_INDICATOR,
        LAYER_MAX = WorldBubble::LAYER_MAX
    };

    // Intent type constants exposed to GDScript
    enum IntentConst {
        INTENT_NONE = 0,
        INTENT_MOVE = 1,
        INTENT_ATTACK = 2,
        INTENT_SMASH = 3,
        INTENT_PICKUP = 4
    };

private:
    FastTileMap* renderer = nullptr;
    WorldBubble bubble;
    std::unique_ptr<WorldGenerator> generator;
    std::unique_ptr<AStarGridPathfinder> pathfinder;
    std::unique_ptr<QuestTracker> quest_tracker;
    WorldSpawnState spawn_state;
    EntityLedger entity_ledger;
    SimulationDirector sim_director;

    Ref<FastNoiseLite> biome_noise;
    int world_seed = 0;

    static void _quest_updated_trampoline(void* userdata, const String& quest_id);

protected:
    static void _bind_methods();

public:
    GameWorld();
    ~GameWorld();

    void setup_renderer();
    FastTileMap* get_renderer() const { return renderer; }
    WorldBubble* get_bubble() { return &bubble; }
    const WorldBubble* get_bubble() const { return &bubble; }

    uint32_t player_entity_id = PLAYER_ENTITY_ID;
    TurnScheduler turn_scheduler;

    static int get_region_size() { return WorldCoords::REGION_SIZE; }
    static int get_chunk_size() { return WorldCoords::CHUNK_SIZE; }

    static uint64_t pack_coords(int x, int y) { return WorldCoords::pack_coords(x, y); }
    static Vector2i unpack_coords(uint64_t key) { return WorldCoords::unpack_coords(key); }

    bool is_player(uint32_t id) const { return id == player_entity_id; }

    void set_biome_noise(const Ref<FastNoiseLite>& noise);
    Ref<FastNoiseLite> get_biome_noise() const;
    void set_world_seed(int seed);
    int get_world_seed() const;

    void init_world_bubble(const Vector2i& player_pos, bool is_square = false);
    void update_world_bubble(const Vector2i& playerPos);
    Dictionary init_region(const Vector2i& regionPos);

    void place_tile(int x, int y, const String& tile_id, BubbleLayer p_layer = LAYER_TILE);
    String get_tile_at(int x, int y, BubbleLayer p_layer = LAYER_TILE) const;
    void fill_tiles(int x, int y, const String& tile_id, const Vector2i& player_pos, const Rect2i& mask = Rect2i(), bool invert_mask = false, bool contiguous = true, BubbleLayer p_layer = LAYER_TILE);
    void clear_cache(BubbleLayer p_layer = LAYER_TILE);
    void clear_all_caches();
    Dictionary get_tile_id_cache(BubbleLayer p_layer = LAYER_TILE) const;
    void set_tile_id_cache(const Dictionary& p_cache, BubbleLayer p_layer = LAYER_TILE);
    void merge_tile_id_cache(const Dictionary& p_cache, BubbleLayer p_layer = LAYER_TILE);
    Array get_seen_cells() const;
    void set_seen_cells(const Array& p_seen);
    void invalidate_tile_cache(int world_x, int world_y, BubbleLayer p_layer = LAYER_TILE);
    void invalidate_region_cache(const Rect2i& p_rect, BubbleLayer p_layer = LAYER_TILE);

    void drop_item(const Vector2i& pos, const String& item_id, int amount);
    Array get_items_at(const Vector2i& pos) const;
    bool pickup_item_specific(const Vector2i& pos, const String& item_id, int amount, uint32_t entity_id);
    bool has_item(const Vector2i& pos) const;

    Array generate_quest_offers(int giver_entity_id, int count);
    Dictionary generate_quest_offer(int giver_entity_id, const String& kind);
    bool  accept_quest(const String& quest_id);
    bool  decline_quest(const String& quest_id);
    bool  can_complete_quest(const String& quest_id) const;
    bool  complete_quest(const String& quest_id);
    Array get_quest_offers_for_giver(int giver_entity_id) const;
    Array get_active_quests() const;
    Array get_completed_quests() const;
    Array get_offered_quests() const;
    Dictionary get_quest(const String& quest_id) const;
    bool is_quest_active(const String& quest_id) const;
    bool is_quest_completed(const String& quest_id) const;

    bool is_cell_seen(const Vector2i& pos) const;
    bool has_entity_at_cell(int x, int y) const;
    Array request_player_path(const Vector2i& start, const Vector2i& goal);
    Array find_path(const Vector2i& start, const Vector2i& goal);

    uint32_t spawn_player(int x, int y, const String& race_id);
    uint32_t spawn_entity(int x, int y, const String& race_id);
    void despawn_entity(uint32_t entity_id);
    Vector2i get_entity_position(uint32_t entity_id) const;
    Vector2i get_entity_chunk(uint32_t entity_id) const;
    Vector2i get_player_position() const;
    Vector2i get_player_chunk() const;
    float submit_player_intent(int intent_type, int target_x, int target_y, const String& param);

    bool add_entity_inventory_item(uint32_t entity_id, const String& item_id, int amount);
    bool remove_entity_inventory_item(uint32_t entity_id, const String& item_id, int amount);
    int get_entity_inventory_item_amount(uint32_t entity_id, const String& item_id) const;
    Dictionary get_entity_inventory(uint32_t entity_id) const;
    float get_entity_inventory_weight(uint32_t entity_id) const;
    float get_entity_inventory_volume(uint32_t entity_id) const;

    Dictionary get_entity_anatomy(uint32_t entity_id) const;
    String get_entity_gender(uint32_t entity_id) const;
    bool entity_has_sapient(uint32_t entity_id) const;
    int get_entity_friendship(uint32_t entity_id) const;
    int get_entity_romance(uint32_t entity_id) const;
    void set_entity_friendship(uint32_t entity_id, int value);
    void set_entity_romance(uint32_t entity_id, int value);
    int get_entity_social_cooldown(uint32_t entity_id) const;
    void set_entity_social_cooldown(uint32_t entity_id, int turn);
    Dictionary get_entity_social_state(uint32_t entity_id) const;
    void set_entity_social_state(uint32_t entity_id, const Dictionary& state);
    void clear_entity_social_state(uint32_t entity_id);
    String get_entity_name(uint32_t entity_id) const;
    Dictionary get_entity_clothing(uint32_t entity_id) const;
    String get_entity_anatomy_part_name(uint32_t entity_id, int part_index) const;
    bool equip_entity_clothing(uint32_t entity_id, int part_index, const String& item_id, const String& layer);
    bool unequip_entity_clothing(uint32_t entity_id, const String& item_id);
    float get_entity_armor_rating(uint32_t entity_id) const;
    bool unequip_entity_clothing_by_string(uint32_t entity_id, const String& item_id);

    void process_game_turn(float current_time);

    void on_entity_moved(uint32_t entity_id, const Vector2i& new_pos, const Vector2i& new_chunk) override;
    void on_entity_died(uint32_t entity_id, const String& cause) override;
    void on_player_turn_ready(uint32_t entity_id) override;
    void on_player_action_resolved(uint32_t entity_id, float cost, float next_turn_time) override;
    void on_combat_event(uint32_t attacker_id, uint32_t defender_id, float damage, const String& result, const String& verb, const String& part) override;
    void on_smash_event(uint32_t entity_id, const String& tile_id, const String& result) override;
    void on_effect_event(uint32_t entity_id, const String& effect_type, const String& note, const String& part) override;
    void on_interact_event(uint32_t entity_id, uint32_t target_id) override;
    void on_player_died(const String& cause) override;

    Dictionary get_entity_health(uint32_t entity_id) const;
    Dictionary get_player_health() const;
    Dictionary get_entity_stamina(uint32_t entity_id) const;
    Dictionary get_player_stamina() const;
    Dictionary get_entity_effects(uint32_t entity_id) const;
    Dictionary get_player_effects() const;
    Dictionary get_entity_social_profile(uint32_t entity_id) const;

    void add_overlay(int x, int y, int atlas_x, int atlas_y, const Color& color, float lifetime);
    void remove_overlay(int x, int y);
    void clear_overlays();

    Dictionary get_save_data() const;

    void load_save_data(const Dictionary &p_data);

    void sync_entity_streaming(const Vector2i& player_pos);

private:
    Array find_path_with_flags(const Vector2i& start, const Vector2i& goal, uint32_t flags);
};

}

VARIANT_ENUM_CAST(GameWorld::BubbleLayer);
VARIANT_ENUM_CAST(GameWorld::IntentConst);

#endif // ! SPACETRAVELLER_GAME_WORLD_H
