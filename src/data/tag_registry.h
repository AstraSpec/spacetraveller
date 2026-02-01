#ifndef SPACETRAVELLER_TAG_REGISTRY_H
#define SPACETRAVELLER_TAG_REGISTRY_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/array.hpp>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace godot {

class TagRegistry : public Object {
    GDCLASS(TagRegistry, Object)

private:
    static TagRegistry *singleton;
    std::unordered_map<String, uint16_t> tag_to_id;
    std::vector<String> id_to_tag;

protected:
    static void _bind_methods();

public:
    static TagRegistry *get_singleton() { return singleton; }
    static void create_singleton() { if (!singleton) singleton = memnew(TagRegistry); }
    static void delete_singleton() { if (singleton) { memdelete(singleton); singleton = nullptr; } }

    TagRegistry();
    ~TagRegistry();

    uint16_t register_tag(const String &p_tag);
    uint16_t get_tag_id(const String &p_tag) const;
    String get_tag_name(uint16_t p_id) const;

    static bool has_tag(uint16_t p_id, const std::vector<uint16_t> &p_tags);
};

}

#endif // ! SPACETRAVELLER_TAG_REGISTRY_H
