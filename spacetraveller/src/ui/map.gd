extends Control

@onready var MapView :SubViewport = get_node("/root/Main/MapView")
@onready var Camera :ViewCamera = get_node("/root/Main/MapView/Camera")
@onready var Tilemap :TileMapLayer = get_node("/root/Main/MapView/TileMap")
@onready var playerChunk :TextureRect = get_node("/root/Main/MapView/PlayerChunk")
@onready var container :MarginContainer = $MarginContainer

const SOURCE :int = 2
var REGION_SIZE = WorldGeneration.get_region_size()
var TILE_SIZE = FastTileMap.get_tile_size()

func _ready():
	get_window().size_changed.connect(resize_viewport)
	resized.connect(resize_viewport)
	call_deferred("resize_viewport")
	
	InputManager.map_toggled.connect(_map_toggled)
	
	# Configure Camera
	Camera.centerNode = playerChunk
	Camera.margins = container
	Camera.viewport = MapView
	Camera.limits = Rect2(0, 0, REGION_SIZE * TILE_SIZE, REGION_SIZE * TILE_SIZE)
	Camera._view_centered()

func _on_world_generated(regionChunks: Dictionary) -> void:
	if regionChunks.is_empty():
		return
	
	# Determine bounds from any key in the dictionary
	var firstKey = regionChunks.keys()[0]
	var coords = WorldGeneration.unpack_coords(firstKey)
	var coordX = coords.x
	var coordY = coords.y
	
	var startX = floor(float(coordX) / REGION_SIZE) * REGION_SIZE
	var startY = floor(float(coordY) / REGION_SIZE) * REGION_SIZE
	
	for y in range(startY, startY + REGION_SIZE):
		for x in range(startX, startX + REGION_SIZE):
			var key = WorldGeneration.pack_coords(x, y)
			var chunkID = regionChunks.get(key, "")
			
			if chunkID == "":
				continue
				
			var atlas :Vector2i = ChunkDb.get_atlas_coords(chunkID)
			Tilemap.set_cell(Vector2i(x, y), SOURCE, atlas)
	
	MapView.size = container.get_size()

func _map_toggled() -> void:
	visible = !visible
	if visible:
		Camera._view_centered()

func _on_player_moved_chunk(chunkPos: Vector2) -> void:
	var newChunkPos = Vector2(
		chunkPos.x * TILE_SIZE,
		chunkPos.y * TILE_SIZE
	)
	playerChunk.position = newChunkPos

func resize_viewport():
	MapView.size = _get_inner_size()
	Camera._update_camera_pos(Camera.position)

func _get_inner_size() -> Vector2:
	return Vector2(
		container.size.x 
			- container.get_theme_constant("margin_left")
			- container.get_theme_constant("margin_right"),
		container.size.y 
			- container.get_theme_constant("margin_top")
			- container.get_theme_constant("margin_bottom"))
