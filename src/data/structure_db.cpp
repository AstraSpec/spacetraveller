#include "structure_db.h"
#include "core/id_registry.h"
#include "core/world_coords.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>

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

static int variant_to_level(const Variant& p_value, int p_default = 0) {
    if (p_value.get_type() == Variant::STRING) {
        return String(p_value).to_int();
    }
    return static_cast<int>(p_value);
}

static void parse_rules(
    const Array& p_rules,
    const String& p_structure_id,
    StructureLevelInfo& r_level,
    const StructureDb* p_db,
    IdRegistry* p_id_reg
) {
    for (int i = 0; i < p_rules.size(); i++) {
        if (p_rules[i].get_type() != Variant::DICTIONARY) continue;
        Dictionary rule_data = p_rules[i];

        StructureRuleInfo rule;
        rule.pos = p_db->variant_to_vector2i(rule_data.get("pos", Array()), Vector2i());
        rule.entity = String(rule_data.get("entity", rule_data.get("race_id", "")));
        rule.job = String(rule_data.get("job", ""));
        rule.dialogue_profile = String(rule_data.get("dialogue_profile", ""));
        rule.attitude = String(rule_data.get("attitude", ""));
        rule.role = String(rule_data.get("role", ""));
        rule.ai_state = String(rule_data.get("ai_state", ""));
        rule.params = rule_data;

        String type_str = String(rule_data.get("type", ""));
        if (!parse_rule_type(type_str, rule.type)) {
            UtilityFunctions::push_error("[StructureDb] Unknown rule type in structure ", p_structure_id, ": ", type_str);
            continue;
        }

        String loot_table = String(rule_data.get("loot_table", ""));
        if (p_id_reg && !loot_table.is_empty()) {
            rule.loot_table = p_id_reg->register_string(loot_table);
        }

        String item_id_str = String(rule_data.get("item_id", ""));
        if (p_id_reg && !item_id_str.is_empty()) {
            rule.item_id = p_id_reg->register_string(item_id_str);
        }
        rule.amount = static_cast<int>(rule_data.get("amount", Variant(0)));

        if (rule.pos.x < 0 || rule.pos.x >= WorldCoords::CHUNK_SIZE || rule.pos.y < 0 || rule.pos.y >= WorldCoords::CHUNK_SIZE) {
            UtilityFunctions::push_error("[StructureDb] Rule in structure ", p_structure_id, " has out-of-bounds pos: ", rule.pos);
        }

        r_level.rules.push_back(rule);
    }
}

static StructureLevelInfo parse_level(
    const Dictionary& p_data,
    const String& p_structure_id,
    const StructureDb* p_db,
    IdRegistry* p_id_reg
) {
    StructureLevelInfo level;
    level.blueprint = p_data.get("blueprint", "");
    level.palette = p_data.get("palette", Array());
    parse_rules(p_data.get("rules", Array()), p_structure_id, level, p_db, p_id_reg);

    std::vector<uint16_t> palette_ids;
    for (int i = 0; i < level.palette.size(); i++) {
        if (p_id_reg) {
            palette_ids.push_back(p_id_reg->register_string(level.palette[i]));
        } else {
            palette_ids.push_back(0);
        }
    }

    const int total_tiles = WorldCoords::CHUNK_SIZE * WorldCoords::CHUNK_SIZE;
    level.data.assign(total_tiles, 0);

    String rle = level.blueprint;
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
            level.data[current_pos++] = tile_id;
        }
    }

    return level;
}

