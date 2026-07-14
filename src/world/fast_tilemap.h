#ifndef SPACETRAVELLER_FAST_TILEMAP_H
#define SPACETRAVELLER_FAST_TILEMAP_H

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/color.hpp>
#include <unordered_map>
#include <vector>
#include "data/tile_db.h"
#include "world_bubble.h"
#include "core/rng.h"

namespace godot {

class FastTileMap : public Node2D {
    GDCLASS(FastTileMap, Node2D)

public:
    enum Layer {
        LAYER_TILE = 0,
        LAYER_INDICATOR = 1,
        LAYER_MAX = 2
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

    Ref<Texture2D> tilesheet;

    WorldBubble* bubble_source = nullptr;

    bool occlusion_enabled = false;
    bool show_items = true;
    bool show_entities = true;

    Vector2i last_render_focus;
    Vector2i last_view_origin;
    bool has_rendered = false;

    void draw_item_at(int ox, int oy, uint16_t item_id, RenderingServer* rs, RID texture_rid, class ItemDb* item_db, Layer p_layer = LAYER_TILE);
    void update_tile_at(int ox, int oy, const Vector2i& render_focus, uint16_t tile_id, RenderingServer* rs, RID texture_rid, TileDb* tile_db, Layer p_layer = LAYER_TILE);
    void draw_below_tile_at(int ox, int oy, const Vector2i& render_focus, uint16_t tile_id, int depth, RenderingServer* rs, RID texture_rid, TileDb* tile_db);

    uint32_t _get_variant_index(int x, int y, int variant_count) const {
        return Rng::variant_index(static_cast<uint32_t>(world_seed), x, y, static_cast<uint32_t>(variant_count));
    }

public:
    FastTileMap();
    ~FastTileMap();

    void set_tilesheet(const Ref<Texture2D>& texture);
    Ref<Texture2D> get_tilesheet() const;

    void set_bubble(WorldBubble* p_bubble) { bubble_source = p_bubble; }
    WorldBubble* get_bubble() const { return bubble_source; }
    void set_occlusion_enabled(bool p_enabled) { occlusion_enabled = p_enabled; }
    bool is_occlusion_enabled() const { return occlusion_enabled; }
    void set_show_items(bool p_enabled) { show_items = p_enabled; }
    bool get_show_items() const { return show_items; }
    void set_show_entities(bool p_enabled) { show_entities = p_enabled; }
    bool get_show_entities() const { return show_entities; }

    void set_spacing(int p_spacing) { spacing = p_spacing; }
    int get_spacing() const { return spacing; }
    int get_cell_size() const { return TILE_SIZE + spacing; }

    void set_world_seed(int p_seed) { world_seed = p_seed; }
    int get_world_seed() const { return world_seed; }

    static int get_tile_size() { return TILE_SIZE; }
    void set_world_bubble_size(int p_size);
    int get_world_bubble_size() const { return world_bubble_size; }
    int get_world_bubble_radius() const { return world_bubble_radius; }

    void init_world_bubble(const Vector2i& playerPos, bool is_square = true);
    std::vector<uint64_t> get_render_offset_keys() const;
    void update_visuals(const Vector2i& render_focus, const Vector2i& view_origin);
    void _process(double delta) override;
};

}

#endif // SPACETRAVELLER_FAST_TILEMAP_H
