#include "fast_tilemap.h"
#include "cell_area.h"
#include "data/item_db.h"
#include "data/tile_db.h"
#include "core/world_coords.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <vector>

using namespace godot;

const FastTileMap::LayerProperties FastTileMap::LAYER_PROPS[LAYER_MAX] = {
    { 0, Color(0.4f, 0.4f, 0.5f, 1.0f), Color(0.0f, 0.0f, 0.0f, 1.0f), true },
    { 1, Color(1.0f, 1.0f, 1.0f, 1.0f), Color(0.0f, 0.0f, 0.0f, 0.0f), false }
};

void FastTileMap::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_tilesheet", "texture"), &FastTileMap::set_tilesheet);
    ClassDB::bind_method(D_METHOD("get_tilesheet"), &FastTileMap::get_tilesheet);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "tilesheet", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_tilesheet", "get_tilesheet");

    ClassDB::bind_method(D_METHOD("init_world_bubble", "playerPos", "is_square"), &FastTileMap::init_world_bubble, DEFVAL(true));
    ClassDB::bind_method(D_METHOD("update_visuals", "playerPos"), &FastTileMap::update_visuals);

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
    if (bubble_source) {
        bubble_source->set_active_radius(world_bubble_radius);
        bubble_source->set_player_vision_radius(world_bubble_radius);
    }
}

void FastTileMap::init_world_bubble(const Vector2i& playerPos, bool is_square) {
    RenderingServer* rs = RenderingServer::get_singleton();
    RID parent_rid = get_canvas_item();

    for (int l = 0; l < LAYER_MAX; l++) {
        for (auto& pair : tile_rids[l]) {
            rs->free_rid(pair.second);
        }
        tile_rids[l].clear();
    }

    CellArea render_area = is_square
        ? CellArea::square(Vector2i(), 0, world_bubble_radius)
        : CellArea::circle(Vector2i(), 0, world_bubble_radius);

    for (uint64_t offset_key : render_area.offset_keys()) {
        for (int l = 0; l < LAYER_MAX; l++) {
            RID tile_rid = rs->canvas_item_create();
            rs->canvas_item_set_parent(tile_rid, parent_rid);
            rs->canvas_item_set_z_index(tile_rid, LAYER_PROPS[l].z_index);
            tile_rids[l][offset_key] = tile_rid;
        }
    }
}

std::vector<uint64_t> FastTileMap::get_render_offset_keys() const {
    std::vector<uint64_t> offset_keys;
    offset_keys.reserve(tile_rids[LAYER_TILE].size());
    for (const auto& pair : tile_rids[LAYER_TILE]) {
        offset_keys.push_back(pair.first);
    }
    return offset_keys;
}

void FastTileMap::update_visuals(const Vector2i& playerPos) {
    if (!tilesheet.is_valid() || !bubble_source) return;

    last_player_pos = playerPos;
    has_rendered = true;

    RenderingServer* rs = RenderingServer::get_singleton();
    RID texture_rid = tilesheet->get_rid();
    TileDb* tile_db = TileDb::get_singleton();
    ItemDb* item_db = ItemDb::get_singleton();
    if (!tile_db || !item_db) return;

    std::vector<uint64_t> offset_keys = get_render_offset_keys();

    WorldBubble::BubbleSnapshot snapshot = bubble_source->build_snapshot(playerPos, offset_keys, occlusion_enabled);

    for (int l = 0; l < LAYER_MAX; l++) {
        const LayerProperties& props = LAYER_PROPS[l];
        for (auto& pair : tile_rids[l]) {
            uint64_t offset_key = pair.first;
            Vector2i offset = WorldCoords::unpack_coords(offset_key);
            int ox = offset.x;
            int oy = offset.y;

            auto snap_it = snapshot.cells[l].find(offset_key);
            if (snap_it == snapshot.cells[l].end()) {
                rs->canvas_item_clear(pair.second);
                continue;
            }

            const WorldBubble::CellVisual& visual = snap_it->second;
            bool used_below_tile = false;

            if (visual.draw_item) {
                draw_item_at(ox, oy, visual.item_id, rs, texture_rid, item_db, (Layer)l);
            } else if (visual.draw_overlay) {
                rs->canvas_item_clear(pair.second);
                rs->canvas_item_add_texture_rect_region(
                    pair.second,
                    Rect2(ox * get_cell_size(), oy * get_cell_size(), TILE_SIZE, TILE_SIZE),
                    texture_rid,
                    Rect2(
                        1 + visual.overlay_atlas_x * (TILE_SIZE + 1),
                        1 + visual.overlay_atlas_y * (TILE_SIZE + 1),
                        TILE_SIZE,
                        TILE_SIZE
                    )
                );
            } else if (l == LAYER_TILE && visual.draw_below_tile) {
                draw_below_tile_at(ox, oy, playerPos, visual.below_tile_id, visual.below_depth, rs, texture_rid, tile_db);
                used_below_tile = true;
            } else if (visual.tile_id != 0) {
                update_tile_at(ox, oy, playerPos, visual.tile_id, rs, texture_rid, tile_db, (Layer)l);
            } else {
                rs->canvas_item_clear(pair.second);
            }

            if (l == LAYER_TILE && visual.entity_sprite_id != 0) {
                rs->canvas_item_add_texture_rect_region(
                    pair.second,
                    Rect2(ox * get_cell_size(), oy * get_cell_size(), TILE_SIZE, TILE_SIZE),
                    texture_rid,
                    Rect2(
                        1 + visual.entity_atlas_x * (TILE_SIZE + 1),
                        1 + visual.entity_atlas_y * (TILE_SIZE + 1),
                        TILE_SIZE,
                        TILE_SIZE
                    )
                );
            }

            if (l == LAYER_INDICATOR && visual.draw_overlay) {
                rs->canvas_item_set_modulate(pair.second, visual.overlay_color);
            } else if (used_below_tile) {
                if (occlusion_enabled && visual.occluded) {
                    rs->canvas_item_set_modulate(pair.second, props.seen_modulation);
                } else {
                    rs->canvas_item_set_modulate(pair.second, Color(1, 1, 1, 1));
                }
            } else if (occlusion_enabled) {
                if (!visual.occluded) {
                    rs->canvas_item_set_modulate(pair.second, Color(1, 1, 1, 1));
                } else {
                    rs->canvas_item_set_modulate(pair.second, visual.seen ? props.seen_modulation : props.hidden_modulation);
                }
            } else {
                rs->canvas_item_set_modulate(pair.second, Color(1, 1, 1, 1));
            }
        }
    }
}

