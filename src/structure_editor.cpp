#include "structure_editor.h"
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "data/database.h"
#include "data/id_registry.h"

using namespace godot;

void StructureEditor::_bind_methods() {
    ClassDB::bind_method(D_METHOD("update_visuals", "centerPos"), &StructureEditor::update_visuals);
    ClassDB::bind_method(D_METHOD("export_to_rle", "id"), &StructureEditor::export_to_rle);
}

StructureEditor::StructureEditor() {
    spacing = 1;
}

StructureEditor::~StructureEditor() {
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
