extends GridContainer

signal selection_changed(id: String, is_primary: bool)

var spacing = 0

func start(_spacing :int, db: Object) -> void:
	spacing = _spacing
	
	for child in get_children():
		child.queue_free()
		
	var ids = db.get_ids()
	for id in ids:
		add_entry_button(id, db)

func add_entry_button(id: String, db: Object) -> void:
	var button = preload("res://src/structure_editor/tile_button.tscn").instantiate()
	
	var atlas = db.get_atlas_coords(id)
	var new_atlas = button.texture_normal.duplicate()
	
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
				selection_changed.emit(id, true)
			elif event.button_index == MOUSE_BUTTON_RIGHT:
				selection_changed.emit(id, false)
	)
