extends Node2D

@export var _StructureEditor :Node2D
@export var World :GameWorld
@export var BG :TextureRect
@export var Camera :ViewCamera

const CUSTOM_SPACING :int = 1
const TILESHEET := preload("res://gfx/urizen.png")

func _ready() -> void:
	TileDb.initialize_data()
	StructureDb.initialize_data()

	World.setup_renderer()
	var renderer := World.get_renderer()
	renderer.tilesheet = TILESHEET

	var CHUNK_SIZE = GameWorld.get_chunk_size()
	renderer.set_spacing(CUSTOM_SPACING)
	renderer.set_world_bubble_size(CHUNK_SIZE)
	World.set_world_seed(randi())
	World.init_world_bubble(Vector2i(0, 0), true)
	World.update_world_bubble(Vector2i(0, 0))

	BG.size = Vector2(
		CHUNK_SIZE * renderer.get_cell_size(),
		CHUNK_SIZE * renderer.get_cell_size()
	)
	BG.position = -Vector2(
		CHUNK_SIZE * renderer.get_cell_size() / 2.0,
		CHUNK_SIZE * renderer.get_cell_size() / 2.0
	)

	Camera._view_centered()

	_StructureEditor.World = World
	_StructureEditor.FastTilemap = renderer
	_StructureEditor.spacing = CUSTOM_SPACING
	_StructureEditor.start_editor()
