class_name BaseListTab extends Control

const TYPE_ORDER = ["weapon", "consumable", "material", "tool", "clothing", "misc"]
const BODY_PART_ORDER = ["head", "torso", "leg", "arm", "hand", "finger", "foot", "ear", "other"]

@export var stripContainer: ScrollContainer
@export var titleLabel: Label
@export var descriptionLabel: Label
@export var detailsContainer: VBoxContainer

var selected_index: int = 0
var _items_cache: Array = []

func _get_display_data() -> Array:
	return []

func _type_sort_key(type_str: String) -> int:
	var idx = TYPE_ORDER.find(type_str.to_lower())
	return idx if idx >= 0 else TYPE_ORDER.size()

func _body_part_sort_key(part_str: String) -> int:
	var idx = BODY_PART_ORDER.find(part_str.to_lower())
	return idx if idx >= 0 else BODY_PART_ORDER.size()

func _build_strip_data_with_separators(items: Array, group_key: String, _sort_key_func: Callable) -> Array:
	if items.is_empty(): return []
	var strip_data: Array = []
	var last_key: String = ""
	for item in items:
		var key = str(item.get(group_key, "other")).to_lower()
		if key != last_key:
			last_key = key
			var label = key.capitalize() if key.length() > 0 else "Other"
			strip_data.append({ "separator": label })
		strip_data.append({
			"left": item.get("display_name", "???"),
			"right": item.get("quantity_text", "")
		})
	return strip_data

func _items_have_key(key_name: String) -> bool:
	for item in _items_cache:
		if not item is Dictionary or not item.has(key_name):
			return false
	return _items_cache.size() > 0

func _items_have_type() -> bool:
	return _items_have_key("type")

func _update_details_ui(_item_data: Dictionary) -> void:
	pass

func _on_item_activated() -> void:
	pass

func handle_directional_input(direction: Vector2) -> void:
	var count = stripContainer.get_button_count()
	if count == 0: return
	
	var columns = stripContainer.columns
	# Calculate items per column based on total count
	var items_per_column = int(ceil(float(count) / columns))
	
	var old_index = selected_index
	
	if direction.y != 0:
		selected_index = (selected_index + int(direction.y) + count) % count
	
	if direction.x != 0:
		selected_index = (selected_index + int(direction.x) * items_per_column + count) % count
		
	if old_index != selected_index:
		_update_selection_visuals()

func refresh_view() -> void:
	_items_cache = _get_display_data()
	
	var strip_data: Array
	if _items_have_key("separator_key"):
		_items_cache.sort_custom(func(a, b): return _body_part_sort_key(a.get("separator_key", "other")) < _body_part_sort_key(b.get("separator_key", "other")))
		strip_data = _build_strip_data_with_separators(_items_cache, "separator_key", _body_part_sort_key)
	elif _items_have_type():
		_items_cache.sort_custom(func(a, b): return _type_sort_key(a.get("type", "misc")) < _type_sort_key(b.get("type", "misc")))
		strip_data = _build_strip_data_with_separators(_items_cache, "type", _type_sort_key)
	else:
		strip_data = []
		for item in _items_cache:
			strip_data.append({
				"left": item.get("display_name", "???"),
				"right": item.get("quantity_text", "")
			})
	
	stripContainer.data = strip_data
	stripContainer._update_grid_layout()
	
	# Connect signals for mouse interaction
	_connect_strip_signals()
	
	# Reset selection if out of bounds
	var new_count = stripContainer.get_button_count()
	if new_count == 0:
		selected_index = 0
	else:
		selected_index = clamp(selected_index, 0, new_count - 1)
	
	_update_selection_visuals()

func _update_selection_visuals() -> void:
	var count = stripContainer.get_button_count()
	for i in range(count):
		var btn = stripContainer.get_button(i)
		if btn:
			var is_selected = (i == selected_index)
			btn.set_selected(is_selected)
			
			if is_selected and i < _items_cache.size():
				var data = _items_cache[i]
				
				# Generic Label Updates
				if titleLabel: titleLabel.text = data.get("display_name", "")
				if descriptionLabel: descriptionLabel.text = data.get("description", "")
				
				# Delegate specific UI construction to child class
				_update_details_ui(data)

# Helper to connect mouse signals if needed
func _connect_strip_signals() -> void:
	for i in range(stripContainer.get_button_count()):
		var btn = stripContainer.get_button(i)
		if btn and not btn.hovered.is_connected(_on_button_hovered):
			btn.hovered.connect(_on_button_hovered)

func _on_button_hovered(index: int) -> void:
	selected_index = index
	_update_selection_visuals()
