#include "fast_tilemap.h"
#include "core/id_registry.h"
#include "data/item_db.h"
#include "core/world_coords.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <queue>

using namespace godot;

const FastTileMap::LayerProperties FastTileMap::LAYER_PROPS[LAYER_MAX] = {
    // TILE: Base layer, shows items, dimmed when remembered, black when hidden
    { 0, Color(0.4f, 0.4f, 0.5f, 1.0f), Color(0.0f, 0.0f, 0.0f, 1.0f), true },
    // INDICATOR: Overlay layer, no items, bright when remembered, transparent when hidden
    { 1, Color(1.0f, 1.0f, 1.0f, 1.0f), Color(0.0f, 0.0f, 0.0f, 0.0f), false }
};

void FastTileMap::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_tilesheet", "texture"), &FastTileMap::set_tilesheet);
    ClassDB::bind_method(D_METHOD("get_tilesheet"), &FastTileMap::get_tilesheet);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "tilesheet", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_tilesheet", "get_tilesheet");

    ClassDB::bind_method(D_METHOD("init_world_bubble", "playerPos", "is_square"), &FastTileMap::init_world_bubble, DEFVAL(false));
    ClassDB::bind_method(D_METHOD("update_visuals", "playerPos"), &FastTileMap::update_visuals);
    ClassDB::bind_method(D_METHOD("place_tile", "x", "y", "tile_id", "layer"), &FastTileMap::place_tile, DEFVAL(LAYER_TILE));
    ClassDB::bind_method(D_METHOD("get_tile_at", "x", "y", "layer"), &FastTileMap::get_tile_at, DEFVAL(LAYER_TILE));
    ClassDB::bind_method(D_METHOD("fill_tiles", "x", "y", "tile_id", "playerPos", "mask", "invert_mask", "contiguous", "layer"), &FastTileMap::fill_tiles, DEFVAL(Rect2i()), DEFVAL(false), DEFVAL(true), DEFVAL(LAYER_TILE));
    ClassDB::bind_method(D_METHOD("clear_cache", "layer"), &FastTileMap::clear_cache, DEFVAL(LAYER_TILE));
    ClassDB::bind_method(D_METHOD("clear_all_caches"), &FastTileMap::clear_all_caches);
    ClassDB::bind_method(D_METHOD("get_tile_id_cache", "layer"), &FastTileMap::get_tile_id_cache, DEFVAL(LAYER_TILE));
    ClassDB::bind_method(D_METHOD("set_tile_id_cache", "cache", "layer"), &FastTileMap::set_tile_id_cache, DEFVAL(LAYER_TILE));
    ClassDB::bind_method(D_METHOD("merge_tile_id_cache", "cache", "layer"), &FastTileMap::merge_tile_id_cache, DEFVAL(LAYER_TILE));

    ClassDB::bind_method(D_METHOD("get_seen_cells"), &FastTileMap::get_seen_cells);
    ClassDB::bind_method(D_METHOD("set_seen_cells", "seen"), &FastTileMap::set_seen_cells);

    ClassDB::bind_method(D_METHOD("set_spacing", "spacing"), &FastTileMap::set_spacing);
    ClassDB::bind_method(D_METHOD("get_spacing"), &FastTileMap::get_spacing);
    ClassDB::bind_method(D_METHOD("get_cell_size"), &FastTileMap::get_cell_size);

    ClassDB::bind_method(D_METHOD("set_world_seed", "seed"), &FastTileMap::set_world_seed);
    ClassDB::bind_method(D_METHOD("get_world_seed"), &FastTileMap::get_world_seed);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "world_seed"), "set_world_seed", "get_world_seed");

    ClassDB::bind_static_method("FastTileMap", D_METHOD("get_tile_size"), &FastTileMap::get_tile_size);
    ClassDB::bind_method(D_METHOD("set_world_bubble_size", "size"), &FastTileMap::set_world_bubble_size);
    ClassDB::bind_method(D_METHOD("get_world_bubble_size"), &FastTileMap::get_world_bubble_size);
    ClassDB::bind_method(D_METHOD("get_world_bubble_radius"), &FastTileMap::get_world_bubble_radius);

    ClassDB::bind_method(D_METHOD("set_occlusion_enabled", "enabled"), &FastTileMap::set_occlusion_enabled);
    ClassDB::bind_method(D_METHOD("is_occlusion_enabled"), &FastTileMap::is_occlusion_enabled);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "occlusion_enabled"), "set_occlusion_enabled", "is_occlusion_enabled");

    ClassDB::bind_method(D_METHOD("invalidate_tile_cache", "world_x", "world_y", "layer"), &FastTileMap::invalidate_tile_cache, DEFVAL(LAYER_TILE));
    ClassDB::bind_method(D_METHOD("invalidate_region_cache", "rect", "layer"), &FastTileMap::invalidate_region_cache, DEFVAL(LAYER_TILE));

    BIND_ENUM_CONSTANT(LAYER_TILE);
    BIND_ENUM_CONSTANT(LAYER_INDICATOR);
    BIND_ENUM_CONSTANT(LAYER_MAX);
}

