#ifndef SPACETRAVELLER_REGISTER_TYPES_H
#define SPACETRAVELLER_REGISTER_TYPES_H

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void initialize_world_generation_module(ModuleInitializationLevel p_level);
void uninitialize_world_generation_module(ModuleInitializationLevel p_level);

#endif // ! SPACETRAVELLER_REGISTER_TYPES_H