void StructureDb::_bind_methods() {
    ClassDB::bind_static_method("StructureDb", D_METHOD("get_singleton"), &StructureDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &StructureDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_tile_at", "id", "x", "y"), static_cast<uint16_t (StructureDb::*)(const String&, int, int) const>(&StructureDb::get_tile_at));
    ClassDB::bind_method(D_METHOD("get_ids"), &StructureDb::get_ids);
    ClassDB::bind_method(D_METHOD("get_blueprint", "id"), &StructureDb::get_blueprint);
    ClassDB::bind_method(D_METHOD("get_palette", "id"), &StructureDb::get_palette);
    ClassDB::bind_method(D_METHOD("get_levels", "id"), &StructureDb::get_levels);
}

StructureDb::StructureDb() {}
StructureDb::~StructureDb() {}

void StructureDb::initialize_data() {
    structures_by_type.clear();
    DataBase::initialize_data("res://data/structures");

    for (const auto& pair : cache) {
        const StructureInfo& info = pair.second;
        structures_by_type[info.type].push_back(pair.first);
    }

    for (auto& pair : structures_by_type) {
        std::sort(pair.second.begin(), pair.second.end());
    }
}

StructureInfo StructureDb::_parse_row(const Dictionary &p_data) {
    IdRegistry* id_reg = IdRegistry::get_singleton();
    StructureInfo info;
    String structure_id = String(p_data.get("id", ""));
    info.type = String(p_data.get("type", "building"));

    if (id_reg) {
        id_reg->register_string(structure_id);
    }

    if (p_data.has("blueprint") || p_data.has("palette") || p_data.has("rules")) {
        StructureLevelInfo level_zero = parse_level(p_data, structure_id, this, id_reg);
        info.levels[0] = level_zero;
    }

    Variant levels_var = p_data.get("levels", Variant());
    if (levels_var.get_type() == Variant::DICTIONARY) {
        Dictionary levels = levels_var;
        Array keys = levels.keys();
        for (int i = 0; i < keys.size(); i++) {
            Variant key_var = keys[i];
            Variant value = levels[key_var];
            if (value.get_type() != Variant::DICTIONARY) continue;
            int z = variant_to_level(key_var);
            Dictionary level_data = value;
            info.levels[z] = parse_level(level_data, structure_id, this, id_reg);
        }
    } else if (levels_var.get_type() == Variant::ARRAY) {
        Array levels = levels_var;
        for (int i = 0; i < levels.size(); i++) {
            if (levels[i].get_type() != Variant::DICTIONARY) continue;
            Dictionary level_data = levels[i];
            int z = variant_to_level(level_data.get("z", level_data.get("level", 0)));
            info.levels[z] = parse_level(level_data, structure_id, this, id_reg);
        }
    }

    auto level_zero_it = info.levels.find(0);
    if (level_zero_it != info.levels.end()) {
        info.data = level_zero_it->second.data;
        info.blueprint = level_zero_it->second.blueprint;
        info.palette = level_zero_it->second.palette;
        info.rules = level_zero_it->second.rules;
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

Dictionary StructureDb::get_levels(const String &p_id) const {
    Dictionary result;
    const StructureInfo* info = get_info(p_id);
    if (!info) return result;

    for (const auto& pair : info->levels) {
        const int z = pair.first;
        const StructureLevelInfo& level = pair.second;

        Dictionary level_data;
        level_data["blueprint"] = level.blueprint;
        level_data["palette"] = level.palette;

        Array rules;
        for (const StructureRuleInfo& rule : level.rules) {
            rules.push_back(rule.params);
        }
        if (!rules.is_empty()) {
            level_data["rules"] = rules;
        }

        result[String::num_int64(z)] = level_data;
    }

    return result;
}

const StructureInfo* StructureDb::get_structure_info(const String &p_id) const {
    return get_info(p_id);
}

const std::vector<String>* StructureDb::get_structure_ids_by_type(const String& p_type) const {
    auto it = structures_by_type.find(p_type);
    return it != structures_by_type.end() ? &it->second : nullptr;
}

uint16_t StructureDb::get_tile_at(const String &p_structure_id, int p_x, int p_y) const {
    return get_tile_at(p_structure_id, p_x, p_y, 0);
}

uint16_t StructureDb::get_tile_at(const String &p_structure_id, int p_x, int p_y, int p_z) const {
    const StructureInfo* info = get_info(p_structure_id);
    if (!info) return 0;
    if (p_x < 0 || p_x >= CHUNK_SIZE || p_y < 0 || p_y >= CHUNK_SIZE) return 0;

    auto level_it = info->levels.find(p_z);
    if (level_it == info->levels.end()) return 0;
    const StructureLevelInfo& level = level_it->second;

    int idx = p_y * CHUNK_SIZE + p_x;
    if (idx < 0 || idx >= (int)level.data.size()) return 0;

    return level.data[idx];
}

}
