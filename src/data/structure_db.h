#ifndef SPACETRAVELLER_STRUCTURE_DB_H
#define SPACETRAVELLER_STRUCTURE_DB_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include "database.h"
#include <vector>

namespace godot {

enum class RuleType : uint8_t {
    SPAWN_ENTITY,
    SPAWN_LOOT_TABLE,
    SPAWN_ITEM,
    SET_METADATA
};

struct StructureRuleInfo {
    RuleType type = RuleType::SPAWN_ENTITY;
    Vector2i pos;
    String entity;
    String job;
    String dialogue_profile;
    String attitude;
    String role;
    String ai_state;
    uint16_t loot_table = 0;
    uint16_t item_id = 0;
    int amount = 0;
    Dictionary params;
};

struct StructureInfo {
    std::vector<uint16_t> data;
    String blueprint;
    Array palette;
    std::vector<StructureRuleInfo> rules;
};

class StructureDb : public Object, public DataBase<StructureInfo, StructureDb> {
    GDCLASS(StructureDb, Object)

private:
    static const int CHUNK_SIZE;

protected:
    static void _bind_methods();
    virtual StructureInfo _parse_row(const Dictionary &p_data) override;

public:
    StructureDb();
    ~StructureDb();

    void initialize_data() { DataBase::initialize_data("res://data/structures"); }
    Array get_ids() const { return DataBase::get_ids(); }

    String get_blueprint(const String &p_id) const;
    Array get_palette(const String &p_id) const;

    // Fast C++ access
    const StructureInfo* get_structure_info(const String &p_id) const;
    uint16_t get_tile_at(const String &p_structure_id, int p_x, int p_y) const;
};

}

#endif // ! SPACETRAVELLER_STRUCTURE_DB_H