FastTileMap::FastTileMap() {
}

FastTileMap::~FastTileMap() {
    RenderingServer* rs = RenderingServer::get_singleton();
    for (int l = 0; l < LAYER_MAX; l++) {
        for (auto& pair : tile_rids[l]) {
            rs->free_rid(pair.second);
        }
        tile_rids[l].clear();
    }
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
    for (int l = 0; l < LAYER_MAX; l++) {
        for (auto& pair : tile_rids[l]) {
            rs->free_rid(pair.second);
        }
        tile_rids[l].clear();
        tile_id_cache[l].clear();
    }
    
    // Create tiles in circular or square bubble
    for (int i = 0; i < world_bubble_size * world_bubble_size; i++) {
        int ox = (i / world_bubble_size) - world_bubble_radius;
        int oy = (i % world_bubble_size) - world_bubble_radius;
        
        // Check if within radius
        float dist = sqrtf(static_cast<float>(ox * ox + oy * oy));
        if (is_square || dist < static_cast<float>(world_bubble_radius)) {
            uint64_t offsetKey = WorldCoords::pack_coords(ox, oy);
            
            // Create canvas items for all layers
            for (int l = 0; l < LAYER_MAX; l++) {
                RID tile_rid = rs->canvas_item_create();
                rs->canvas_item_set_parent(tile_rid, parent_rid);
                // Set z-index based on layer properties
                rs->canvas_item_set_z_index(tile_rid, LAYER_PROPS[l].z_index);
                tile_rids[l][offsetKey] = tile_rid;
            }
        }
    }
}

void FastTileMap::update_visuals(const Vector2i& playerPos) {
    if (!tilesheet.is_valid()) return;
    RenderingServer* rs = RenderingServer::get_singleton();
    RID texture_rid = tilesheet->get_rid();
    TileDb* tile_db = TileDb::get_singleton();
    ItemDb* item_db = ItemDb::get_singleton();
    if (!tile_db || !item_db) return;

    // Pass 1 - Draw: iterate bubble offsets per layer.
    for (int l = 0; l < LAYER_MAX; l++) {
        const LayerProperties& props = LAYER_PROPS[l];
        for (auto& pair : tile_rids[l]) {
            uint64_t offsetKey = pair.first;
            Vector2i offset = WorldCoords::unpack_coords(offsetKey);
            int ox = offset.x, oy = offset.y;
            int cx = ox + playerPos.x;
            int cy = oy + playerPos.y;
            uint64_t cellKey = WorldCoords::pack_coords(cx, cy);
            
            bool item_drawn = false;
            if (props.has_items && cell_data_source) {
                const DroppedItem* top = cell_data_source->get_top_item(cellKey);
                if (top) {
                    draw_item_at(ox, oy, top->id, rs, texture_rid, item_db, (Layer)l);
                    item_drawn = true;
                }
            }
            
            if (!item_drawn) {
                uint16_t tile_id = 0;
                auto it = tile_id_cache[l].find(cellKey);
                if (it != tile_id_cache[l].end()) {
                    tile_id = it->second;
                } else if (l == LAYER_TILE && tile_source) {
                    tile_id = tile_source(cx, cy);
                    if (tile_id != 0) {
                        tile_id_cache[l][cellKey] = tile_id;
                    }
                }
                
                if (tile_id != 0) {
                    update_tile_at(ox, oy, playerPos, tile_id, rs, texture_rid, tile_db, (Layer)l);
                } else {
                    rs->canvas_item_clear(pair.second);
                }
            }
        }
    }

    // Pass 2 - Occlusion:
    if (occlusion_enabled) {
        std::unordered_set<uint64_t> visible_keys;
        Occlusion::compute_visible(playerPos, world_bubble_radius, tile_id_cache[LAYER_TILE], visible_keys);
        
        for (int l = 0; l < LAYER_MAX; l++) {
            const LayerProperties& props = LAYER_PROPS[l];
            for (auto& pair : tile_rids[l]) {
                uint64_t offsetKey = pair.first;
                int ox = static_cast<int>(static_cast<int32_t>(offsetKey >> 32));
                int oy = static_cast<int>(static_cast<int32_t>(offsetKey & 0xFFFFFFFF));
                int cx = ox + playerPos.x;
                int cy = oy + playerPos.y;
                uint64_t cellKey = WorldCoords::pack_coords(cx, cy);
                
                bool occluded = visible_keys.find(cellKey) == visible_keys.end();
                bool seen = seen_cells.count(cellKey) > 0;
                if (!occluded) {
                    seen_cells.insert(cellKey);
                    seen = true;
                }
                
                if (!occluded) {
                    rs->canvas_item_set_modulate(pair.second, Color(1, 1, 1, 1));
                } else {
                    rs->canvas_item_set_modulate(pair.second, seen ? props.seen_modulation : props.hidden_modulation);
                }
            }
        }
    } else {
        // Force white modulation on all RIDs
        for (int l = 0; l < LAYER_MAX; l++) {
            for (auto& pair : tile_rids[l]) {
                rs->canvas_item_set_modulate(pair.second, Color(1, 1, 1, 1));
            }
        }
    }
}

