#include "id_registry.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

IdRegistry *IdRegistry::singleton = nullptr;

void IdRegistry::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_id", "string"), &IdRegistry::get_id_gd);
    ClassDB::bind_method(D_METHOD("get_string", "id"), &IdRegistry::get_string_gd);
}

void IdRegistry::create_singleton() {
    singleton = memnew(IdRegistry);
    singleton->register_string("void"); // ID 0 by default
}

void IdRegistry::delete_singleton() {
    if (singleton) {
        memdelete(singleton);
        singleton = nullptr;
    }
}

IdRegistry::IdRegistry() {}
IdRegistry::~IdRegistry() {}

uint16_t IdRegistry::register_string(const String &p_string) {
    auto it = string_to_id.find(p_string);
    if (it != string_to_id.end()) {
        return it->second;
    }

    uint16_t id = static_cast<uint16_t>(id_to_string.size());
    string_to_id[p_string] = id;
    id_to_string.push_back(p_string);
    return id;
}

uint16_t IdRegistry::get_id(const String &p_string) const {
    auto it = string_to_id.find(p_string);
    if (it != string_to_id.end()) {
        return it->second;
    }
    return 0; // void
}

String IdRegistry::get_string(uint16_t p_id) const {
    if (p_id < id_to_string.size()) {
        return id_to_string[p_id];
    }
    return "void";
}

