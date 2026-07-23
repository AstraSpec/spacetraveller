#include "structure_db.h"
#include "tile_group_db.h"
#include "core/id_registry.h"
#include "core/world_coords.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace godot {

template<> StructureDb* DataBase<StructureInfo, StructureDb>::singleton = nullptr;
const int StructureDb::CHUNK_SIZE = WorldCoords::CHUNK_SIZE;
static constexpr int64_t MAX_STRUCTURE_CELLS = 1024 * 1024;

static bool is_valid_structure_size(const Vector2i& p_size) {
    const int64_t total = static_cast<int64_t>(p_size.x) * static_cast<int64_t>(p_size.y);
    return p_size.x > 0 && p_size.y > 0 && total <= MAX_STRUCTURE_CELLS;
}

static std::vector<uint8_t> parse_placement_rotations(const Dictionary& p_data, const String& p_structure_id) {
    std::vector<uint8_t> rotations;
    Variant placement_var = p_data.get("placement", Variant());
    if (placement_var.get_type() == Variant::DICTIONARY) {
        Dictionary placement = placement_var;
        Variant rotations_var = placement.get("rotations", Variant());
        if (rotations_var.get_type() == Variant::ARRAY) {
            Array values = rotations_var;
            for (int i = 0; i < values.size(); i++) {
                const int rotation = static_cast<int>(values[i]);
                if (rotation < 0 || rotation > 3) {
                    UtilityFunctions::push_error("[StructureDb] Invalid placement rotation in structure ", p_structure_id, ".");
                    continue;
                }
                const uint8_t value = static_cast<uint8_t>(rotation);
                if (std::find(rotations.begin(), rotations.end(), value) == rotations.end()) {
                    rotations.push_back(value);
                }
            }
        }
    }
    if (rotations.empty()) {
        rotations = { WorldCoords::ROT_SOUTH, WorldCoords::ROT_WEST, WorldCoords::ROT_NORTH, WorldCoords::ROT_EAST };
    }
    return rotations;
}

