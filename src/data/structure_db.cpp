#include "structure_db.h"
#include "id_registry.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

StructureDb *StructureDb::singleton = nullptr;

void StructureDb::_bind_methods() {
    ClassDB::bind_method(D_METHOD("initialize_data"), &StructureDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_tile_at", "id", "x", "y"), &StructureDb::get_tile_at);
    ClassDB::bind_method(D_METHOD("get_full_data"), &StructureDb::get_full_data);
}

void StructureDb::create_singleton() {
    singleton = memnew(StructureDb);
}

void StructureDb::delete_singleton() {
    if (singleton) {
        memdelete(singleton);
        singleton = nullptr;
    }
}

StructureDb::StructureDb() {}
StructureDb::~StructureDb() {}

void StructureDb::initialize_data() {
    IdRegistry* id_reg = IdRegistry::get_singleton();
    db_helper.load_directory("res://data/structures");
    
    cache.clear();
    for (const auto& pair : db_helper.get_all_rows()) {
        const String& id = pair.first;
        const Dictionary& d = pair.second;
        
        if (id_reg) id_reg->register_string(id);

        StructureInfo info;
        
        // Setup palette cache
        Array p_array = d.get("palette", Array());
        for (int i = 0; i < p_array.size(); i++) {
            info.palette.push_back(p_array[i]);
        }

        // Decompress RLE blueprint (Implicit 32x32 = 1024)
        const int total_tiles = 1024;
        info.data.assign(total_tiles, 0);

        String rle = d.get("blueprint", "");
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

            // Safety bounds check
            if (palette_idx >= info.palette.size()) {
                palette_idx = 0; // Fallback to void
            }

            for (int j = 0; j < count && current_pos < total_tiles; j++) {
                info.data[current_pos++] = static_cast<uint8_t>(palette_idx);
            }
        }
        
        cache[id] = info;
    }
    UtilityFunctions::print("StructureDb initialized with ", cache.size(), " structures.");
}

String StructureDb::get_tile_at(const String &p_structure_id, int p_x, int p_y) const {
    auto it = cache.find(p_structure_id);
    if (it == cache.end()) return "void";

    const StructureInfo& info = it->second;
    // Hardcoded 32 width for index calculation
    int idx = p_y * 32 + p_x;
    if (idx < 0 || idx >= (int)info.data.size()) return "void";

    uint8_t palette_idx = info.data[idx];
    if (palette_idx < info.palette.size()) {
        return info.palette[palette_idx];
    }

    return "void";
}

}