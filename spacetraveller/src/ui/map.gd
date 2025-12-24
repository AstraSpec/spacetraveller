extends Control

@onready var MapView :SubViewport = get_node("/root/Main/MapView")
@onready var Camera :Camera2D = get_node("/root/Main/MapView/Camera")
@onready var Tilemap :TileMapLayer = get_node("/root/Main/MapView/TileMap")
@onready var container :MarginContainer = $MarginContainer

const SOURCE :int = 2
const DRAG_SPEED :float = 1.75

func _ready():
	get_window().size_changed.connect(resize_viewport)
	resized.connect(resize_viewport)
	call_deferred("resize_viewport")

func get_inner_size() -> Vector2:
	return Vector2(
		container.size.x 
			- container.get_theme_constant("margin_left")
			- container.get_theme_constant("margin_right"),
		container.size.y 
			- container.get_theme_constant("margin_top")
			- container.get_theme_constant("margin_bottom"))

func resize_viewport():
	var inner_size = get_inner_size()
	
	MapView.size = inner_size
	MapView.render_target_update_mode = SubViewport.UPDATE_ONCE

func _on_world_generated(regionChunks: Dictionary) -> void:
	if regionChunks.is_empty():
		return
		
	var region_size = 256
	
	# Determine bounds from any key in the dictionary
	var first_key = regionChunks.keys()[0]
	var x_coord = int(first_key) >> 32
	var y_coord = int(first_key) & 0xFFFFFFFF
	if y_coord & 0x80000000:
		y_coord |= -0x100000000
	
	var start_x = floor(float(x_coord) / region_size) * region_size
	var start_y = floor(float(y_coord) / region_size) * region_size
	
	for y in range(start_y, start_y + region_size):
		for x in range(start_x, start_x + region_size):
			var key = (int(x) << 32) | (int(y) & 0xFFFFFFFF)
			var chunk_id = regionChunks.get(key, "")
			
			if chunk_id == "":
				continue
				
			var atlas_coords = ChunkDb.get_atlas_coords(chunk_id)
			Tilemap.set_cell(Vector2i(x, y), SOURCE, atlas_coords)
	
	MapView.size = container.get_size()

func _unhandled_input(event :InputEvent) -> void:
	if !visible: return
	
	if event is InputEventMouseMotion and Input.is_mouse_button_pressed(MOUSE_BUTTON_MIDDLE):
		var pos :Vector2 = Camera.position
		pos -= event.relative * DRAG_SPEED
		
		var innerSize :Vector2 = get_inner_size()
		
		pos.x = clamp(pos.x, 0, 256*12 - innerSize.x)
		pos.y = clamp(pos.y, 0, 256*12 - innerSize.y)
		
		pos.x = floor(pos.x)
		pos.y = floor(pos.y)
		
		Camera.position = pos
		
		MapView.render_target_update_mode = SubViewport.UPDATE_ONCE
