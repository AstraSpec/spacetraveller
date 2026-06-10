#include "structure_db.h"
#include "core/id_registry.h"
#include "core/world_coords.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

template<> StructureDb* DataBase<StructureInfo, StructureDb>::singleton = nullptr;
const int StructureDb::CHUNK_SIZE = WorldCoords::CHUNK_SIZE;

static bool parse_rule_type(const String& p_type, RuleType& r_type) {
    if (p_type == "spawn_entity" || p_type == "spawn_point") {
        r_type = RuleType::SPAWN_ENTITY;
        return true;
    }
    if (p_type == "spawn_loot_table" || p_type == "loot_table") {
        r_type = RuleType::SPAWN_LOOT_TABLE;
        return true;
    }
    if (p_type == "spawn_item") {
        r_type = RuleType::SPAWN_ITEM;
        return true;
    }
    if (p_type == "set_metadata") {
        r_type = RuleType::SET_METADATA;
        return true;
    }
    return false;
}

void StructureDb::_bind_methods() {
    ClassDB::bind_static_method("StructureDb", D_METHOD("get_singleton"), &StructureDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &StructureDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_tile_at", "id", "x", "y"), &StructureDb::get_tile_at);
    ClassDB::bind_method(D_METHOD("get_ids"), &StructureDb::get_ids);
    ClassDB::bind_method(D_METHOD("get_blueprint", "id"), &StructureDb::get_blueprint);
    ClassDB::bind_method(D_METHOD("get_palette", "id"), &StructureDb::get_palette);
}

StructureDb::StructureDb() {}
StructureDb::~StructureDb() {}

StructureInfo StructureDb::_parse_row(const Dictionary &p_data) {
    IdRegistry* id_reg = IdRegistry::get_singleton();
    StructureInfo info;
    String structure_id = String(p_data.get("id", ""));

    if (id_reg) {
        id_reg->register_string(structure_id);
    }

    info.blueprint = p_data.get("blueprint", "");
    info.palette = p_data.get("palette", Array());
    Array rules = p_data.get("rules", Array());
    for (int i = 0; i < rules.size(); i++) {
        if (rules[i].get_type() != Variant::DICTIONARY) continue;
        Dictionary rule_data = rules[i];

        StructureRuleInfo rule;
        rule.pos = variant_to_vector2i(rule_data.get("pos", Array()), Vector2i());
        rule.entity = String(rule_data.get("entity", rule_data.get("race_id", "")));
        rule.job = String(rule_data.get("job", ""));
        rule.dialogue_profile = String(rule_data.get("dialogue_profile", ""));
        rule.params = rule_data;

        String type_str = String(rule_data.get("type", ""));
        if (!parse_rule_type(type_str, rule.type)) {
            UtilityFunctions::push_error("[StructureDb] Unknown rule type in structure ", structure_id, ": ", type_str);
            continue;
        }

        String loot_table = String(rule_data.get("loot_table", ""));
        if (id_reg && !loot_table.is_empty()) {
            rule.loot_table = id_reg->register_string(loot_table);
        }

        String item_id_str = String(rule_data.get("item_id", ""));
        if (id_reg && !item_id_str.is_empty()) {
            rule.item_id = id_reg->register_string(item_id_str);
        }
        rule.amount = static_cast<int>(rule_data.get("amount", Variant(0)));

        if (rule.pos.x < 0 || rule.pos.x >= CHUNK_SIZE || rule.pos.y < 0 || rule.pos.y >= CHUNK_SIZE) {
            UtilityFunctions::push_error("[StructureDb] Rule in structure ", structure_id, " has out-of-bounds pos: ", rule.pos);
        }

        info.rules.push_back(rule);
    }

    std::vector<uint16_t> palette_ids;
    Array p_array = info.palette;
    for (int i = 0; i < p_array.size(); i++) {
        if (id_reg) {
            palette_ids.push_back(id_reg->register_string(p_array[i]));
        } else {
            palette_ids.push_back(0);
        }
    }

    const int total_tiles = CHUNK_SIZE * CHUNK_SIZE;
    info.data.assign(total_tiles, 0);

    String rle = info.blueprint;
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

String StructureDb::get_blueprint(const String &p_id) const {
    const StructureInfo* info = get_info(p_id);
    return info ? info->blueprint : "";
}

Array StructureDb::get_palette(const String &p_id) const {
    const StructureInfo* info = get_info(p_id);
    return info ? info->palette : Array();
}

const StructureInfo* StructureDb::get_structure_info(const String &p_id) const {
    return get_info(p_id);
}

uint16_t StructureDb::get_tile_at(const String &p_structure_id, int p_x, int p_y) const {
    const StructureInfo* info = get_info(p_structure_id);
    if (!info) return 0;
    if (p_x < 0 || p_x >= CHUNK_SIZE || p_y < 0 || p_y >= CHUNK_SIZE) return 0;

    int idx = p_y * CHUNK_SIZE + p_x;
    if (idx < 0 || idx >= (int)info->data.size()) return 0;

    return info->data[idx];
}

}