void FastTileMap::draw_item_at(int ox, int oy, uint16_t item_id, RenderingServer* rs, RID texture_rid, ItemDb* item_db, Layer p_layer) {
    uint64_t offsetKey = WorldCoords::pack_coords(ox, oy);
    auto it_rid = tile_rids[p_layer].find(offsetKey);
    if (it_rid == tile_rids[p_layer].end()) return;
    
    RID tile_rid = it_rid->second;
    const ItemInfo* info = item_db->get_item_info(item_id);
    if (!info) return;

    Vector2i atlas_pos;
    atlas_pos.x = 1 + info->atlas.x * (TILE_SIZE + 1);
    atlas_pos.y = 1 + info->atlas.y * (TILE_SIZE + 1);

    rs->canvas_item_clear(tile_rid);
    rs->canvas_item_add_texture_rect_region(
        tile_rid,
        Rect2(ox * get_cell_size(), oy * get_cell_size(), TILE_SIZE, TILE_SIZE),
        texture_rid,
        Rect2(atlas_pos.x, atlas_pos.y, TILE_SIZE, TILE_SIZE)
    );
}

void FastTileMap::invalidate_tile_cache(int world_x, int world_y, Layer p_layer) {
    uint64_t key = WorldCoords::pack_coords(world_x, world_y);
    tile_id_cache[p_layer].erase(key);
}

void FastTileMap::invalidate_region_cache(const Rect2i& p_rect, Layer p_layer) {
    auto& cache = tile_id_cache[p_layer];
    for (auto it = cache.begin(); it != cache.end(); ) {
        Vector2i pos = WorldCoords::unpack_coords(it->first);
        if (p_rect.has_point(pos)) {
            it = cache.erase(it);
        } else {
            ++it;
        }
    }
}


