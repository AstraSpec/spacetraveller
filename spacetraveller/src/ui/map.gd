extends Control

@onready var MapView :SubViewport = get_node("/root/Main/MapView")
@onready var Camera :ViewCamera = get_node("/root/Main/MapView/Camera")
@onready var MapRenderer :FastMapRenderer = get_node("/root/Main/MapView/MapRenderer")
@onready var playerChunk :TextureRect = get_node("/root/Main/MapView/PlayerChunk")
@onready var Player :PlayerController = get_node("/root/Main/Player")
@onready var World :GameWorld = get_node("/root/Main/GameWorld")
@onready var ZLevelLabel :Label = get_node("ZLevelLabel")

var REGION_SIZE = GameWorld.get_region_size()
var TILE_SIZE = FastMapRenderer.get_tile_size()

func _ready():
	mouse_filter = Control.MOUSE_FILTER_PASS
	get_window().size_changed.connect(resize_viewport)
	resized.connect(resize_viewport)
	call_deferred("resize_viewport")
	MapView.canvas_item_default_texture_filter = Viewport.DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_NEAREST
	
	visible = true
	Player.moved_chunk.connect(_on_player_moved_chunk)
	
	# Configure Camera
	Camera.centerNode = playerChunk
	Camera.viewport = MapView
	Camera.limits = Rect2(0, 0, REGION_SIZE * TILE_SIZE, REGION_SIZE * TILE_SIZE)
	_sync_player_chunk_position()
	Camera._view_centered()

func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_RIGHT and event.pressed:
			var chunk_pos := _screen_pos_to_chunk(event.position)
			if Player.has_method("cancel_navigation"):
				Player.cancel_navigation()
			if World.teleport_player_to_chunk(chunk_pos):
				accept_event()

func _screen_pos_to_chunk(screen_pos: Vector2) -> Vector2i:
	var viewport_size := Vector2(MapView.size)
	var offset := screen_pos - viewport_size * 0.5
	var map_pos := Camera.get_screen_center_position() + Vector2(
		offset.x / Camera.zoom.x,
		offset.y / Camera.zoom.y
	)
	return Vector2i(
		floori(map_pos.x / TILE_SIZE),
		floori(map_pos.y / TILE_SIZE)
	)

func _on_world_generated(regionChunks: Dictionary) -> void:
	if regionChunks.is_empty():
		return

	MapRenderer.clear()
	
	# Determine bounds from any key in the dictionary
	var firstKey = regionChunks.keys()[0]
	var coords = GameWorld.unpack_coords(firstKey)
	var coordX = coords.x
	var coordY = coords.y
	
	var startX = floor(float(coordX) / REGION_SIZE) * REGION_SIZE
	var startY = floor(float(coordY) / REGION_SIZE) * REGION_SIZE
	
	for y in range(startY, startY + REGION_SIZE):
		for x in range(startX, startX + REGION_SIZE):
			var key = GameWorld.pack_coords(x, y)
			var chunkID = regionChunks.get(key, "")
			
			if chunkID == "":
				continue
				
			var atlas :Vector2i = ChunkDb.get_atlas_coords(chunkID)
			MapRenderer.set_cell(Vector2i(x, y), atlas)
	
	MapView.size = get_size()
	_sync_player_chunk_position()

func center_view() -> void:
	_sync_player_chunk_position()
	_update_z_label()
	Camera._view_centered()

func _on_player_moved_chunk(chunkPos: Vector2) -> void:
	var newChunkPos = Vector2(
		chunkPos.x * TILE_SIZE,
		chunkPos.y * TILE_SIZE
	)
	playerChunk.position = newChunkPos

func _sync_player_chunk_position() -> void:
	_on_player_moved_chunk(Player.chunkPos())

func _update_z_label() -> void:
	if ZLevelLabel and World:
		ZLevelLabel.text = "Z: " + str(World.get_player_z())

func resize_viewport():
	MapView.size = _get_inner_size()
	Camera._update_camera_pos(Camera.position)

func _get_inner_size() -> Vector2:
	return Vector2(
		size.x,
		size.y)
