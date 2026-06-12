#include "fast_map_renderer.h"
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void FastMapRenderer::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_tilesheet", "texture"), &FastMapRenderer::set_tilesheet);
    ClassDB::bind_method(D_METHOD("get_tilesheet"), &FastMapRenderer::get_tilesheet);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "tilesheet", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_tilesheet", "get_tilesheet");

    ClassDB::bind_method(D_METHOD("clear"), &FastMapRenderer::clear);
    ClassDB::bind_method(D_METHOD("set_cell", "pos", "atlas"), &FastMapRenderer::set_cell);
    ClassDB::bind_static_method("FastMapRenderer", D_METHOD("get_tile_size"), &FastMapRenderer::get_tile_size);
}

FastMapRenderer::FastMapRenderer() = default;

FastMapRenderer::~FastMapRenderer() {
    clear();
}

void FastMapRenderer::set_tilesheet(const Ref<Texture2D>& p_texture) {
    tilesheet = p_texture;
    RenderingServer* rs = RenderingServer::get_singleton();
    if (rs) {
        rs->canvas_item_set_default_texture_filter(get_canvas_item(), RenderingServer::CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
    }
}

Ref<Texture2D> FastMapRenderer::get_tilesheet() const {
    return tilesheet;
}

void FastMapRenderer::clear() {
    RenderingServer* rs = RenderingServer::get_singleton();
    if (rs) {
        for (const auto& pair : cell_rids) {
            rs->free_rid(pair.second);
        }
    }
    cell_rids.clear();
}

void FastMapRenderer::set_cell(const Vector2i& p_pos, const Vector2i& p_atlas) {
    if (!tilesheet.is_valid() || p_atlas.x < 0 || p_atlas.y < 0) {
        return;
    }

    RenderingServer* rs = RenderingServer::get_singleton();
    if (!rs) {
        return;
    }

    const uint64_t key = WorldCoords::pack_coords(p_pos.x, p_pos.y);
    auto rid_it = cell_rids.find(key);
    if (rid_it == cell_rids.end()) {
        RID rid = rs->canvas_item_create();
        rs->canvas_item_set_parent(rid, get_canvas_item());
        rs->canvas_item_set_default_texture_filter(rid, RenderingServer::CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
        rid_it = cell_rids.emplace(key, rid).first;
    }

    RID rid = rid_it->second;
    rs->canvas_item_clear(rid);
    rs->canvas_item_add_texture_rect_region(
        rid,
        Rect2(p_pos.x * TILE_SIZE, p_pos.y * TILE_SIZE, TILE_SIZE, TILE_SIZE),
        tilesheet->get_rid(),
        Rect2(
            1 + p_atlas.x * (TILE_SIZE + 1),
            1 + p_atlas.y * (TILE_SIZE + 1),
            TILE_SIZE,
            TILE_SIZE
        )
    );
}
