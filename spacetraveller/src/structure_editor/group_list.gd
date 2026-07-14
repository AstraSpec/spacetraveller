extends VBoxContainer

signal selection_changed(id: String, is_primary: bool)

const ROW_HEIGHT := 26
var _db: Object
var _ids: Array = []
var _filter_query := ""

func start(db: Object, filter_query: String = "") -> void:
	_db = db
	_filter_query = filter_query
	_ids = db.get_ids()
	_ids.sort()
	_rebuild()

func set_filter(filter_query: String) -> void:
	_filter_query = filter_query
	_rebuild()

func _rebuild() -> void:
	for child in get_children():
		child.queue_free()

	for raw_id in _ids:
		var id := str(raw_id)
		if _matches_filter(id):
			_add_entry_button(id)

func _matches_filter(id: String) -> bool:
	var query := _normalize_search(_filter_query)
	return query.is_empty() or _normalize_search(id).contains(query)

func _normalize_search(value: String) -> String:
	return value.to_lower().replace("_", " ").replace("-", " ").strip_edges()

func _add_entry_button(id: String) -> void:
	var button := Button.new()
	button.text = id
	button.tooltip_text = id
	button.custom_minimum_size = Vector2(0, ROW_HEIGHT)
	button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	button.alignment = HORIZONTAL_ALIGNMENT_LEFT
	button.clip_text = true
	add_child(button)

	button.gui_input.connect(func(event: InputEvent) -> void:
		if event is InputEventMouseButton and event.pressed:
			if event.button_index == MOUSE_BUTTON_LEFT:
				selection_changed.emit(id, true)
			elif event.button_index == MOUSE_BUTTON_RIGHT:
				selection_changed.emit(id, false)
		)
