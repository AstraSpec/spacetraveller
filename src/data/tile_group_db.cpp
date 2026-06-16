#include "tile_group_db.h"
#include "core/id_registry.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

template<> TileGroupDb* DataBase<TileGroupInfo, TileGroupDb>::singleton = nullptr;

void TileGroupDb::_bind_methods() {
    ClassDB::bind_static_method("TileGroupDb", D_METHOD("get_singleton"), &TileGroupDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &TileGroupDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_ids"), &TileGroupDb::get_ids);
}

TileGroupDb::TileGroupDb() {}
TileGroupDb::~TileGroupDb() {}

TileGroupInfo TileGroupDb::_parse_row(const Dictionary &p_data) {
    TileGroupInfo info;
    info.id = String(p_data.get("id", ""));

    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (!id_reg) return info;

    Variant entries_var = p_data.get("entries", Array());
    if (entries_var.get_type() == Variant::ARRAY) {
        Array entries = entries_var;
        for (int i = 0; i < entries.size(); i++) {
            if (entries[i].get_type() != Variant::DICTIONARY) continue;

            Dictionary entry_data = entries[i];
            String tile = String(entry_data.get("tile", entry_data.get("tile_id", "")));
            if (tile.is_empty()) {
                UtilityFunctions::push_error("[TileGroupDb] Entry in tile group ", info.id, " has no tile");
                continue;
            }

            TileGroupEntryInfo entry;
            entry.tile_id = id_reg->register_string(tile);
            entry.weight = static_cast<int>(entry_data.get("weight", Variant(1)));
            if (entry.weight <= 0) continue;

            info.total_weight += entry.weight;
            info.entries.push_back(entry);
        }
    }

    if (info.entries.empty() || info.total_weight <= 0) {
        UtilityFunctions::push_error("[TileGroupDb] Tile group has no rollable entries: ", info.id);
    }

    return info;
}

const TileGroupInfo* TileGroupDb::get_tile_group(const String &p_id) const {
    return get_info(p_id);
}

}
