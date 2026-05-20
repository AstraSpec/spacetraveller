#include "tag_registry.h"
#include <godot_cpp/core/class_db.hpp>
#include <algorithm>

namespace godot {

TagRegistry *TagRegistry::singleton = nullptr;

void TagRegistry::create_singleton() {
    if (!singleton) {
        singleton = memnew(TagRegistry);
    }
}

void TagRegistry::delete_singleton() {
    if (singleton) {
        memdelete(singleton);
        singleton = nullptr;
    }
}

void TagRegistry::_bind_methods() {
    ClassDB::bind_static_method("TagRegistry", D_METHOD("get_singleton"), &TagRegistry::get_singleton);
    ClassDB::bind_method(D_METHOD("register_tag", "tag"), &TagRegistry::register_tag);
    ClassDB::bind_method(D_METHOD("get_tag_id", "tag"), &TagRegistry::get_tag_id);
    ClassDB::bind_method(D_METHOD("get_tag_name", "id"), &TagRegistry::get_tag_name);
}

TagRegistry::TagRegistry() {
    // ID 0 is reserved for invalid/no tag
    id_to_tag.push_back("INVALID");
}

TagRegistry::~TagRegistry() {}

uint16_t TagRegistry::register_tag(const String &p_tag) {
    auto it = tag_to_id.find(p_tag);
    if (it != tag_to_id.end()) {
        return it->second;
    }

    uint16_t id = static_cast<uint16_t>(id_to_tag.size());
    tag_to_id[p_tag] = id;
    id_to_tag.push_back(p_tag);
    return id;
}

uint16_t TagRegistry::get_tag_id(const String &p_tag) const {
    auto it = tag_to_id.find(p_tag);
    return (it != tag_to_id.end()) ? it->second : 0;
}

String TagRegistry::get_tag_name(uint16_t p_id) const {
    if (p_id < id_to_tag.size()) {
        return id_to_tag[p_id];
    }
    return "INVALID";
}

bool TagRegistry::has_tag(uint16_t p_id, const std::vector<uint16_t> &p_tags) {
    if (p_id == 0) return false;
    return std::binary_search(p_tags.begin(), p_tags.end(), p_id);
}

}
