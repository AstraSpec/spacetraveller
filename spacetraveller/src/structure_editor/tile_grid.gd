extends GridContainer

signal tile_selected(id: String, is_primary: bool)

var spacing = 0

func start(s :int) -> void:
	spacing = s
	
	var ids = TileDb.get_ids()
	for id in ids:
		add_tile_button(id)

func add_tile_button(id: String) -> void:
	var button = preload("res://src/structure_editor/tile_button.tscn").instantiate()
	
	var atlas = TileDb.get_atlas_coords(id)
	var new_atlas = button.texture_normal
	
	# Structure tile and spacing
	var tile_size = FastTileMap.get_tile_size()
	new_atlas.region = Rect2(
		spacing + atlas.x * (tile_size + spacing), 
		spacing + atlas.y * (tile_size + spacing), 
		tile_size, 
		tile_size
	)
	
	button.texture_normal = new_atlas
	button.tooltip_text = id
	
	add_child(button)
	button.gui_input.connect(func(event):
		if event is InputEventMouseButton and event.pressed:
			if event.button_index == MOUSE_BUTTON_LEFT:
				tile_selected.emit(id, true)
			elif event.button_index == MOUSE_BUTTON_RIGHT:
				tile_selected.emit(id, false)
	)
