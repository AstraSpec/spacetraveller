#ifndef SPACETRAVELLER_FACTION_DB_H
#define SPACETRAVELLER_FACTION_DB_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <unordered_map>

#include "database.h"
#include "core/faction.h"
#include "core/string_hasher.h"

namespace godot {

struct FactionInfo {
    String id;
    String display_name;
    std::unordered_map<String, FactionRelation, StringHasher> declared_relations;
};

class FactionDb : public Object, public DataBase<FactionInfo, FactionDb> {
    GDCLASS(FactionDb, Object)

protected:
    static void _bind_methods();
    FactionInfo _parse_row(const Dictionary& p_data) override;

private:
    std::unordered_map<String,
        std::unordered_map<String, FactionRelation, StringHasher>,
        StringHasher> resolved_relations;

    void rebuild_relations();

public:
    FactionDb() = default;
    ~FactionDb() = default;

    void initialize_data();
    Array get_ids() const { return DataBase<FactionInfo, FactionDb>::get_ids(); }
    bool has_faction(const String& p_id) const;
    FactionRelation get_relation_value(const String& p_a, const String& p_b) const;
    String get_relation(const String& p_a, const String& p_b) const;
};

}

#endif // SPACETRAVELLER_FACTION_DB_H
