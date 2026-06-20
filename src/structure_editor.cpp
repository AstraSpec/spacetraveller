#include "structure_editor.h"
#include "world/game_world.h"
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "data/database.h"
#include "data/race_db.h"
#include "core/id_registry.h"
#include "core/rng.h"
#include <unordered_set>
#include <algorithm>

using namespace godot;

static Vector2i normalize_structure_editor_size(const Vector2i &p_size) {
    const int max_size = GameWorld::get_chunk_size();
    Vector2i size = p_size;
    if (size.x <= 0) size.x = max_size;
    if (size.y <= 0) size.y = max_size;
    size.x = std::clamp(size.x, 1, max_size);
    size.y = std::clamp(size.y, 1, max_size);
    return size;
}

void StructureEditor::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_world", "world"), &StructureEditor::set_world);
    ClassDB::bind_method(D_METHOD("get_world"), &StructureEditor::get_world);
    ClassDB::bind_method(D_METHOD("set_tilemap", "tilemap"), &StructureEditor::set_tilemap);
    ClassDB::bind_method(D_METHOD("get_tilemap"), &StructureEditor::get_tilemap);

    ClassDB::bind_method(D_METHOD("export_to_rle", "id", "offset", "z", "size"), &StructureEditor::export_to_rle, DEFVAL(Vector2i()), DEFVAL(0), DEFVAL(Vector2i()));
    ClassDB::bind_method(D_METHOD("import_from_rle", "blueprint", "palette", "offset", "z", "size"), &StructureEditor::import_from_rle, DEFVAL(Vector2i()), DEFVAL(0), DEFVAL(Vector2i()));
    ClassDB::bind_method(D_METHOD("update_preview_tiles", "positions", "tile_id", "entry_type"), &StructureEditor::update_preview_tiles, DEFVAL("tile"));
    ClassDB::bind_method(D_METHOD("update_preview_tiles_with_data", "data", "entry_type"), &StructureEditor::update_preview_tiles_with_data, DEFVAL("tile"));
    ClassDB::bind_method(D_METHOD("update_preview_shape", "type", "p1", "p2", "filled", "perfect", "tile_id", "entry_type"), &StructureEditor::update_preview_shape, DEFVAL("tile"));
    ClassDB::bind_method(D_METHOD("get_shape_points", "type", "p1", "p2", "filled", "perfect"), &StructureEditor::get_shape_points);
    ClassDB::bind_method(D_METHOD("clear_preview_tiles"), &StructureEditor::clear_preview_tiles);

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "tilemap", PROPERTY_HINT_RESOURCE_TYPE, "FastTileMap"), "set_tilemap", "get_tilemap");

    BIND_ENUM_CONSTANT(SHAPE_RECTANGLE);
    BIND_ENUM_CONSTANT(SHAPE_ELLIPSIS);
}

StructureEditor::StructureEditor() {
}

StructureEditor::~StructureEditor() {
    clear_preview_tiles();
}

void StructureEditor::set_world(GameWorld *p_world) {
    world = p_world;
    tilemap = p_world ? p_world->get_renderer() : nullptr;
}

GameWorld *StructureEditor::get_world() const {
    return world;
}

void StructureEditor::set_tilemap(FastTileMap *p_tilemap) {
    tilemap = p_tilemap;
}

FastTileMap *StructureEditor::get_tilemap() const {
    return tilemap;
}

