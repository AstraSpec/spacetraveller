extends GridContainer

signal tile_selected(id: String)

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
	button.pressed.connect(func(): tile_selected.emit(id))
