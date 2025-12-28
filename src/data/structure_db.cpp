#include "structure_db.h"
#include "id_registry.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

template<> StructureDb* DataBase<StructureInfo, StructureDb>::singleton = nullptr;

void StructureDb::_bind_methods() {
    ClassDB::bind_static_method("StructureDb", D_METHOD("get_singleton"), &StructureDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &StructureDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_tile_at", "id", "x", "y"), &StructureDb::get_tile_at);
    ClassDB::bind_method(D_METHOD("get_ids"), &StructureDb::get_ids);
}

StructureDb::StructureDb() {}
StructureDb::~StructureDb() {}

StructureInfo StructureDb::_parse_row(const Dictionary &p_data) {
    IdRegistry* id_reg = IdRegistry::get_singleton();
        StructureInfo info;
        
    if (id_reg) {
        id_reg->register_string(p_data["id"]);
    }

        std::vector<uint16_t> palette_ids;
    Array p_array = p_data.get("palette", Array());
        for (int i = 0; i < p_array.size(); i++) {
            if (id_reg) {
                palette_ids.push_back(id_reg->register_string(p_array[i]));
            } else {
                palette_ids.push_back(0);
            }
        }

        const int total_tiles = 1024;
        info.data.assign(total_tiles, 0);

    String rle = p_data.get("blueprint", "");
        rle = rle.replace("(", "").replace(")", "").replace("[", "").replace("]", "");
        PackedStringArray parts = rle.split(",");

        int current_pos = 0;
        for (int i = 0; i < parts.size(); i++) {
            String part = parts[i].strip_edges();
            if (part.is_empty()) continue;

            PackedStringArray sub = part.split("x");
            if (sub.size() != 2) continue;

            int count = sub[0].to_int();
            int palette_idx = sub[1].to_int();

        uint16_t tile_id = 0;
            if (palette_idx >= 0 && palette_idx < (int)palette_ids.size()) {
                tile_id = palette_ids[palette_idx];
            }

            for (int j = 0; j < count && current_pos < total_tiles; j++) {
                info.data[current_pos++] = tile_id;
            }
        }
    return info;
}

uint16_t StructureDb::get_tile_at(const String &p_structure_id, int p_x, int p_y) const {
    const StructureInfo* info = get_info(p_structure_id);
    if (!info) return 0;

    int idx = p_y * 32 + p_x;
    if (idx < 0 || idx >= (int)info->data.size()) return 0;

    return info->data[idx];
}

}
