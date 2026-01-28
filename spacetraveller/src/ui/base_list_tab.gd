class_name BaseListTab extends Control

@export var stripContainer: ScrollContainer
@export var titleLabel: Label
@export var descriptionLabel: Label
@export var detailsContainer: VBoxContainer

var selected_index: int = 0
var _items_cache: Array = []

func _get_display_data() -> Array:
	return []

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
	
	# Map data to the format StripContainer expects
	var strip_data = []
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
