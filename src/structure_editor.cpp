#include "structure_editor.h"
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "data/database.h"
#include "data/id_registry.h"
#include <unordered_set>
#include <algorithm>

using namespace godot;

void StructureEditor::_bind_methods() {
    ClassDB::bind_method(D_METHOD("update_visuals", "centerPos"), &StructureEditor::update_visuals);
    ClassDB::bind_method(D_METHOD("export_to_rle", "id"), &StructureEditor::export_to_rle);
    ClassDB::bind_method(D_METHOD("import_from_rle", "blueprint", "palette"), &StructureEditor::import_from_rle);
    ClassDB::bind_method(D_METHOD("update_preview_tiles", "positions", "tile_id"), &StructureEditor::update_preview_tiles);
    ClassDB::bind_method(D_METHOD("update_preview_tiles_with_data", "data"), &StructureEditor::update_preview_tiles_with_data);
    ClassDB::bind_method(D_METHOD("update_preview_shape", "type", "p1", "p2", "filled", "perfect", "tile_id"), &StructureEditor::update_preview_shape);
    ClassDB::bind_method(D_METHOD("commit_shape", "type", "p1", "p2", "filled", "perfect", "tile_id"), &StructureEditor::commit_shape);
    ClassDB::bind_method(D_METHOD("clear_preview_tiles"), &StructureEditor::clear_preview_tiles);

    BIND_ENUM_CONSTANT(SHAPE_RECTANGLE);
    BIND_ENUM_CONSTANT(SHAPE_ELLIPSIS);
}

StructureEditor::StructureEditor() {
    spacing = 1;
}

StructureEditor::~StructureEditor() {
    clear_preview_tiles();
}

void StructureEditor::update_visuals(const Vector2i& centerPos) {
    if (!tilesheet.is_valid()) return;
    
    RenderingServer* rs = RenderingServer::get_singleton();
    RID texture_rid = tilesheet->get_rid();
    TileDb* tile_db = TileDb::get_singleton();
    if (!tile_db) return;

    for (auto& pair : tile_rids) {
        uint64_t offsetKey = pair.first;
        int ox = static_cast<int>(static_cast<int32_t>(offsetKey >> 32));
        int oy = static_cast<int>(static_cast<int32_t>(offsetKey & 0xFFFFFFFF));
        
        int cx = ox + centerPos.x;
        int cy = oy + centerPos.y;
        uint64_t cellKey = Occlusion::pack_coords(cx, cy);
        
        uint16_t tile_id = 0; // void
        auto it = tile_id_cache.find(cellKey);
        if (it != tile_id_cache.end()) {
            tile_id = it->second;
        }

        if (tile_id != 0) {
            update_tile_at(ox, oy, centerPos, tile_id, rs, texture_rid, tile_db);
            rs->canvas_item_set_modulate(pair.second, Color(1, 1, 1, 1));
        } else {
            rs->canvas_item_clear(pair.second);
        }
    }
}

Dictionary StructureEditor::export_to_rle(const String &p_id) const {
    Dictionary result;
    
    int size = world_bubble_size;

    // RLE Encoding with Numeric Indices
    Array palette;
    std::unordered_map<String, int, StringHasher> id_to_index;
    String blueprint_str = "(";
    
    String current_id = "";
    int count = 0;

    auto finalize_run = [&](const String &id, int run_count) {
        if (run_count <= 0) return;

        if (id_to_index.find(id) == id_to_index.end()) {
            id_to_index[id] = palette.size();
            palette.push_back(id);
        }
        
        int index = id_to_index[id];
        if (blueprint_str.length() > 1) blueprint_str += ", ";
        blueprint_str += String::num_int64(run_count) + "x" + String::num_int64(index);
    };

    // Iterate through the full grid
    IdRegistry* id_reg = IdRegistry::get_singleton();
    for (int y = -size/2; y < size/2; y++) {
        for (int x = -size/2; x < size/2; x++) {
            uint64_t key = Occlusion::pack_coords(x, y);
            String tile_id = "void";
            auto it = tile_id_cache.find(key);
            if (it != tile_id_cache.end()) {
                if (id_reg) tile_id = id_reg->get_string(it->second);
            }

            if (tile_id == current_id) {
                count++;
            } else {
                finalize_run(current_id, count);
                current_id = tile_id;
                count = 1;
            }
        }
    }
    finalize_run(current_id, count);
    blueprint_str += ")";

    // Build Result
    result["id"] = p_id;
    result["palette"] = palette;
    result["blueprint"] = blueprint_str;

    return result;
}

