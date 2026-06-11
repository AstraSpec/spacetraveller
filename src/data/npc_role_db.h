#ifndef SPACETRAVELLER_NPC_ROLE_DB_H
#define SPACETRAVELLER_NPC_ROLE_DB_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/string.hpp>
#include "database.h"

namespace godot {

struct NpcRoleInfo {
    String id;
    String display_name;
    String default_attitude;
    String default_ai_state;
    String default_dialogue_profile;
};

class NpcRoleDb : public Object, public DataBase<NpcRoleInfo, NpcRoleDb> {
    GDCLASS(NpcRoleDb, Object)

protected:
    static void _bind_methods();
    virtual NpcRoleInfo _parse_row(const Dictionary &p_data) override;

public:
    NpcRoleDb();
    ~NpcRoleDb();

    void initialize_data() { DataBase<NpcRoleInfo, NpcRoleDb>::initialize_data("res://data/npc_roles"); }
    Array get_ids() const { return DataBase<NpcRoleInfo, NpcRoleDb>::get_ids(); }

    const NpcRoleInfo* get_role_info(const String &p_id) const;
    String get_default_attitude(const String &p_id) const;
    String get_default_ai_state(const String &p_id) const;
    String get_default_dialogue_profile(const String &p_id) const;
};

}

#endif // SPACETRAVELLER_NPC_ROLE_DB_H
