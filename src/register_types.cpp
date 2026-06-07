#include "register_types.h"

#include "world/game_world.h"
#include "world/fast_tilemap.h"
#include "structure_editor.h"
#include "data/tile_db.h"
#include "data/chunk_db.h"
#include "data/item_db.h"
#include "data/recipe_db.h"
#include "data/structure_db.h"
#include "core/id_registry.h"
#include "core/tag_registry.h"
#include "data/body_part_db.h"
#include "data/race_db.h"
#include "data/style_db.h"
#include "data/ability_db.h"
#include "data/name_db.h"
#include "data/quest_db.h"
#include "data/job_db.h"
#include "data/spawn_db.h"
#include "data/loot_db.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/engine.hpp>

using namespace godot;

void initialize_game_world_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	GDREGISTER_RUNTIME_CLASS(FastTileMap);
	GDREGISTER_RUNTIME_CLASS(GameWorld);
	GDREGISTER_RUNTIME_CLASS(StructureEditor);
	GDREGISTER_CLASS(TileDb);
	GDREGISTER_CLASS(ChunkDb);
	GDREGISTER_CLASS(ItemDb);
	GDREGISTER_CLASS(RecipeDb);
	GDREGISTER_CLASS(StructureDb);
	GDREGISTER_CLASS(IdRegistry);
	GDREGISTER_CLASS(TagRegistry);
	GDREGISTER_CLASS(BodyPartDb);
	GDREGISTER_CLASS(RaceDb);
	GDREGISTER_CLASS(StyleDb);
	GDREGISTER_CLASS(AbilityDb);
	GDREGISTER_CLASS(NameDb);
	GDREGISTER_CLASS(QuestDb);
	GDREGISTER_CLASS(JobDb);
	GDREGISTER_CLASS(SpawnDb);
	GDREGISTER_CLASS(LootDb);

	TileDb::create_singleton();
	Engine::get_singleton()->register_singleton("TileDb", TileDb::get_singleton());

	ChunkDb::create_singleton();
	Engine::get_singleton()->register_singleton("ChunkDb", ChunkDb::get_singleton());

	ItemDb::create_singleton();
	Engine::get_singleton()->register_singleton("ItemDb", ItemDb::get_singleton());

	RecipeDb::create_singleton();
	Engine::get_singleton()->register_singleton("RecipeDb", RecipeDb::get_singleton());

	StructureDb::create_singleton();
	Engine::get_singleton()->register_singleton("StructureDb", StructureDb::get_singleton());

	IdRegistry::create_singleton();
	Engine::get_singleton()->register_singleton("IdRegistry", IdRegistry::get_singleton());

	TagRegistry::create_singleton();
	Engine::get_singleton()->register_singleton("TagRegistry", TagRegistry::get_singleton());

	BodyPartDb::create_singleton();
	Engine::get_singleton()->register_singleton("BodyPartDb", BodyPartDb::get_singleton());

	RaceDb::create_singleton();
	Engine::get_singleton()->register_singleton("RaceDb", RaceDb::get_singleton());

	StyleDb::create_singleton();
	Engine::get_singleton()->register_singleton("StyleDb", StyleDb::get_singleton());

	AbilityDb::create_singleton();
	Engine::get_singleton()->register_singleton("AbilityDb", AbilityDb::get_singleton());

	NameDb::create_singleton();
	Engine::get_singleton()->register_singleton("NameDb", NameDb::get_singleton());

	QuestDb::create_singleton();
	Engine::get_singleton()->register_singleton("QuestDb", QuestDb::get_singleton());

	JobDb::create_singleton();
	Engine::get_singleton()->register_singleton("JobDb", JobDb::get_singleton());

	SpawnDb::create_singleton();
	Engine::get_singleton()->register_singleton("SpawnDb", SpawnDb::get_singleton());

	LootDb::create_singleton();
	Engine::get_singleton()->register_singleton("LootDb", LootDb::get_singleton());
}

void uninitialize_game_world_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	Engine::get_singleton()->unregister_singleton("TileDb");
	TileDb::delete_singleton();

	Engine::get_singleton()->unregister_singleton("ChunkDb");
	ChunkDb::delete_singleton();

	Engine::get_singleton()->unregister_singleton("ItemDb");
	ItemDb::delete_singleton();

	Engine::get_singleton()->unregister_singleton("RecipeDb");
	RecipeDb::delete_singleton();

	Engine::get_singleton()->unregister_singleton("StructureDb");
	StructureDb::delete_singleton();

	Engine::get_singleton()->unregister_singleton("IdRegistry");
	IdRegistry::delete_singleton();

	Engine::get_singleton()->unregister_singleton("TagRegistry");
	TagRegistry::delete_singleton();

	Engine::get_singleton()->unregister_singleton("BodyPartDb");
	BodyPartDb::delete_singleton();

	Engine::get_singleton()->unregister_singleton("RaceDb");
	RaceDb::delete_singleton();

	Engine::get_singleton()->unregister_singleton("StyleDb");
	StyleDb::delete_singleton();

	Engine::get_singleton()->unregister_singleton("AbilityDb");
	AbilityDb::delete_singleton();

	Engine::get_singleton()->unregister_singleton("NameDb");
	NameDb::delete_singleton();

	Engine::get_singleton()->unregister_singleton("QuestDb");
	QuestDb::delete_singleton();

	Engine::get_singleton()->unregister_singleton("JobDb");
	JobDb::delete_singleton();

	Engine::get_singleton()->unregister_singleton("SpawnDb");
	SpawnDb::delete_singleton();

	Engine::get_singleton()->unregister_singleton("LootDb");
	LootDb::delete_singleton();
}

extern "C" {
// Initialization.
GDExtensionBool GDE_EXPORT library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(initialize_game_world_module);
	init_obj.register_terminator(uninitialize_game_world_module);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}
