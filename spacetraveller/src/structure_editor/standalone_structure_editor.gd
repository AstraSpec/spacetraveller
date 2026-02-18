extends Node2D

@export var _StructureEditor :Node2D
@export var FastTilemap :FastTileMap
@export var BG :TextureRect
@export var Camera :ViewCamera

const CUSTOM_SPACING :int = 1

func _ready() -> void:
	TileDb.initialize_data()
	StructureDb.initialize_data()
	
	var CHUNK_SIZE = WorldGeneration.get_chunk_size()
	FastTilemap.set_spacing(CUSTOM_SPACING)
	FastTilemap.set_world_bubble_size(CHUNK_SIZE)
	FastTilemap.init_world_bubble(Vector2i(0, 0), true)
	FastTilemap.update_visuals(Vector2i(0, 0))
	
	BG.size = Vector2(
		CHUNK_SIZE * FastTilemap.get_cell_size(),
		CHUNK_SIZE * FastTilemap.get_cell_size()
	)
	BG.position = -Vector2(
		CHUNK_SIZE * FastTilemap.get_cell_size() / 2.0,
		CHUNK_SIZE * FastTilemap.get_cell_size() / 2.0
	)
	
	Camera._view_centered()
	
	_StructureEditor.FastTilemap = FastTilemap
	_StructureEditor.spacing = CUSTOM_SPACING
	_StructureEditor.start_editor()