void FastTileMap::draw_item_at(int ox, int oy, uint16_t item_id, RenderingServer* rs, RID texture_rid, ItemDb* item_db, Layer p_layer) {
    uint64_t offset_key = WorldCoords::pack_coords(ox, oy);
    auto it_rid = tile_rids[p_layer].find(offset_key);
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

void FastTileMap::draw_below_tile_at(int ox, int oy, const Vector2i& playerPos, uint16_t tile_id, int depth, RenderingServer* rs, RID texture_rid, TileDb* tile_db) {
    uint64_t offset_key = WorldCoords::pack_coords(ox, oy);
    auto it_rid = tile_rids[LAYER_TILE].find(offset_key);
    if (it_rid == tile_rids[LAYER_TILE].end()) return;

    RID tile_rid = it_rid->second;
    Vector2i atlas_pos(1, 1);
    const TileInfo* info = tile_db->get_tile_info(tile_id);
    if (info && !info->atlas_variants.empty()) {
        uint32_t variant_idx = _get_variant_index(ox + playerPos.x, oy + playerPos.y - depth, info->atlas_variants.size());
        Vector2i variant_coords = info->atlas_variants[variant_idx];
        atlas_pos.x = 1 + variant_coords.x * (TILE_SIZE + 1);
        atlas_pos.y = 1 + variant_coords.y * (TILE_SIZE + 1);
    }

    rs->canvas_item_clear(tile_rid);
    rs->canvas_item_add_texture_rect_region(
        tile_rid,
        Rect2(ox * get_cell_size(), oy * get_cell_size(), TILE_SIZE, TILE_SIZE),
        texture_rid,
        Rect2(atlas_pos.x, atlas_pos.y, TILE_SIZE, TILE_SIZE)
    );

    float overlay_alpha = 0.18f + 0.08f * static_cast<float>(depth - 1);
    if (overlay_alpha > 0.48f) overlay_alpha = 0.48f;
    rs->canvas_item_add_rect(
        tile_rid,
        Rect2(ox * get_cell_size(), oy * get_cell_size(), TILE_SIZE, TILE_SIZE),
        Color(1.0f, 1.0f, 1.0f, overlay_alpha)
    );
}

void FastTileMap::update_tile_at(int ox, int oy, const Vector2i& playerPos, uint16_t tile_id, RenderingServer* rs, RID texture_rid, TileDb* tile_db, Layer p_layer) {
    uint64_t offset_key = WorldCoords::pack_coords(ox, oy);
    auto it_rid = tile_rids[p_layer].find(offset_key);
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

    rs->canvas_item_clear(tile_rid);
    rs->canvas_item_add_texture_rect_region(
        tile_rid,
        Rect2(ox * get_cell_size(), oy * get_cell_size(), TILE_SIZE, TILE_SIZE),
        texture_rid,
        Rect2(atlas_pos.x, atlas_pos.y, TILE_SIZE, TILE_SIZE)
    );
}

void FastTileMap::_process(double delta) {
    if (!bubble_source || !has_rendered) return;
    if (!bubble_source->has_timed_overlays()) return;

    bubble_source->tick_overlays(static_cast<float>(delta));
    update_visuals(last_player_pos);
}