void FastTileMap::update_tile_at(int ox, int oy, const Vector2i& playerPos, uint16_t tile_id, RenderingServer* rs, RID texture_rid, TileDb* tile_db, Layer p_layer) {
    uint64_t offsetKey = WorldCoords::pack_coords(ox, oy);
    auto it_rid = tile_rids[p_layer].find(offsetKey);
    if (it_rid == tile_rids[p_layer].end()) return;
    
    RID tile_rid = it_rid->second;
    
    Vector2i atlas_pos(1, 1);
    const TileInfo* info = tile_db->get_tile_info(tile_id);
    if (info && !info->atlas_variants.empty()) {
        uint32_t variant_idx = _get_variant_index(ox + playerPos.x, oy + playerPos.y, info->atlas_variants.size());
        Vector2i variant_coords = info->atlas_variants[variant_idx];
        atlas_pos.x = 1 + variant_coords.x * (TILE_SIZE + 1);
        atlas_pos.y = 1 + variant_coords.y * (TILE_SIZE + 1);
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

void FastTileMap::place_tile(int x, int y, const String& tile_id, Layer p_layer) {
    uint64_t cellKey = WorldCoords::pack_coords(x, y);
    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (id_reg) {
        tile_id_cache[p_layer][cellKey] = id_reg->get_id(tile_id);
    }
}

// Reads tile_id_cache only, won't find tiles outside of world bubble
String FastTileMap::get_tile_at(int x, int y, Layer p_layer) const {
    uint64_t cellKey = WorldCoords::pack_coords(x, y);
    auto it = tile_id_cache[p_layer].find(cellKey);
    if (it != tile_id_cache[p_layer].end()) {
        IdRegistry* id_reg = IdRegistry::get_singleton();
        if (id_reg) {
            return id_reg->get_string(it->second);
        }
    }
    return "void";
}

void FastTileMap::fill_tiles(int x, int y, const String& tile_id, const Vector2i& playerPos, const Rect2i& mask, bool invert_mask, bool p_contiguous, Layer p_layer) {
    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (!id_reg) return;

    uint16_t new_id = id_reg->get_id(tile_id);
    uint16_t target_id = 0;

    uint64_t startKey = WorldCoords::pack_coords(x, y);
    auto it = tile_id_cache[p_layer].find(startKey);
    if (it != tile_id_cache[p_layer].end()) {
        target_id = it->second;
    }

    if (new_id == target_id) return;

    int radius = world_bubble_radius;
    bool has_mask = mask.size.x > 0 && mask.size.y > 0;

    if (p_contiguous) {
        std::queue<Vector2i> q;
        q.push(Vector2i(x, y));

        while (!q.empty()) {
            Vector2i p = q.front();
            q.pop();

            // World bounds check relative to playerPos
            if (p.x < playerPos.x - radius || p.x >= playerPos.x + radius || 
                p.y < playerPos.y - radius || p.y >= playerPos.y + radius) continue;

            // Mask check (p is in world-space, mask is in relative-space)
            if (has_mask) {
                Vector2i relative_p = p - playerPos;
                bool inside = mask.has_point(relative_p);
                if (invert_mask) {
                    if (inside) continue;
                } else {
                    if (!inside) continue;
                }
            }

            uint64_t key = WorldCoords::pack_coords(p.x, p.y);
            uint16_t current_id = 0;
            auto it_cur = tile_id_cache[p_layer].find(key);
            if (it_cur != tile_id_cache[p_layer].end()) {
                current_id = it_cur->second;
            }

            if (current_id == target_id) {
                tile_id_cache[p_layer][key] = new_id;
                q.push(Vector2i(p.x + 1, p.y));
                q.push(Vector2i(p.x - 1, p.y));
                q.push(Vector2i(p.x, p.y + 1));
                q.push(Vector2i(p.x, p.y - 1));
            }
        }
    } else {
        // Non-contiguous fill (Replace within the world bubble and mask)
        for (int gy = playerPos.y - radius; gy < playerPos.y + radius; gy++) {
            for (int gx = playerPos.x - radius; gx < playerPos.x + radius; gx++) {
                Vector2i p(gx, gy);

                // Mask check (p is in world-space, mask is in relative-space)
                if (has_mask) {
                    Vector2i relative_p = p - playerPos;
                    bool inside = mask.has_point(relative_p);
                    if (invert_mask) {
                        if (inside) continue;
                    } else {
                        if (!inside) continue;
                    }
                }

                uint64_t key = WorldCoords::pack_coords(gx, gy);
                uint16_t current_id = 0;
                auto it_cur = tile_id_cache[p_layer].find(key);
                if (it_cur != tile_id_cache[p_layer].end()) {
                    current_id = it_cur->second;
                }

                if (current_id == target_id) {
                    tile_id_cache[p_layer][key] = new_id;
                }
            }
        }
    }
}

void FastTileMap::clear_cache(Layer p_layer) {
    tile_id_cache[p_layer].clear();
}

void FastTileMap::clear_all_caches() {
    for (int l = 0; l < LAYER_MAX; l++) {
        tile_id_cache[l].clear();
    }
}

Dictionary FastTileMap::get_tile_id_cache(Layer p_layer) const {
    Dictionary d;
    for (auto const& [key, val] : tile_id_cache[p_layer]) {
        d[key] = (int)val;
    }
    return d;
}

void FastTileMap::set_tile_id_cache(const Dictionary &p_cache, Layer p_layer) {
    tile_id_cache[p_layer].clear();
    merge_tile_id_cache(p_cache, p_layer);
}

void FastTileMap::merge_tile_id_cache(const Dictionary &p_cache, Layer p_layer) {
    Array keys = p_cache.keys();
    for (int i = 0; i < keys.size(); i++) {
        Variant key_var = keys[i];
        uint64_t key;
        if (key_var.get_type() == Variant::STRING) {
            key = ((String)key_var).to_int();
        } else {
            key = key_var;
        }
        tile_id_cache[p_layer][key] = (uint16_t)((int)p_cache[key_var]);
    }
}

Array FastTileMap::get_seen_cells() const {
    Array a;
    for (uint64_t key : seen_cells) {
        a.push_back(key);
    }
    return a;
}

void FastTileMap::set_seen_cells(const Array &p_seen) {
    seen_cells.clear();
    for (int i = 0; i < p_seen.size(); i++) {
        Variant v = p_seen[i];
        if (v.get_type() == Variant::STRING) {
            seen_cells.insert(((String)v).to_int());
        } else {
            seen_cells.insert((uint64_t)v);
        }
    }
}

