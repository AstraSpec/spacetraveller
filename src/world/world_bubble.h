#ifndef SPACETRAVELLER_WORLD_BUBBLE_H
#define SPACETRAVELLER_WORLD_BUBBLE_H

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/rect2i.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/color.hpp>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "cell_data.h"
#include "traversal_snapshot.h"
#include "entities/entity.h"

namespace godot {

class EntityPool;
class EntityLedger;

class WorldBubble {
public:
    using TileSource = std::function<uint16_t(int world_x, int world_y)>;

    enum Layer {
        LAYER_TILE = 0,
        LAYER_INDICATOR = 1,
        LAYER_MAX
    };

    struct CellEntity {
        uint32_t entity_id;
    };

    struct CellVisual {
        bool draw_item = false;
        uint16_t item_id = 0;
        uint16_t tile_id = 0;
        bool occluded = false;
        bool seen = true;
        uint16_t entity_sprite_id = 0;
        uint16_t entity_atlas_x = 0;
        uint16_t entity_atlas_y = 0;
        bool draw_overlay = false;
        uint16_t overlay_atlas_x = 0;
        uint16_t overlay_atlas_y = 0;
        Color overlay_color = Color(1, 1, 1, 1);
    };

    struct BubbleSnapshot {
        std::unordered_map<uint64_t, CellVisual> cells[LAYER_MAX];
    };

    struct Overlay {
        uint16_t atlas_x = 0;
        uint16_t atlas_y = 0;
        Color color = Color(1, 1, 1, 1);
        float lifetime = -1.0f; // seconds; < 0 = persistent until removed
        float age = 0.0f;
    };

private:
    CellData cell_data;
    std::unordered_map<uint64_t, uint16_t> tile_overrides[LAYER_MAX];
    std::unordered_map<uint64_t, uint16_t> generated_tile_cache[LAYER_MAX];
    std::unordered_set<uint64_t> seen_cells;
    std::unordered_set<uint64_t> visible_cells;
    std::vector<uint64_t> newly_seen_cells;
    std::unordered_map<uint64_t, CellEntity> entity_positions;
    std::unordered_map<uint64_t, Overlay> overlays;
    std::unordered_map<uint64_t, Dictionary> tile_metadata;

    EntityPool* entity_pool_source = nullptr;

    int world_bubble_radius = 32;
    TileSource tile_source = nullptr;

    uint16_t resolve_tile_id(int layer, uint64_t cell_key, int world_x, int world_y);

public:
    WorldBubble() = default;

    void set_tile_source(TileSource p_source) { tile_source = p_source; }
    void set_world_bubble_radius(int p_radius) { world_bubble_radius = p_radius; }
    int get_world_bubble_radius() const { return world_bubble_radius; }

    void place_tile(int x, int y, const String& tile_id, Layer p_layer = LAYER_TILE);
    void place_tile_id(int x, int y, uint16_t tile_id, Layer p_layer = LAYER_TILE);
    String get_tile_at(int x, int y, Layer p_layer = LAYER_TILE) const;
    void fill_tiles(int x, int y, const String& tile_id, const Vector2i& playerPos, const Rect2i& mask = Rect2i(), bool invert_mask = false, bool p_contiguous = true, Layer p_layer = LAYER_TILE);

    const DroppedItem* get_top_item(int x, int y) const;

    void drop_item(const Vector2i& pos, uint16_t item_id, int amount);
    int remove_item(const Vector2i& pos, uint16_t item_id, int amount);
    int peek_item_amount(const Vector2i& pos, uint16_t item_id) const;
    Array get_items_at(const Vector2i& pos) const;
    bool has_items(const Vector2i& pos) const;

    void set_tile_metadata(const Vector2i& pos, const Dictionary& data);
    Dictionary get_tile_metadata(const Vector2i& pos) const;
    void clear_tile_metadata(const Vector2i& pos);
    void clear_all_tile_metadata();
    Dictionary serialize_tile_metadata() const;
    void deserialize_tile_metadata(const Dictionary& data);

    Dictionary serialize_ground_items() const;
    void deserialize_ground_items(const Dictionary& data);

    void invalidate_tile_cache(int world_x, int world_y, Layer p_layer = LAYER_TILE);
    void invalidate_region_cache(const Rect2i& p_rect, Layer p_layer = LAYER_TILE);
    void clear_cache(Layer p_layer = LAYER_TILE);
    void clear_all_caches();

    Dictionary get_tile_id_cache(Layer p_layer = LAYER_TILE) const;
    void set_tile_id_cache(const Dictionary& p_cache, Layer p_layer = LAYER_TILE);
    void merge_tile_id_cache(const Dictionary& p_cache, Layer p_layer = LAYER_TILE);
    const std::unordered_map<uint64_t, uint16_t>& get_tile_overrides(Layer p_layer = LAYER_TILE) const {
        return tile_overrides[p_layer];
    }

    Array get_seen_cells() const;
    void set_seen_cells(const Array& p_seen);
    bool is_cell_seen(int x, int y) const;
    std::vector<uint64_t> consume_newly_seen_cells();

    void set_entity_pool(EntityPool* pool) { entity_pool_source = pool; }
    bool set_entity(int x, int y, uint32_t entity_id);
    void force_set_entity(int x, int y, uint32_t entity_id);
    void remove_entity(int x, int y);
    bool update_entity_position(int old_x, int old_y, int new_x, int new_y, uint32_t entity_id);
    void clear_entities();
    void rebuild_from_pool();
    const CellEntity* get_entity_at(int x, int y) const;

    void add_overlay(int x, int y, uint16_t atlas_x, uint16_t atlas_y, const Color& color, float lifetime = -1.0f);
    void remove_overlay(int x, int y);
    void clear_overlays();
    bool tick_overlays(float delta);
    bool has_timed_overlays() const;

    uint16_t query_tile_id(int x, int y);
    TraversalSnapshot build_traversal_snapshot(
        const Vector2i& start,
        const Vector2i& goal,
        const std::vector<Vector2i>& blocking_positions = std::vector<Vector2i>(),
        const EntityLedger* ledger = nullptr,
        uint32_t entity_id = UINT32_MAX,
        const String& traversal_profile = "",
        bool allow_openable_tiles = false
    );

    const std::unordered_map<uint64_t, uint16_t>& get_tile_cache(Layer p_layer = LAYER_TILE) const {
        return generated_tile_cache[p_layer];
    }

    void update_visibility(
        const Vector2i& player_pos,
        const std::vector<uint64_t>& offset_keys,
        bool occlusion_enabled
    );

    BubbleSnapshot build_snapshot(
        const Vector2i& player_pos,
        const std::vector<uint64_t>& offset_keys,
        bool occlusion_enabled
    );
};

}

#endif // SPACETRAVELLER_WORLD_BUBBLE_H
