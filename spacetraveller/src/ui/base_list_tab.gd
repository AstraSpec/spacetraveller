class_name BaseListTab extends Control

const TYPE_ORDER = ["weapon", "consumable", "material", "tool", "clothing", "misc"]
const BODY_PART_ORDER = ["head", "torso", "leg", "arm", "hand", "finger", "foot", "ear", "other"]

@export var stripContainer: ButtonListContainer
@export var titleLabel: Label
@export var descriptionLabel: Label
@export var detailsContainer: VBoxContainer

var Player: Node2D
var _items_cache: Array = []

func _ready() -> void:
	if stripContainer:
		stripContainer.item_selected.connect(_on_container_item_selected)
		stripContainer.item_activated.connect(_on_container_item_activated)

func _type_sort_key(type_str: String) -> int:
	var idx = TYPE_ORDER.find(type_str.to_lower())
	return idx if idx >= 0 else TYPE_ORDER.size()

func _body_part_sort_key(part_str: String) -> int:
	var idx = BODY_PART_ORDER.find(part_str.to_lower())
	return idx if idx >= 0 else BODY_PART_ORDER.size()

func _separator_sort_key(item: Dictionary) -> int:
	if item.has("separator_sort"):
		return int(item.get("separator_sort", 0))
	return _body_part_sort_key(item.get("separator_key", "other"))

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
		
		# Preserve original item data but add display fields
		var display_item = item.duplicate()
		display_item["left"] = item.get("display_name", "???")
		display_item["right"] = item.get("quantity_text", "")
		strip_data.append(display_item)
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

func _get_display_data() -> Array:
	return []

func _on_item_activated() -> void:
	pass

func handle_directional_input(direction: Vector2) -> void:
	if stripContainer:
		stripContainer.handle_directional_input(direction)

func refresh_view() -> void:
	_items_cache = _get_display_data()
	
	var data_to_send: Array = []
	if _items_have_key("separator_key"):
		_items_cache.sort_custom(func(a, b): return _separator_sort_key(a) < _separator_sort_key(b))
		data_to_send = _build_strip_data_with_separators(_items_cache, "separator_key", _body_part_sort_key)
	elif _items_have_type():
		_items_cache.sort_custom(func(a, b): return _type_sort_key(a.get("type", "misc")) < _type_sort_key(b.get("type", "misc")))
		data_to_send = _build_strip_data_with_separators(_items_cache, "type", _type_sort_key)
	else:
		data_to_send = _items_cache
	
	if stripContainer:
		stripContainer.set_data(data_to_send)
		_on_container_item_selected(selected_index, stripContainer._get_data_for_button_index(selected_index))
	
	_on_refresh()

func _on_refresh() -> void:
	pass

func _on_container_item_selected(_index: int, data: Variant) -> void:
	if not data or not data is Dictionary: return
	
	# Generic Label Updates
	if titleLabel: titleLabel.text = str(data.get("display_name", data.get("left", "")))
	if descriptionLabel: descriptionLabel.text = str(data.get("description", ""))
	
	# Delegate specific UI construction to child class
	_update_details_ui(data)

func _on_container_item_activated(_index: int, _data: Variant) -> void:
	_on_item_activated()

# Getter for selected index
var selected_index: int:
	get: return stripContainer.selected_index if stripContainer else 0
	set(val): if stripContainer: stripContainer.selected_index = val