void StructureEditor::import_from_rle(const String &p_blueprint, const Array &p_palette) {
    tile_id_cache.clear();
    
    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (!id_reg) return;

    // Resolve Palette to IDs
    std::vector<uint16_t> palette_ids;
    for (int i = 0; i < p_palette.size(); i++) {
        palette_ids.push_back(id_reg->register_string(p_palette[i]));
    }

    // Clean and Parse Blueprint
    String rle = p_blueprint.replace("(", "").replace(")", "").replace("[", "").replace("]", "");
    PackedStringArray parts = rle.split(",");

    int size = world_bubble_size;
    int current_pos = 0;
    int total_expected = size * size;

    for (int i = 0; i < parts.size(); i++) {
        String part = parts[i].strip_edges();
        if (part.is_empty()) continue;

        PackedStringArray sub = part.split("x");
        if (sub.size() != 2) continue;

        int count = sub[0].to_int();
        int palette_idx = sub[1].to_int();

        uint16_t tile_id = 0; // Default to void
        if (palette_idx >= 0 && palette_idx < (int)palette_ids.size()) {
            tile_id = palette_ids[palette_idx];
        }

        for (int j = 0; j < count && current_pos < total_expected; j++) {
            int x = (current_pos % size) - size/2;
            int y = (current_pos / size) - size/2;
            
            if (tile_id != 0) {
                uint64_t key = Occlusion::pack_coords(x, y);
                tile_id_cache[key] = tile_id;
            }
            current_pos++;
        }
    }
}

void StructureEditor::update_preview_tiles(const Array &p_positions, const String &p_tile_id) {
    clear_preview_tiles();

    if (!tilesheet.is_valid()) return;
    TileDb* tile_db = TileDb::get_singleton();
    if (!tile_db) return;

    const TileInfo* info = tile_db->get_tile_info(p_tile_id);
    if (!info) return;

    RenderingServer* rs = RenderingServer::get_singleton();
    RID texture_rid = tilesheet->get_rid();
    RID parent_rid = get_canvas_item();

    int half = world_bubble_size / 2;

    Vector2i atlas_pos;
    atlas_pos.x = 1 + info->atlas.x * (TILE_SIZE + 1);
    atlas_pos.y = 1 + info->atlas.y * (TILE_SIZE + 1);

    for (int i = 0; i < p_positions.size(); i++) {
        Vector2i pos = p_positions[i];

        if (pos.x < -half || pos.x >= half || pos.y < -half || pos.y >= half) continue;

        uint64_t key = Occlusion::pack_coords(pos.x, pos.y);

        if (preview_tile_rids.count(key)) {
            rs->free_rid(preview_tile_rids[key]);
        }

        RID preview_rid = rs->canvas_item_create();
        rs->canvas_item_set_parent(preview_rid, parent_rid);
        rs->canvas_item_set_z_index(preview_rid, 1);
        
        rs->canvas_item_add_texture_rect_region(
            preview_rid,
            Rect2(pos.x * get_cell_size(), pos.y * get_cell_size(), TILE_SIZE, TILE_SIZE),
            texture_rid,
            Rect2(atlas_pos.x, atlas_pos.y, TILE_SIZE, TILE_SIZE)
        );

        rs->canvas_item_set_modulate(preview_rid, Color(1, 1, 1, 0.5));
        preview_tile_rids[key] = preview_rid;
    }
}

void StructureEditor::update_preview_tiles_with_data(const Dictionary &p_data) {
    clear_preview_tiles();

    if (!tilesheet.is_valid()) return;
    TileDb* tile_db = TileDb::get_singleton();
    if (!tile_db) return;

    RenderingServer* rs = RenderingServer::get_singleton();
    RID texture_rid = tilesheet->get_rid();
    RID parent_rid = get_canvas_item();

    int half = world_bubble_size / 2;
    int cell_size = get_cell_size();

    Array keys = p_data.keys();
    for (int i = 0; i < keys.size(); i++) {
        Variant key_v = keys[i];
        if (key_v.get_type() != Variant::VECTOR2I) continue;
        Vector2i pos = key_v;

        if (pos.x < -half || pos.x >= half || pos.y < -half || pos.y >= half) continue;

        String tile_id = p_data[key_v];
        const TileInfo* info = tile_db->get_tile_info(tile_id);
        if (!info) continue;

        uint64_t key = Occlusion::pack_coords(pos.x, pos.y);

        Vector2i atlas_pos;
        atlas_pos.x = 1 + info->atlas.x * (TILE_SIZE + 1);
        atlas_pos.y = 1 + info->atlas.y * (TILE_SIZE + 1);

        RID preview_rid = rs->canvas_item_create();
        rs->canvas_item_set_parent(preview_rid, parent_rid);
        rs->canvas_item_set_z_index(preview_rid, 1);
        
        rs->canvas_item_add_texture_rect_region(
            preview_rid,
            Rect2(pos.x * cell_size, pos.y * cell_size, TILE_SIZE, TILE_SIZE),
            texture_rid,
            Rect2(atlas_pos.x, atlas_pos.y, TILE_SIZE, TILE_SIZE)
        );

        rs->canvas_item_set_modulate(preview_rid, Color(1, 1, 1, 0.5));
        preview_tile_rids[key] = preview_rid;
    }
}

