extends Node2D

@export var _StructureEditor :Node2D
@export var World :GameWorld
@export var BG :TextureRect
@export var Camera :ViewCamera

const CUSTOM_SPACING :int = 0
const TILESHEET := preload("res://gfx/urizen.png")
var renderer :FastTileMap
var editor_offset :Vector2i
var configured_editor_area_size := Vector2i.ZERO

func _ready() -> void:
	TileDb.initialize_data()
	TileGroupDb.initialize_data()
	ChunkDb.initialize_data()
	StructureDb.initialize_data()
	ItemDb.initialize_data()
	LootDb.initialize_data()
	OreDb.initialize_data()
	RaceDb.initialize_data()
	FactionDb.initialize_data()
	JobDb.initialize_data()
	EntityGroupDb.initialize_data()

	World.setup_renderer()
	renderer = World.get_renderer()
	renderer.tilesheet = TILESHEET
	renderer.set_occlusion_enabled(false)

	var CHUNK_SIZE := int(GameWorld.get_chunk_size())
	var half_chunk := int(CHUNK_SIZE / 2)
	editor_offset = Vector2i(half_chunk, half_chunk)

	renderer.set_spacing(CUSTOM_SPACING)
	renderer.set_world_bubble_size(CHUNK_SIZE)
	World.set_world_seed(randi())
	World.init_world_bubble(editor_offset, true)
	World.update_world_bubble(editor_offset)

	Camera.make_current()
	Camera.locked = false

	_StructureEditor.World = World
	_StructureEditor.FastTilemap = renderer
	_StructureEditor.spacing = CUSTOM_SPACING
	_StructureEditor.start_editor(Vector2(editor_offset))
	_StructureEditor.editor_area_changed.connect(_on_editor_area_changed)
	_configure_editor_area(_StructureEditor.editor_area_size)
	InputManager.reset_stack(InputManager.InputMode.STRUCTURE)

func _on_editor_area_changed(size: Vector2i) -> void:
	_configure_editor_area(size)

func _configure_editor_area(size: Vector2i) -> void:
	if size == configured_editor_area_size:
		return
	configured_editor_area_size = size
	var bubble_size := maxi(size.x, size.y)
	if bubble_size % 2 != 0:
		bubble_size += 1
	var cell_size := renderer.get_cell_size()
	var half_bubble := int(bubble_size / 2)
	editor_offset = Vector2i(half_bubble, half_bubble)
	_StructureEditor.playerOffset = Vector2(editor_offset)
	renderer.set_world_bubble_size(bubble_size)
	World.init_world_bubble(editor_offset, true)
	for x in range(0, bubble_size):
		for y in range(0, bubble_size):
			World.place_tile(x, y, "void")
	World.update_world_bubble(editor_offset)

	BG.size = Vector2(size * cell_size)
	BG.position = Vector2(_StructureEditor.editor_area_origin * cell_size)
	Camera.position = BG.position + BG.size / 2.0
	_StructureEditor._update_chunk_visual()
