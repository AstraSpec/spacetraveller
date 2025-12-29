#include "fast_tilemap.h"
#include "data/id_registry.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <queue>

using namespace godot;

void FastTileMap::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_tilesheet", "texture"), &FastTileMap::set_tilesheet);
    ClassDB::bind_method(D_METHOD("get_tilesheet"), &FastTileMap::get_tilesheet);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "tilesheet", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_tilesheet", "get_tilesheet");

    ClassDB::bind_method(D_METHOD("init_world_bubble", "playerPos", "is_square"), &FastTileMap::init_world_bubble, DEFVAL(false));
    ClassDB::bind_method(D_METHOD("place_tile", "x", "y", "tile_id"), &FastTileMap::place_tile);
    ClassDB::bind_method(D_METHOD("get_tile_at", "x", "y"), &FastTileMap::get_tile_at);
    ClassDB::bind_method(D_METHOD("fill_tiles", "x", "y", "tile_id"), &FastTileMap::fill_tiles);
    ClassDB::bind_method(D_METHOD("clear_cache"), &FastTileMap::clear_cache);

    ClassDB::bind_method(D_METHOD("get_spacing"), &FastTileMap::get_spacing);
    ClassDB::bind_method(D_METHOD("get_cell_size"), &FastTileMap::get_cell_size);

    ClassDB::bind_static_method("FastTileMap", D_METHOD("get_tile_size"), &FastTileMap::get_tile_size);
    ClassDB::bind_method(D_METHOD("set_world_bubble_size", "size"), &FastTileMap::set_world_bubble_size);
    ClassDB::bind_method(D_METHOD("get_world_bubble_size"), &FastTileMap::get_world_bubble_size);
    ClassDB::bind_method(D_METHOD("get_world_bubble_radius"), &FastTileMap::get_world_bubble_radius);
}

FastTileMap::FastTileMap() {
}

FastTileMap::~FastTileMap() {
    RenderingServer* rs = RenderingServer::get_singleton();
    for (auto& pair : tile_rids) {
        rs->free_rid(pair.second);
    }
    tile_rids.clear();
}

void FastTileMap::set_tilesheet(const Ref<Texture2D>& texture) {
    tilesheet = texture;
}

Ref<Texture2D> FastTileMap::get_tilesheet() const {
    return tilesheet;
}

void FastTileMap::set_world_bubble_size(int p_size) {
    world_bubble_size = p_size;
    world_bubble_radius = p_size / 2;
}

void FastTileMap::init_world_bubble(const Vector2i& playerPos, bool is_square) {
    RenderingServer* rs = RenderingServer::get_singleton();
    RID parent_rid = get_canvas_item();
    
    // Clear any existing tiles
    for (auto& pair : tile_rids) {
        rs->free_rid(pair.second);
    }
    tile_rids.clear();
    tile_id_cache.clear();
    seen_cells.clear();
    
    // Create tiles in circular or square bubble
    for (int i = 0; i < world_bubble_size * world_bubble_size; i++) {
        int ox = (i / world_bubble_size) - world_bubble_radius;
        int oy = (i % world_bubble_size) - world_bubble_radius;
        
        // Check if within radius
        float dist = sqrtf(static_cast<float>(ox * ox + oy * oy));
        if (is_square || dist < static_cast<float>(world_bubble_radius)) {
            uint64_t offsetKey = Occlusion::pack_coords(ox, oy);
            
            // Create canvas item for this tile
            RID tile_rid = rs->canvas_item_create();
            rs->canvas_item_set_parent(tile_rid, parent_rid);
            tile_rids[offsetKey] = tile_rid;
        }
    }
}

void FastTileMap::update_tile_at(int ox, int oy, const Vector2i& playerPos, uint16_t tile_id, RenderingServer* rs, RID texture_rid, TileDb* tile_db) {
    uint64_t offsetKey = Occlusion::pack_coords(ox, oy);
    auto it_rid = tile_rids.find(offsetKey);
    if (it_rid == tile_rids.end()) return;
    
    RID tile_rid = it_rid->second;
    
    Vector2i atlas_pos(1, 1);
    const TileInfo* info = tile_db->get_tile_info(tile_id);
    if (info) {
        atlas_pos.x = 1 + info->atlas.x * (TILE_SIZE + 1);
        atlas_pos.y = 1 + info->atlas.y * (TILE_SIZE + 1);
    }
    
    // Clear and render tile
    rs->canvas_item_clear(tile_rid);
    rs->canvas_item_add_texture_rect_region(
        tile_rid,
        Rect2(ox * get_cell_size(), oy * get_cell_size(), TILE_SIZE, TILE_SIZE),
        texture_rid,
        Rect2(atlas_pos.x, atlas_pos.y, TILE_SIZE, TILE_SIZE)
    );
}

void FastTileMap::place_tile(int x, int y, const String& tile_id) {
    uint64_t cellKey = Occlusion::pack_coords(x, y);
    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (id_reg) {
        tile_id_cache[cellKey] = id_reg->get_id(tile_id);
    }
}

String FastTileMap::get_tile_at(int x, int y) const {
    uint64_t cellKey = Occlusion::pack_coords(x, y);
    auto it = tile_id_cache.find(cellKey);
    if (it != tile_id_cache.end()) {
        IdRegistry* id_reg = IdRegistry::get_singleton();
        if (id_reg) {
            return id_reg->get_string(it->second);
        }
    }
    return "void";
}

void FastTileMap::fill_tiles(int x, int y, const String& tile_id) {
    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (!id_reg) return;

    uint16_t new_id = id_reg->get_id(tile_id);
    uint16_t target_id = 0;

    uint64_t startKey = Occlusion::pack_coords(x, y);
    auto it = tile_id_cache.find(startKey);
    if (it != tile_id_cache.end()) {
        target_id = it->second;
    }

    if (new_id == target_id) return;

    std::queue<Vector2i> q;
    q.push(Vector2i(x, y));

    int radius = world_bubble_radius;

    while (!q.empty()) {
        Vector2i p = q.front();
        q.pop();

        if (p.x < -radius || p.x >= radius || p.y < -radius || p.y >= radius) continue;

        uint64_t key = Occlusion::pack_coords(p.x, p.y);
        uint16_t current_id = 0;
        auto it_cur = tile_id_cache.find(key);
        if (it_cur != tile_id_cache.end()) {
            current_id = it_cur->second;
        }

        if (current_id == target_id) {
            tile_id_cache[key] = new_id;
            q.push(Vector2i(p.x + 1, p.y));
            q.push(Vector2i(p.x - 1, p.y));
            q.push(Vector2i(p.x, p.y + 1));
            q.push(Vector2i(p.x, p.y - 1));
        }
    }
}

void FastTileMap::clear_cache() {
    tile_id_cache.clear();
}