void StructureEditor::clear_preview_tiles() {
    RenderingServer* rs = RenderingServer::get_singleton();
    for (auto& pair : preview_tile_rids) {
        rs->free_rid(pair.second);
    }
    preview_tile_rids.clear();
}

std::vector<Vector2i> StructureEditor::_get_shape_points(ShapeType p_type, const Vector2i &p_p1, const Vector2i &p_p2, bool p_filled, bool p_perfect) {
    std::vector<Vector2i> points;
    std::unordered_set<uint64_t> unique_points;

    Vector2i p1 = p_p1;
    Vector2i p2 = p_p2;

    if (p_perfect) {
        int dx = abs(p2.x - p1.x);
        int dy = abs(p2.y - p1.y);
        int size = std::max(dx, dy);
        p2.x = p1.x + size * (p2.x >= p1.x ? 1 : -1);
        p2.y = p1.y + size * (p2.y >= p1.y ? 1 : -1);
    }

    Vector2i min_p(std::min(p1.x, p2.x), std::min(p1.y, p2.y));
    Vector2i max_p(std::max(p1.x, p2.x), std::max(p1.y, p2.y));

    auto add_point = [&](int x, int y) {
        uint64_t key = Occlusion::pack_coords(x, y);
        if (unique_points.find(key) == unique_points.end()) {
            unique_points.insert(key);
            points.push_back(Vector2i(x, y));
        }
    };

    if (p_type == SHAPE_RECTANGLE) {
        if (p_filled) {
            for (int x = min_p.x; x <= max_p.x; x++) {
                for (int y = min_p.y; y <= max_p.y; y++) {
                    add_point(x, y);
                }
            }
        } else {
            for (int x = min_p.x; x <= max_p.x; x++) {
                add_point(x, min_p.y);
                add_point(x, max_p.y);
            }
            for (int y = min_p.y + 1; y < max_p.y; y++) {
                add_point(min_p.x, y);
                add_point(max_p.x, y);
            }
        }
    } else if (p_type == SHAPE_ELLIPSIS) {
        double rx = (max_p.x - min_p.x + 1) / 2.0;
        double ry = (max_p.y - min_p.y + 1) / 2.0;
        double cx = (min_p.x + max_p.x + 1) / 2.0;
        double cy = (min_p.y + max_p.y + 1) / 2.0;

        if (p_filled) {
            for (int y = min_p.y; y <= max_p.y; y++) {
                double py = y + 0.5;
                double dy = std::abs(py - cy);
                if (ry > 0) {
                    double s = 1.0 - (dy * dy) / (ry * ry);
                    if (s >= 0) {
                        double dx = rx * std::sqrt(s);
                        int x_start = std::ceil(cx - dx - 0.5);
                        int x_end = std::floor(cx + dx - 0.5);
                        for (int x = x_start; x <= x_end; x++) {
                            add_point(x, y);
                        }
                    }
                }
            }
        } else {
            // Hollow
            // Horizontal pass
            for (int x = min_p.x; x <= max_p.x; x++) {
                double px = x + 0.5;
                double dx = std::abs(px - cx);
                if (rx > 0) {
                    double s = 1.0 - (dx * dx) / (rx * rx);
                    if (s >= 0) {
                        double dy = ry * std::sqrt(s);
                        add_point(x, std::floor(cy + dy - 0.5));
                        add_point(x, std::floor(cy - dy + 0.5));
                    }
                }
            }
            // Vertical pass
            for (int y = min_p.y; y <= max_p.y; y++) {
                double py = y + 0.5;
                double dy = std::abs(py - cy);
                if (ry > 0) {
                    double s = 1.0 - (dy * dy) / (ry * ry);
                    if (s >= 0) {
                        double dx = rx * std::sqrt(s);
                        add_point(std::floor(cx + dx - 0.5), y);
                        add_point(std::floor(cx - dx + 0.5), y);
                    }
                }
            }
        }
    }

    return points;
}

void StructureEditor::update_preview_shape(ShapeType p_type, const Vector2i &p_p1, const Vector2i &p_p2, bool p_filled, bool p_perfect, const String &p_tile_id) {
    std::vector<Vector2i> points = _get_shape_points(p_type, p_p1, p_p2, p_filled, p_perfect);
    Array positions;
    for (const Vector2i &p : points) {
        positions.push_back(p);
    }
    update_preview_tiles(positions, p_tile_id);
}

void StructureEditor::commit_shape(ShapeType p_type, const Vector2i &p_p1, const Vector2i &p_p2, bool p_filled, bool p_perfect, const String &p_tile_id) {
    std::vector<Vector2i> points = _get_shape_points(p_type, p_p1, p_p2, p_filled, p_perfect);
    for (const Vector2i &p : points) {
        place_tile(p.x, p.y, p_tile_id);
    }
    update_visuals(Vector2i(0, 0));
}
