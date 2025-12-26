#ifndef SPACETRAVELLER_ID_REGISTRY_H
#define SPACETRAVELLER_ID_REGISTRY_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/string.hpp>
#include <unordered_map>
#include <vector>
#include "database.h"

namespace godot {

class IdRegistry : public Object {
    GDCLASS(IdRegistry, Object)

private:
    static IdRegistry *singleton;
    std::unordered_map<String, uint16_t, StringHasher> string_to_id;
    std::vector<String> id_to_string;

protected:
    static void _bind_methods();

public:
    static IdRegistry *get_singleton() { return singleton; }
    static void create_singleton();
    static void delete_singleton();

    IdRegistry();
    ~IdRegistry();

    uint16_t register_string(const String &p_string);
    uint16_t get_id(const String &p_string) const;
    String get_string(uint16_t p_id) const;
    
    // GDScript access
    uint16_t get_id_gd(const String &p_string) const { return get_id(p_string); }
    String get_string_gd(int p_id) const { return get_string(static_cast<uint16_t>(p_id)); }
};

}

#endif // SPACETRAVELLER_ID_REGISTRY_H