static bool parse_rule_type(const String& p_type, RuleType& r_type) {
    if (p_type == "spawn_entity") {
        r_type = RuleType::SPAWN_ENTITY;
        return true;
    }
    if (p_type == "spawn_entity_group") {
        r_type = RuleType::SPAWN_ENTITY_GROUP;
        return true;
    }
    if (p_type == "spawn_loot_table") {
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

static bool parse_dungeon_room_side(const String& p_side, uint8_t& r_mask) {
    if (p_side == "north" || p_side == "n") {
        r_mask |= DUNGEON_ROOM_ENTRANCE_NORTH;
        return true;
    }
    if (p_side == "south" || p_side == "s") {
        r_mask |= DUNGEON_ROOM_ENTRANCE_SOUTH;
        return true;
    }
    if (p_side == "east" || p_side == "e") {
        r_mask |= DUNGEON_ROOM_ENTRANCE_EAST;
        return true;
    }
    if (p_side == "west" || p_side == "w") {
        r_mask |= DUNGEON_ROOM_ENTRANCE_WEST;
        return true;
    }
    if (p_side == "all") {
        r_mask |= DUNGEON_ROOM_ENTRANCE_ALL;
        return true;
    }
    return false;
}

static uint8_t parse_dungeon_room_entrance_mask(const Dictionary& p_data, const String& p_structure_id) {
    if (!p_data.has("dungeon_room")) {
        return DUNGEON_ROOM_ENTRANCE_ALL;
    }

    Variant room_var = p_data["dungeon_room"];
    if (room_var.get_type() != Variant::DICTIONARY) {
        UtilityFunctions::push_error("[StructureDb] dungeon_room must be a dictionary in structure ", p_structure_id);
        return DUNGEON_ROOM_ENTRANCE_ALL;
    }

    Dictionary room_data = room_var;
    Variant entrances_var = room_data.get("entrances", Variant());
    if (entrances_var.get_type() != Variant::ARRAY) {
        return DUNGEON_ROOM_ENTRANCE_ALL;
    }

    Array entrances = entrances_var;
    uint8_t mask = 0;
    for (int i = 0; i < entrances.size(); i++) {
        String side = String(entrances[i]).to_lower();
        if (!parse_dungeon_room_side(side, mask)) {
            UtilityFunctions::push_error("[StructureDb] Unknown dungeon room entrance side in structure ", p_structure_id, ": ", side);
        }
    }

    return mask;
}

static std::vector<int> parse_structure_entrances(const Dictionary& p_data, const String& p_structure_id) {
    std::vector<int> entrances;
    Variant entrance_var = p_data.get("entrance", Variant());
    if (entrance_var.get_type() == Variant::NIL) {
        return entrances;
    }
    if (entrance_var.get_type() != Variant::ARRAY) {
        UtilityFunctions::push_error("[StructureDb] entrance must be an array in structure ", p_structure_id);
        return entrances;
    }

    Array entrance_array = entrance_var;
    entrances.reserve(entrance_array.size());
    for (int i = 0; i < entrance_array.size(); i++) {
        entrances.push_back(static_cast<int>(entrance_array[i]));
    }
    return entrances;
}

static bool variant_to_rule_coordinate(const Variant& p_value, int& r_coordinate) {
    int64_t value = 0;
    if (p_value.get_type() == Variant::INT) {
        value = static_cast<int64_t>(p_value);
    } else if (p_value.get_type() == Variant::FLOAT) {
        const double float_value = static_cast<double>(p_value);
        if (!std::isfinite(float_value) || std::floor(float_value) != float_value) {
            return false;
        }
        if (float_value < static_cast<double>(std::numeric_limits<int>::min())
            || float_value > static_cast<double>(std::numeric_limits<int>::max())) {
            return false;
        }
        value = static_cast<int64_t>(float_value);
    } else {
        return false;
    }

    if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max()) {
        return false;
    }
    r_coordinate = static_cast<int>(value);
    return true;
}

static bool variant_to_rule_position(const Variant& p_value, Vector2i& r_position) {
    if (p_value.get_type() != Variant::ARRAY) {
        return false;
    }
    Array position_data = p_value;
    if (position_data.size() != 2) {
        return false;
    }

    int x = 0;
    int y = 0;
    if (!variant_to_rule_coordinate(position_data[0], x)
        || !variant_to_rule_coordinate(position_data[1], y)) {
        return false;
    }
    r_position = Vector2i(x, y);
    return true;
}

static void parse_rules(
    const Array& p_rules,
    const String& p_structure_id,
    StructureLevelInfo& r_level,
    IdRegistry* p_id_reg,
    const Vector2i& p_size
) {
    for (int i = 0; i < p_rules.size(); i++) {
        if (p_rules[i].get_type() != Variant::DICTIONARY) continue;
        Dictionary rule_data = p_rules[i];

        if (rule_data.has("pos")) {
            UtilityFunctions::push_error(
                "[StructureDb] Rule ", i, " in structure ", p_structure_id,
                " uses removed 'pos'; use a non-empty 'positions' array."
            );
            continue;
        }

        Variant positions_var = rule_data.get("positions", Variant());
        if (positions_var.get_type() != Variant::ARRAY) {
            UtilityFunctions::push_error(
                "[StructureDb] Rule ", i, " in structure ", p_structure_id,
                " is missing a valid 'positions' array."
            );
            continue;
        }
        Array positions_data = positions_var;
        if (positions_data.is_empty()) {
            UtilityFunctions::push_error(
                "[StructureDb] Rule ", i, " in structure ", p_structure_id,
                " has an empty 'positions' array."
            );
            continue;
        }

        StructureRuleInfo rule;
        rule.entity = String(rule_data.get("entity", rule_data.get("race_id", "")));
        rule.entity_group = String(rule_data.get("entity_group", rule_data.get("entity_group_id", "")));
        rule.job = String(rule_data.get("job", ""));
        rule.dialogue_id = String(rule_data.get("dialogue_id", ""));
        rule.faction = String(rule_data.get("faction", ""));
        rule.reaction_policy = String(rule_data.get("reaction_policy", ""));
        rule.reaction_radius = static_cast<int>(rule_data.get("reaction_radius", 0));
        rule.ai_state = String(rule_data.get("ai_state", ""));

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

        std::vector<Vector2i> positions;
        positions.reserve(positions_data.size());
        bool positions_valid = true;
        for (int position_index = 0; position_index < positions_data.size(); position_index++) {
            Vector2i position;
            if (!variant_to_rule_position(positions_data[position_index], position)) {
                UtilityFunctions::push_error(
                    "[StructureDb] Rule ", i, " in structure ", p_structure_id,
                    " has invalid position at index ", position_index, "."
                );
                positions_valid = false;
                break;
            }
            if (position.x < 0 || position.x >= p_size.x || position.y < 0 || position.y >= p_size.y) {
                UtilityFunctions::push_error(
                    "[StructureDb] Rule ", i, " in structure ", p_structure_id,
                    " has out-of-bounds position: ", position
                );
                positions_valid = false;
                break;
            }
            positions.push_back(position);
        }
        if (!positions_valid) {
            continue;
        }

        r_level.rule_groups.push_back(rule_data.duplicate(true));
        for (const Vector2i& position : positions) {
            StructureRuleInfo positioned_rule = rule;
            positioned_rule.pos = position;
            positioned_rule.params = rule_data.duplicate(true);
            positioned_rule.params.erase("positions");
            Array position_data;
            position_data.push_back(position.x);
            position_data.push_back(position.y);
            positioned_rule.params["pos"] = position_data;
            r_level.rules.push_back(std::move(positioned_rule));
        }
    }
}

static StructureLevelInfo parse_level(
    const Dictionary& p_data,
    const String& p_structure_id,
    IdRegistry* p_id_reg,
    const Vector2i& p_size
) {
    StructureLevelInfo level;
    level.blueprint = p_data.get("blueprint", "");
    level.palette = p_data.get("palette", Array());
    level.size = p_size;
    parse_rules(p_data.get("rules", Array()), p_structure_id, level, p_id_reg, p_size);

    std::vector<uint16_t> palette_ids;
    std::vector<uint16_t> palette_tile_group_ids;
    TileGroupDb* tile_group_db = TileGroupDb::get_singleton();

    for (int i = 0; i < level.palette.size(); i++) {
        uint16_t tile_id = 0;
        uint16_t tile_group_id = 0;
        bool has_tile_group = false;

        if (level.palette[i].get_type() == Variant::DICTIONARY) {
            Dictionary entry = level.palette[i];
            String tile_str = entry.get("tile", "");
            String tile_group_str = entry.get("tile_group", "");

            if (!tile_group_str.is_empty() && p_id_reg) {
                tile_group_id = p_id_reg->register_string(tile_group_str);
                has_tile_group = true;
                if (tile_group_db) {
                    const TileGroupInfo* tg_info = tile_group_db->get_tile_group(tile_group_str);
                    if (tg_info && !tg_info->entries.empty()) {
                        tile_id = tg_info->entries[0].tile_id;
                    }
                }
            } else if (!tile_str.is_empty() && p_id_reg) {
                tile_id = p_id_reg->register_string(tile_str);
            }
        } else if (p_id_reg) {
            tile_id = p_id_reg->register_string(level.palette[i]);
        }

        palette_ids.push_back(tile_id);
        palette_tile_group_ids.push_back(has_tile_group ? tile_group_id : 0);
    }

    const int total_tiles = level.size.x * level.size.y;
    level.data.reserve(total_tiles);
    level.tile_group_ids.reserve(total_tiles);

    String rle = level.blueprint;
    rle = rle.replace("(", "").replace(")", "").replace("[", "").replace("]", "");
    PackedStringArray parts = rle.split(",");

    for (int i = 0; i < parts.size(); i++) {
        String part = parts[i].strip_edges();
        if (part.is_empty()) continue;

        PackedStringArray sub = part.split("x");
        if (sub.size() != 2 || !sub[0].is_valid_int() || !sub[1].is_valid_int()) {
            UtilityFunctions::push_error("[StructureDb] Malformed RLE in ", p_structure_id, ": ", part);
            return StructureLevelInfo();
        }

        int count = sub[0].to_int();
        int palette_idx = sub[1].to_int();
        if (count <= 0 || palette_idx < 0 || palette_idx >= static_cast<int>(palette_ids.size()) || count > total_tiles - static_cast<int>(level.data.size())) {
            UtilityFunctions::push_error("[StructureDb] Invalid RLE run in ", p_structure_id, ": ", part);
            return StructureLevelInfo();
        }

        for (int j = 0; j < count; j++) {
            level.data.push_back(palette_ids[palette_idx]);
            level.tile_group_ids.push_back(palette_tile_group_ids[palette_idx]);
        }
    }
    if (static_cast<int>(level.data.size()) != total_tiles) {
        UtilityFunctions::push_error("[StructureDb] RLE cell count does not match size for ", p_structure_id, ".");
        return StructureLevelInfo();
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
    ClassDB::bind_method(D_METHOD("get_structure_size", "id"), &StructureDb::get_structure_size);
    ClassDB::bind_method(D_METHOD("get_structure_type", "id"), &StructureDb::get_structure_type);
    ClassDB::bind_method(D_METHOD("get_structure_types"), &StructureDb::get_structure_types);
    ClassDB::bind_method(D_METHOD("get_dungeon_room_entrances", "id"), &StructureDb::get_dungeon_room_entrances);
}

StructureDb::StructureDb() {}
StructureDb::~StructureDb() {}

void StructureDb::initialize_data() {
    structures_by_type.clear();
    DataBase::initialize_data("res://data/structures");

    for (const auto& pair : cache) {
        const StructureInfo& info = pair.second;
        if (info.type.is_empty()) continue;
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
    info.type = String(p_data.get("type", ""));
    info.placement_rotations = parse_placement_rotations(p_data, structure_id);
    info.entrances = parse_structure_entrances(p_data, structure_id);
    info.dungeon_room_entrance_mask = parse_dungeon_room_entrance_mask(p_data, structure_id);
    info.size = variant_to_vector2i(p_data.get("size", Array()), Vector2i(WorldCoords::CHUNK_SIZE, WorldCoords::CHUNK_SIZE));
    if (!is_valid_structure_size(info.size)) {
        UtilityFunctions::push_error("[StructureDb] Invalid structure size for ", structure_id, ".");
        return info;
    }

    if (id_reg) {
        id_reg->register_string(structure_id);
    }

    Variant levels_var = p_data.get("levels", Variant());
    if (levels_var.get_type() != Variant::DICTIONARY) {
        UtilityFunctions::push_error("[StructureDb] Missing levels dictionary for ", structure_id, ".");
        return StructureInfo();
    }
    Dictionary levels = levels_var;
    Array keys = levels.keys();
    for (int i = 0; i < keys.size(); i++) {
        Variant key_var = keys[i];
        Variant value = levels[key_var];
        if (value.get_type() != Variant::DICTIONARY) {
            UtilityFunctions::push_error("[StructureDb] Invalid level in ", structure_id, ".");
            return StructureInfo();
        }
        int z = variant_to_level(key_var);
        Dictionary level_data = value;
        StructureLevelInfo level = parse_level(level_data, structure_id, id_reg, info.size);
        if (level.data.empty()) {
            return StructureInfo();
        }
        info.levels[z] = std::move(level);
    }

    auto level_zero_it = info.levels.find(0);
    if (level_zero_it != info.levels.end()) {
        info.data = level_zero_it->second.data;
        info.blueprint = level_zero_it->second.blueprint;
        info.palette = level_zero_it->second.palette;
        info.rules = level_zero_it->second.rules;
        info.size = level_zero_it->second.size;
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

        if (!level.rule_groups.is_empty()) {
            level_data["rules"] = level.rule_groups.duplicate(true);
        }

        result[String::num_int64(z)] = level_data;
    }

    return result;
}

String StructureDb::get_structure_type(const String &p_id) const {
    const StructureInfo* info = get_info(p_id);
    return info ? info->type : "";
}

Array StructureDb::get_structure_types() const {
    std::vector<String> types;
    types.reserve(structures_by_type.size());
    for (const auto& pair : structures_by_type) {
        if (!pair.first.is_empty()) {
            types.push_back(pair.first);
        }
    }
    std::sort(types.begin(), types.end());

    Array result;
    for (const String& type : types) {
        result.push_back(type);
    }
    return result;
}

Array StructureDb::get_dungeon_room_entrances(const String& p_structure_id) const {
    Array result;
    const uint8_t mask = get_dungeon_room_entrance_mask(p_structure_id);
    if (mask & DUNGEON_ROOM_ENTRANCE_NORTH) {
        result.push_back("north");
    }
    if (mask & DUNGEON_ROOM_ENTRANCE_EAST) {
        result.push_back("east");
    }
    if (mask & DUNGEON_ROOM_ENTRANCE_SOUTH) {
        result.push_back("south");
    }
    if (mask & DUNGEON_ROOM_ENTRANCE_WEST) {
        result.push_back("west");
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

uint8_t StructureDb::get_dungeon_room_entrance_mask(const String& p_structure_id) const {
    const StructureInfo* info = get_info(p_structure_id);
    return info ? info->dungeon_room_entrance_mask : DUNGEON_ROOM_ENTRANCE_ALL;
}

Vector2i StructureDb::get_structure_size(const String &p_structure_id) const {
    const StructureInfo* info = get_info(p_structure_id);
    return info ? info->size : Vector2i();
}

uint16_t StructureDb::get_tile_at(const String &p_structure_id, int p_x, int p_y) const {
    return get_tile_at(p_structure_id, p_x, p_y, 0, 0);
}

uint16_t StructureDb::get_tile_at(const String &p_structure_id, int p_x, int p_y, int p_z) const {
    return get_tile_at(p_structure_id, p_x, p_y, p_z, 0);
}

uint16_t StructureDb::get_tile_at(const String &p_structure_id, int p_x, int p_y, int p_z, uint32_t p_position_hash) const {
    const StructureInfo* info = get_info(p_structure_id);
    if (!info) return 0;

    auto level_it = info->levels.find(p_z);
    if (level_it == info->levels.end()) return 0;
    const StructureLevelInfo& level = level_it->second;
    if (p_x < 0 || p_x >= level.size.x || p_y < 0 || p_y >= level.size.y) return 0;

    int idx = p_y * level.size.x + p_x;
    if (idx < 0 || idx >= (int)level.data.size()) return 0;

    uint16_t tile_id = level.data[idx];

    if (p_position_hash != 0 && idx < (int)level.tile_group_ids.size() && level.tile_group_ids[idx] != 0) {
        IdRegistry* id_reg = IdRegistry::get_singleton();
        TileGroupDb* tg_db = TileGroupDb::get_singleton();
        if (id_reg && tg_db) {
            String group_name = id_reg->get_string(level.tile_group_ids[idx]);
            const TileGroupInfo* tg_info = tg_db->get_tile_group(group_name);
            if (tg_info && tg_info->total_weight > 0) {
                uint32_t roll = p_position_hash % static_cast<uint32_t>(tg_info->total_weight);
                int cumulative = 0;
                for (const auto& entry : tg_info->entries) {
                    if (entry.weight <= 0) continue;
                    cumulative += entry.weight;
                    if (roll < static_cast<uint32_t>(cumulative)) {
                        tile_id = entry.tile_id;
                        break;
                    }
                }
            }
        }
    }

    return tile_id;
}

}
