#ifndef SPACETRAVELLER_STRUCTURE_DB_H
#define SPACETRAVELLER_STRUCTURE_DB_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include "database.h"
#include <vector>
#include <unordered_map>

namespace godot {

enum DungeonRoomEntranceMask : uint8_t {
    DUNGEON_ROOM_ENTRANCE_NORTH = 1 << 0,
    DUNGEON_ROOM_ENTRANCE_SOUTH = 1 << 1,
    DUNGEON_ROOM_ENTRANCE_EAST = 1 << 2,
    DUNGEON_ROOM_ENTRANCE_WEST = 1 << 3,
    DUNGEON_ROOM_ENTRANCE_ALL = DUNGEON_ROOM_ENTRANCE_NORTH |
        DUNGEON_ROOM_ENTRANCE_SOUTH |
        DUNGEON_ROOM_ENTRANCE_EAST |
        DUNGEON_ROOM_ENTRANCE_WEST
};

enum class RuleType : uint8_t {
    SPAWN_ENTITY,
    SPAWN_ENTITY_GROUP,
    SPAWN_LOOT_TABLE,
    SPAWN_ITEM,
    SET_METADATA
};

struct StructureRuleInfo {
    RuleType type = RuleType::SPAWN_ENTITY;
    Vector2i pos;
    String entity;
    String entity_group;
    String job;
    String dialogue_profile;
    String attitude;
    String ai_state;
    uint16_t loot_table = 0;
    uint16_t item_id = 0;
    int amount = 0;
    Dictionary params;
};

struct StructureLevelInfo {
    std::vector<uint16_t> data;
    String blueprint;
    Array palette;
    std::vector<StructureRuleInfo> rules;
    Vector2i size = Vector2i(24, 24);
};

struct StructureInfo {
    std::vector<uint16_t> data;
    String blueprint;
    Array palette;
    std::vector<StructureRuleInfo> rules;
    std::unordered_map<int, StructureLevelInfo> levels;
    String type = "";
    Vector2i size = Vector2i(24, 24);
    uint8_t dungeon_room_entrance_mask = DUNGEON_ROOM_ENTRANCE_ALL;
};

class StructureDb : public Object, public DataBase<StructureInfo, StructureDb> {
    GDCLASS(StructureDb, Object)

private:
    static const int CHUNK_SIZE;
    std::unordered_map<String, std::vector<String>, StringHasher> structures_by_type;

protected:
    static void _bind_methods();
    virtual StructureInfo _parse_row(const Dictionary &p_data) override;

public:
    StructureDb();
    ~StructureDb();

    void initialize_data();
    Array get_ids() const { return DataBase::get_ids(); }

    String get_blueprint(const String &p_id) const;
    Array get_palette(const String &p_id) const;
    Dictionary get_levels(const String &p_id) const;
    String get_structure_type(const String &p_id) const;
    Array get_structure_types() const;
    Array get_dungeon_room_entrances(const String& p_structure_id) const;

    // Fast C++ access
    const StructureInfo* get_structure_info(const String &p_id) const;
    const std::vector<String>* get_structure_ids_by_type(const String& p_type) const;
    uint8_t get_dungeon_room_entrance_mask(const String& p_structure_id) const;
    Vector2i get_structure_size(const String &p_structure_id) const;
    uint16_t get_tile_at(const String &p_structure_id, int p_x, int p_y) const;
    uint16_t get_tile_at(const String &p_structure_id, int p_x, int p_y, int p_z) const;
};

}

#endif // ! SPACETRAVELLER_STRUCTURE_DB_H
