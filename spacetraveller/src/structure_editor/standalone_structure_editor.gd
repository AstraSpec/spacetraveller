extends Node2D

@export var _StructureEditor :Node2D
@export var World :GameWorld
@export var BG :TextureRect
@export var Camera :ViewCamera

const CUSTOM_SPACING :int = 0
const TILESHEET := preload("res://gfx/urizen.png")

func _ready() -> void:
	TileDb.initialize_data()
	StructureDb.initialize_data()
	ItemDb.initialize_data()
	LootDb.initialize_data()
	RaceDb.initialize_data()
	JobDb.initialize_data()

	World.setup_renderer()
	var renderer := World.get_renderer()
	renderer.tilesheet = TILESHEET
	renderer.set_occlusion_enabled(false)

	var CHUNK_SIZE := int(GameWorld.get_chunk_size())
	var half_chunk := int(CHUNK_SIZE / 2)
	var editor_offset := Vector2i(half_chunk, half_chunk)
	var chunk_pixel_size := int(CHUNK_SIZE * renderer.get_cell_size())

	renderer.set_spacing(CUSTOM_SPACING)
	renderer.set_world_bubble_size(CHUNK_SIZE)
	World.set_world_seed(randi())
	World.init_world_bubble(editor_offset, true)
	World.update_world_bubble(editor_offset)

	BG.size = Vector2(
		chunk_pixel_size,
		chunk_pixel_size
	)
	BG.position = -Vector2(
		chunk_pixel_size / 2.0,
		chunk_pixel_size / 2.0
	)

	Camera.make_current()
	Camera.locked = false
	Camera._view_centered()

	_StructureEditor.World = World
	_StructureEditor.FastTilemap = renderer
	_StructureEditor.spacing = CUSTOM_SPACING
	_StructureEditor.start_editor(Vector2(editor_offset))
	InputManager.reset_stack(InputManager.InputMode.STRUCTURE)
