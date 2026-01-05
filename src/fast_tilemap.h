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
#include "data/tile_db.h"
#include "occlusion.h"

namespace godot {

class FastTileMap : public Node2D {
    GDCLASS(FastTileMap, Node2D)

protected:
    static void _bind_methods();

    static const int TILE_SIZE = 12;

    int world_bubble_size = 64;
    int world_bubble_radius = 32;
    int spacing = 0;

    std::unordered_map<uint64_t, RID> tile_rids;
    std::unordered_map<uint64_t, uint16_t> tile_id_cache;
    std::unordered_set<uint64_t> seen_cells;

    Ref<Texture2D> tilesheet;

public:
    FastTileMap();
    ~FastTileMap();

    void set_tilesheet(const Ref<Texture2D>& texture);
    Ref<Texture2D> get_tilesheet() const;

    int get_spacing() const { return spacing; }
    int get_cell_size() const { return TILE_SIZE + spacing; }

    static int get_tile_size() { return TILE_SIZE; }
    void set_world_bubble_size(int p_size);
    int get_world_bubble_size() const { return world_bubble_size; }
    int get_world_bubble_radius() const { return world_bubble_radius; }

    void init_world_bubble(const Vector2i& playerPos, bool is_square = false);
    void update_tile_at(int ox, int oy, const Vector2i& playerPos, uint16_t tile_id, RenderingServer* rs, RID texture_rid, TileDb* tile_db);
    void place_tile(int x, int y, const String& tile_id);
    String get_tile_at(int x, int y) const;
    void fill_tiles(int x, int y, const String& tile_id, const Rect2i& mask = Rect2i(), bool invert_mask = false, bool p_contiguous = true);
    void clear_cache();
};

}

#endif // SPACETRAVELLER_FAST_TILEMAP_H

