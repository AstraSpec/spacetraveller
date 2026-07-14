extends FlowContainer

signal selection_changed(id: String, is_primary: bool)

var spacing = 1
var _db: Object
var _ids: Array = []
var _filter_query := ""

func start(_spacing :int, db: Object, filter_query: String = "") -> void:
	_db = db
	_filter_query = filter_query
	_ids = db.get_ids()
	_rebuild()

func set_filter(filter_query: String) -> void:
	_filter_query = filter_query
	_rebuild()

func _rebuild() -> void:
	for child in get_children():
		child.queue_free()

	if !_db:
		return

	for raw_id in _ids:
		var id := str(raw_id)
		if _matches_filter(id):
			add_entry_button(id, _db)

func _matches_filter(id: String) -> bool:
	var query := _normalize_search(_filter_query)
	if query.is_empty():
		return true
	if _normalize_search(id).contains(query):
		return true

	for method_name in ["get_tile_name", "get_item_name"]:
		if _db.has_method(method_name):
			var display_name := str(_db.call(method_name, id))
			if _normalize_search(display_name).contains(query):
				return true
	return false

func _normalize_search(value: String) -> String:
	return value.to_lower().replace("_", " ").replace("-", " ").strip_edges()

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