Dictionary StructureEditor::export_to_rle(const String &p_id, const Vector2i &p_offset, int p_z, const Vector2i &p_size) const {
    Dictionary result;

    const Vector2i size = normalize_structure_editor_size(p_size);
    if (!world) return result;
    Dictionary cache = world->get_tile_id_cache(GameWorld::LAYER_TILE);

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

    IdRegistry* id_reg = IdRegistry::get_singleton();
    for (int y = p_offset.y; y < p_offset.y + size.y; y++) {
        for (int x = p_offset.x; x < p_offset.x + size.x; x++) {
            uint64_t key = WorldCoords::pack_coords_3d(x, y, p_z);
            String tile_id = "void";
            
            auto it_v = cache.get(key, Variant());
            if (it_v.get_type() != Variant::NIL) {
                if (id_reg) tile_id = id_reg->get_string((uint16_t)((int)it_v));
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

void StructureEditor::import_from_rle(const String &p_blueprint, const Array &p_palette, const Vector2i &p_offset, int p_z, const Vector2i &p_size) {
    if (!world) return;
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

    const Vector2i size = normalize_structure_editor_size(p_size);
    int current_pos = 0;
    int total_expected = size.x * size.y;

    Dictionary new_cache;

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
            int x = (current_pos % size.x) + p_offset.x;
            int y = (current_pos / size.x) + p_offset.y;
            
            uint64_t key = WorldCoords::pack_coords_3d(x, y, p_z);
            new_cache[key] = (int)tile_id;
            
            current_pos++;
        }
    }
    world->merge_tile_id_cache(new_cache, GameWorld::LAYER_TILE);
}

void StructureEditor::clear_preview_tiles() {
    RenderingServer* rs = RenderingServer::get_singleton();
    for (auto& pair : preview_tile_rids) {
        rs->free_rid(pair.second);
    }
    preview_tile_rids.clear();
}

void StructureEditor::_create_preview_tile(const Vector2i &pos, const String &p_id, const String &entry_type, RenderingServer *rs, RID texture_rid, RID parent_rid, int half, int cell_size) {
    if (pos.x < -half || pos.x >= half || pos.y < -half || pos.y >= half) return;

    int tile_size = FastTileMap::get_tile_size();
    Vector2i atlas_coords(-1, -1);

    if (entry_type == "item") {
        ItemDb* item_db = ItemDb::get_singleton();
        if (!item_db) return;
        const ItemInfo* info = item_db->get_item_info(p_id);
        if (!info) return;
        atlas_coords = info->atlas;
    } else if (entry_type == "npc") {
        RaceDb* race_db = RaceDb::get_singleton();
        if (!race_db) return;
        const RaceInfo* info = race_db->get_race_info(p_id);
        if (!info) return;
        atlas_coords = info->atlas;
    } else if (entry_type == "loot_table") {
        atlas_coords = Vector2i(71, 18);
    } else if (entry_type == "entity_group") {
        atlas_coords = Vector2i(72, 18);
    } else {
        TileDb* tile_db = TileDb::get_singleton();
        if (!tile_db) return;
        const TileInfo* info = tile_db->get_tile_info(p_id);
        if (!info) return;
        atlas_coords = info->atlas_variants.empty() ? Vector2i(-1, -1) : info->atlas_variants[0];
        if (info->atlas_variants.size() > 1 && tilemap) {
            uint32_t idx = Rng::variant_index(static_cast<uint32_t>(tilemap->get_world_seed()), pos.x, pos.y, info->atlas_variants.size());
            atlas_coords = info->atlas_variants[idx];
        }
    }

    uint64_t key = WorldCoords::pack_coords(pos.x, pos.y);
    if (preview_tile_rids.count(key)) {
        rs->free_rid(preview_tile_rids[key]);
    }

    Vector2i atlas_pos(1 + atlas_coords.x * (tile_size + 1), 1 + atlas_coords.y * (tile_size + 1));

    RID preview_rid = rs->canvas_item_create();
    rs->canvas_item_set_parent(preview_rid, parent_rid);
    rs->canvas_item_set_z_index(preview_rid, 1);
    rs->canvas_item_add_texture_rect_region(preview_rid, Rect2(pos.x * cell_size, pos.y * cell_size, tile_size, tile_size), texture_rid, Rect2(atlas_pos.x, atlas_pos.y, tile_size, tile_size));
    rs->canvas_item_set_modulate(preview_rid, Color(1, 1, 1, 0.5));
    preview_tile_rids[key] = preview_rid;
}

void StructureEditor::update_preview_tiles(const Array &p_positions, const String &p_tile_id, const String &p_entry_type) {
    clear_preview_tiles();
    Ref<Texture2D> tilesheet = tilemap->get_tilesheet();
    if (!tilesheet.is_valid()) return;

    RenderingServer* rs = RenderingServer::get_singleton();
    RID texture_rid = tilesheet->get_rid();
    RID parent_rid = get_canvas_item();
    int half = tilemap->get_world_bubble_size() / 2;
    int cell_size = tilemap->get_cell_size();

    for (int i = 0; i < p_positions.size(); i++) {
        _create_preview_tile(p_positions[i], p_tile_id, p_entry_type, rs, texture_rid, parent_rid, half, cell_size);
    }
}

void StructureEditor::update_preview_tiles_with_data(const Dictionary &p_data, const String &p_entry_type) {
    clear_preview_tiles();
    Ref<Texture2D> tilesheet = tilemap->get_tilesheet();
    if (!tilesheet.is_valid()) return;

    RenderingServer* rs = RenderingServer::get_singleton();
    RID texture_rid = tilesheet->get_rid();
    RID parent_rid = get_canvas_item();
    int half = tilemap->get_world_bubble_size() / 2;
    int cell_size = tilemap->get_cell_size();

    Array keys = p_data.keys();
    for (int i = 0; i < keys.size(); i++) {
        if (keys[i].get_type() != Variant::VECTOR2I) continue;
        _create_preview_tile(keys[i], p_data[keys[i]], p_entry_type, rs, texture_rid, parent_rid, half, cell_size);
    }
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
        uint64_t key = WorldCoords::pack_coords(x, y);
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

void StructureEditor::update_preview_shape(ShapeType p_type, const Vector2i &p_p1, const Vector2i &p_p2, bool p_filled, bool p_perfect, const String &p_tile_id, const String &p_entry_type) {
    std::vector<Vector2i> points = _get_shape_points(p_type, p_p1, p_p2, p_filled, p_perfect);
    Array positions;
    for (const Vector2i &p : points) {
        positions.push_back(p);
    }
    update_preview_tiles(positions, p_tile_id, p_entry_type);
}

Array StructureEditor::get_shape_points(ShapeType p_type, const Vector2i &p_p1, const Vector2i &p_p2, bool p_filled, bool p_perfect) {
    std::vector<Vector2i> points = _get_shape_points(p_type, p_p1, p_p2, p_filled, p_perfect);
    Array res;
    for (const Vector2i &p : points) {
        res.push_back(p);
    }
    return res;
}
