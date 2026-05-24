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

namespace godot {

class WorldBubble {
public:
    using TileSource = std::function<uint16_t(int world_x, int world_y)>;

    enum Layer {
        LAYER_TILE = 0,
        LAYER_INDICATOR = 1,
        LAYER_MAX
    };

    struct CellVisual {
        bool draw_item = false;
        uint16_t item_id = 0;
        uint16_t tile_id = 0;
        bool occluded = false;
        bool seen = true;
    };

    struct BubbleSnapshot {
        std::unordered_map<uint64_t, CellVisual> cells[LAYER_MAX];
    };

private:
    CellData cell_data;
    std::unordered_map<uint64_t, uint16_t> tile_id_cache[LAYER_MAX];
    std::unordered_set<uint64_t> seen_cells;
    std::unordered_set<uint64_t> visible_cells;

    int world_bubble_radius = 32;
    TileSource tile_source = nullptr;

    uint16_t resolve_tile_id(int layer, uint64_t cell_key, int world_x, int world_y);

public:
    WorldBubble() = default;

    void set_tile_source(TileSource p_source) { tile_source = p_source; }
    void set_world_bubble_radius(int p_radius) { world_bubble_radius = p_radius; }
    int get_world_bubble_radius() const { return world_bubble_radius; }

    void place_tile(int x, int y, const String& tile_id, Layer p_layer = LAYER_TILE);
    String get_tile_at(int x, int y, Layer p_layer = LAYER_TILE) const;
    void fill_tiles(int x, int y, const String& tile_id, const Vector2i& playerPos, const Rect2i& mask = Rect2i(), bool invert_mask = false, bool p_contiguous = true, Layer p_layer = LAYER_TILE);

    const DroppedItem* get_top_item(int x, int y) const;

    void drop_item(const Vector2i& pos, uint16_t item_id, int amount);
    int remove_item(const Vector2i& pos, uint16_t item_id, int amount);
    int peek_item_amount(const Vector2i& pos, uint16_t item_id) const;
    Array get_items_at(const Vector2i& pos) const;
    bool has_items(const Vector2i& pos) const;

    Dictionary serialize_ground_items() const;
    void deserialize_ground_items(const Dictionary& data);

    void invalidate_tile_cache(int world_x, int world_y, Layer p_layer = LAYER_TILE);
    void invalidate_region_cache(const Rect2i& p_rect, Layer p_layer = LAYER_TILE);
    void clear_cache(Layer p_layer = LAYER_TILE);
    void clear_all_caches();

    Dictionary get_tile_id_cache(Layer p_layer = LAYER_TILE) const;
    void set_tile_id_cache(const Dictionary& p_cache, Layer p_layer = LAYER_TILE);
    void merge_tile_id_cache(const Dictionary& p_cache, Layer p_layer = LAYER_TILE);

    Array get_seen_cells() const;
    void set_seen_cells(const Array& p_seen);

    const std::unordered_map<uint64_t, uint16_t>& get_tile_cache(Layer p_layer = LAYER_TILE) const {
        return tile_id_cache[p_layer];
    }

    BubbleSnapshot build_snapshot(
        const Vector2i& player_pos,
        const std::vector<uint64_t>& offset_keys,
        bool occlusion_enabled
    );
};

}

#endif // SPACETRAVELLER_WORLD_BUBBLE_H
