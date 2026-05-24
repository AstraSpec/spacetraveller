#ifndef SPACETRAVELLER_FAST_TILEMAP_H
#define SPACETRAVELLER_FAST_TILEMAP_H

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/color.hpp>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include "data/tile_db.h"
#include "occlusion.h"
#include "cell_data.h"

namespace godot {

class FastTileMap : public Node2D {
    GDCLASS(FastTileMap, Node2D)

public:
    using TileSource = std::function<uint16_t(int world_x, int world_y)>;

    enum Layer {
        LAYER_TILE = 0,
        LAYER_INDICATOR = 1,
        LAYER_MAX
    };

    struct LayerProperties {
        int z_index;
        Color seen_modulation;
        Color hidden_modulation;
        bool has_items;
    };

    static const LayerProperties LAYER_PROPS[LAYER_MAX];

protected:
    static void _bind_methods();

    static const int TILE_SIZE = 12;

    int world_bubble_size = 64;
    int world_bubble_radius = 32;
    int spacing = 0;
    int world_seed = 0;

    std::unordered_map<uint64_t, RID> tile_rids[LAYER_MAX];
    std::unordered_map<uint64_t, uint16_t> tile_id_cache[LAYER_MAX];
    std::unordered_set<uint64_t> seen_cells;

    Ref<Texture2D> tilesheet;

    TileSource tile_source = nullptr;
    CellData* cell_data_source = nullptr;

    bool occlusion_enabled = false;

public:
    FastTileMap();
    ~FastTileMap();

    void set_tilesheet(const Ref<Texture2D>& texture);
    Ref<Texture2D> get_tilesheet() const;

    void set_tile_source(TileSource p_source) { tile_source = p_source; }
    void set_cell_data(CellData* p_cell_data) { cell_data_source = p_cell_data; }
    void set_occlusion_enabled(bool p_enabled) { occlusion_enabled = p_enabled; }
    bool is_occlusion_enabled() const { return occlusion_enabled; }

    void invalidate_tile_cache(int world_x, int world_y, Layer p_layer = LAYER_TILE);
    void invalidate_region_cache(const Rect2i& p_rect, Layer p_layer = LAYER_TILE);
    void draw_item_at(int ox, int oy, uint16_t item_id, RenderingServer* rs, RID texture_rid, class ItemDb* item_db, Layer p_layer = LAYER_TILE);


    void set_spacing(int p_spacing) { spacing = p_spacing; }
    int get_spacing() const { return spacing; }
    int get_cell_size() const { return TILE_SIZE + spacing; }

    void set_world_seed(int p_seed) { world_seed = p_seed; }
    int get_world_seed() const { return world_seed; }

    static int get_tile_size() { return TILE_SIZE; }
    void set_world_bubble_size(int p_size);
    int get_world_bubble_size() const { return world_bubble_size; }
    int get_world_bubble_radius() const { return world_bubble_radius; }

    void init_world_bubble(const Vector2i& playerPos, bool is_square = false);
    void update_visuals(const Vector2i& playerPos);
    void update_tile_at(int ox, int oy, const Vector2i& playerPos, uint16_t tile_id, RenderingServer* rs, RID texture_rid, TileDb* tile_db, Layer p_layer = LAYER_TILE);
    void place_tile(int x, int y, const String& tile_id, Layer p_layer = LAYER_TILE);
    String get_tile_at(int x, int y, Layer p_layer = LAYER_TILE) const;
    void fill_tiles(int x, int y, const String& tile_id, const Vector2i& playerPos, const Rect2i& mask = Rect2i(), bool invert_mask = false, bool p_contiguous = true, Layer p_layer = LAYER_TILE);
    void clear_cache(Layer p_layer = LAYER_TILE);
    void clear_all_caches();

    Dictionary get_tile_id_cache(Layer p_layer = LAYER_TILE) const;
    void set_tile_id_cache(const Dictionary &p_cache, Layer p_layer = LAYER_TILE);
    void merge_tile_id_cache(const Dictionary &p_cache, Layer p_layer = LAYER_TILE);

    Array get_seen_cells() const;
    void set_seen_cells(const Array &p_seen);

protected:
    uint32_t _get_variant_index(int x, int y, int variant_count) const {
        if (variant_count <= 1) return 0;
        uint32_t h = (static_cast<uint32_t>(x) * 1597334677U) ^ 
                     (static_cast<uint32_t>(y) * 3812015801U) ^ 
                     (static_cast<uint32_t>(world_seed));
        return h % variant_count;
    }
};

}

VARIANT_ENUM_CAST(FastTileMap::Layer);

#endif // SPACETRAVELLER_FAST_TILEMAP_H

