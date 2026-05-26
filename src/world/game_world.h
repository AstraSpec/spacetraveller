#ifndef SPACETRAVELLER_GAME_WORLD_H
#define SPACETRAVELLER_GAME_WORLD_H

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/rect2i.hpp>
#include <godot_cpp/variant/string.hpp>
#include <unordered_map>
#include <memory>
#include "core/world_coords.h"
#include "data/item_db.h"
#include "data/race_db.h"
#include "components/inventory.h"
#include "entities/entity_pool.h"
#include "fast_tilemap.h"
#include "world_bubble.h"
#include "world_generator.h"
#include "path/a_star_grid.h"
#include "turn_scheduler.h"
#include "components/locomotion.h"
#include "components/perception.h"
#include "components/action_resolver.h"
#include "components/ai_controller.h"

namespace godot {

class GameWorld : public Node2D {
    GDCLASS(GameWorld, Node2D)

public:
    enum BubbleLayer {
        LAYER_TILE = WorldBubble::LAYER_TILE,
        LAYER_INDICATOR = WorldBubble::LAYER_INDICATOR,
        LAYER_MAX = WorldBubble::LAYER_MAX
    };

private:
    FastTileMap* renderer = nullptr;
    WorldBubble bubble;
    EntityPool entity_pool;
    std::unique_ptr<WorldGenerator> generator;
    std::unique_ptr<AStarGridPathfinder> pathfinder;

    Ref<FastNoiseLite> biome_noise;
    int world_seed = 0;

protected:
    static void _bind_methods();

public:
    GameWorld();
    ~GameWorld();

    void setup_renderer();
    FastTileMap* get_renderer() const { return renderer; }
    WorldBubble* get_bubble() { return &bubble; }
    const WorldBubble* get_bubble() const { return &bubble; }

    std::unordered_map<uint32_t, LocomotionData> locomotion_data;
    std::unordered_map<uint32_t, PerceptionMemory> perception_memory;
    std::unordered_map<uint32_t, AIData> ai_data;
    TurnScheduler turn_scheduler;

    static int get_region_size() { return WorldCoords::REGION_SIZE; }
    static int get_chunk_size() { return WorldCoords::CHUNK_SIZE; }

    static uint64_t pack_coords(int x, int y) { return WorldCoords::pack_coords(x, y); }
    static Vector2i unpack_coords(uint64_t key) { return WorldCoords::unpack_coords(key); }

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
    bool pickup_item_specific(const Vector2i& pos, const String& item_id, int amount, Inventory* p_inventory);
    bool has_item(const Vector2i& pos) const;

    bool is_cell_seen(const Vector2i& pos) const;
    Array request_player_path(const Vector2i& start, const Vector2i& goal);
    Array find_path(const Vector2i& start, const Vector2i& goal);

    uint32_t spawn_entity(int x, int y, const String& race_id, const String& ai_tier = "raycast");
    void despawn_entity(uint32_t entity_id);
    EntityPool* get_entity_pool() { return &entity_pool; }
    const EntityPool* get_entity_pool() const { return &entity_pool; }

    void process_npcs(int current_turn, int player_x, int player_y);

    Dictionary get_save_data() const;

    Array find_path_with_flags(const Vector2i& start, const Vector2i& goal, uint32_t flags);
    void load_save_data(const Dictionary &p_data);
};

}

VARIANT_ENUM_CAST(GameWorld::BubbleLayer);

#endif // ! SPACETRAVELLER_GAME_WORLD_H
