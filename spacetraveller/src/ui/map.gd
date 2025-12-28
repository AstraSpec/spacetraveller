extends Control

@onready var MapView :SubViewport = get_node("/root/Main/MapView")
@onready var Camera :Camera2D = get_node("/root/Main/MapView/Camera")
@onready var Tilemap :TileMapLayer = get_node("/root/Main/MapView/TileMap")
@onready var playerChunk :TextureRect = get_node("/root/Main/MapView/PlayerChunk")
@onready var container :MarginContainer = $MarginContainer

const SOURCE :int = 2
const DRAG_SPEED :float = 1.75
const ZOOM_LVL :Array = [0.75, 1.0, 1.5, 2.0]

var REGION_SIZE = WorldGeneration.get_region_size()
var TILE_SIZE = FastTileMap.get_tile_size()

var zoom :int = 1

func _ready():
	get_window().size_changed.connect(resize_viewport)
	resized.connect(resize_viewport)
	call_deferred("resize_viewport")
	_update_camera_pos(Vector2.ZERO)

func change_visibility() -> void:
	visible = !visible
	MapView.render_target_update_mode = SubViewport.UPDATE_ONCE
	_update_camera_pos(playerChunk.position)

func resize_viewport():
	MapView.size = _get_inner_size()
	MapView.render_target_update_mode = SubViewport.UPDATE_ONCE

func _get_inner_size() -> Vector2:
	return Vector2(
		container.size.x 
			- container.get_theme_constant("margin_left")
			- container.get_theme_constant("margin_right"),
		container.size.y 
			- container.get_theme_constant("margin_top")
			- container.get_theme_constant("margin_bottom"))

func _on_world_generated(regionChunks: Dictionary) -> void:
	if regionChunks.is_empty():
		return
	
	# Determine bounds from any key in the dictionary
	var firstKey = regionChunks.keys()[0]
	var coordX = int(firstKey) >> 32
	var coordY = int(firstKey) & 0xFFFFFFFF
	if coordY & 0x80000000:
		coordY |= -0x100000000
	
	var startX = floor(float(coordX) / REGION_SIZE) * REGION_SIZE
	var startY = floor(float(coordY) / REGION_SIZE) * REGION_SIZE
	
	for y in range(startY, startY + REGION_SIZE):
		for x in range(startX, startX + REGION_SIZE):
			var key = (int(x) << 32) | (int(y) & 0xFFFFFFFF)
			var chunkID = regionChunks.get(key, "")
			
			if chunkID == "":
				continue
				
			var atlas :Vector2i = ChunkDb.get_atlas_coords(chunkID)
			Tilemap.set_cell(Vector2i(x, y), SOURCE, atlas)
	
	MapView.size = container.get_size()

func _unhandled_input(event: InputEvent) -> void:
	if !visible: return
	
	if event is InputEventMouseMotion and Input.is_mouse_button_pressed(MOUSE_BUTTON_MIDDLE):
		_update_camera_pos(Camera.position - event.relative * DRAG_SPEED / ZOOM_LVL[zoom])
	
	elif event is InputEventMouseButton and event.pressed:
		var oldZoom = zoom
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			zoom = clamp(zoom + 1, 0, ZOOM_LVL.size() - 1)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			zoom = clamp(zoom - 1, 0, ZOOM_LVL.size() - 1)
		
		if oldZoom != zoom:
			Camera.zoom = Vector2(ZOOM_LVL[zoom], ZOOM_LVL[zoom])
			_update_camera_pos(Camera.position)

func _update_camera_pos(newPos: Vector2) -> void:
	var innerSize: Vector2 = _get_inner_size()
	var pixelSize = REGION_SIZE * TILE_SIZE
	
	var halfSize = (innerSize / ZOOM_LVL[zoom]) / 2.0
	
	var minPos = halfSize
	var maxPos = Vector2(pixelSize, pixelSize) - halfSize
	
	if maxPos.x < minPos.x:
		newPos.x = pixelSize / 2.0
	else:
		newPos.x = clamp(newPos.x, minPos.x, maxPos.x)
		
	if maxPos.y < minPos.y:
		newPos.y = pixelSize / 2.0
	else:
		newPos.y = clamp(newPos.y, minPos.y, maxPos.y)

	Camera.position = newPos.floor()
	MapView.render_target_update_mode = SubViewport.UPDATE_ONCE

func _on_player_moved_chunk(chunkPos: Vector2) -> void:
	var newChunkPos = Vector2(
		chunkPos.x * TILE_SIZE,
		chunkPos.y * TILE_SIZE
	)
	playerChunk.position = newChunkPos
