#include "register_types.h"

#include "world_generation.h"
#include "fast_tilemap.h"
#include "structure_editor.h"
#include "data/tile_db.h"
#include "data/chunk_db.h"
#include "data/structure_db.h"
#include "data/id_registry.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/engine.hpp>

using namespace godot;

void initialize_world_generation_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	GDREGISTER_RUNTIME_CLASS(FastTileMap);
	GDREGISTER_RUNTIME_CLASS(WorldGeneration);
	GDREGISTER_RUNTIME_CLASS(StructureEditor);
	GDREGISTER_CLASS(TileDb);
	GDREGISTER_CLASS(ChunkDb);
	GDREGISTER_CLASS(StructureDb);
	GDREGISTER_CLASS(IdRegistry);

	TileDb::create_singleton();
	Engine::get_singleton()->register_singleton("TileDb", TileDb::get_singleton());

	ChunkDb::create_singleton();
	Engine::get_singleton()->register_singleton("ChunkDb", ChunkDb::get_singleton());

	StructureDb::create_singleton();
	Engine::get_singleton()->register_singleton("StructureDb", StructureDb::get_singleton());

	IdRegistry::create_singleton();
	Engine::get_singleton()->register_singleton("IdRegistry", IdRegistry::get_singleton());
}

void uninitialize_world_generation_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	Engine::get_singleton()->unregister_singleton("TileDb");
	TileDb::delete_singleton();

	Engine::get_singleton()->unregister_singleton("ChunkDb");
	ChunkDb::delete_singleton();

	Engine::get_singleton()->unregister_singleton("StructureDb");
	StructureDb::delete_singleton();

	Engine::get_singleton()->unregister_singleton("IdRegistry");
	IdRegistry::delete_singleton();
}

extern "C" {
// Initialization.
GDExtensionBool GDE_EXPORT library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(initialize_world_generation_module);
	init_obj.register_terminator(uninitialize_world_generation_module);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}
