extends Window

@export var stripContainer :ScrollContainer
@export var inventory :Inventory

var selected_index : int = 0

func _ready() -> void:
	visible = false
	InputManager.inventory_toggled.connect(_on_inventory_toggled)
	InputManager.inventory_item_selected.connect(_on_inventory_item_selected)
	InputManager.inventory_directional_input.connect(_on_directional_input)
	InputManager.inventory_drop_requested.connect(_on_inventory_drop_requested)

func _on_inventory_toggled() -> void:
	visible = !visible
	if visible:
		selected_index = 0
		_update_inventory()
		_connect_button_signals()
		_update_selection_visuals()

func _connect_button_signals():
	for btn in stripContainer.buttons:
		if not btn.hovered.is_connected(_on_button_hovered):
			btn.hovered.connect(_on_button_hovered)

func _on_button_hovered(index: int):
	selected_index = index
	_update_selection_visuals()

func _on_directional_input(direction: Vector2):
	var count = stripContainer.get_button_count()
	if count == 0: return
	
	var columns = stripContainer.columns
	var items_per_column = int(ceil(float(count) / columns))
	
	var old_index = selected_index
	
	if direction.y != 0:
		selected_index = (selected_index + int(direction.y) + count) % count
	
	if direction.x != 0:
		selected_index = (selected_index + int(direction.x) * items_per_column + count) % count
		
	if old_index != selected_index:
		_update_selection_visuals()

func _update_selection_visuals():
	for i in range(stripContainer.get_button_count()):
		var btn = stripContainer.get_button(i)
		if btn:
			btn.set_selected(i == selected_index)

func _on_inventory_item_selected() -> void:
	_on_close_requested()

func _on_inventory_drop_requested(all: bool) -> void:
	var items = inventory.get_items_list()
	if items.is_empty() or selected_index < 0 or selected_index >= items.size():
		return
	
	var item = items[selected_index]
	var item_id = item["id"]
	var amount_to_remove = item["amount"] if all else 1
	
	if inventory.remove_item(item_id, amount_to_remove):
		InputManager.inventory_item_dropped.emit(item_id, amount_to_remove)
		_update_inventory()
		
		# Clamp selected index to new size
		var new_count = stripContainer.get_button_count()
		if new_count == 0:
			selected_index = 0
		else:
			selected_index = clamp(selected_index, 0, new_count - 1)
		
		_connect_button_signals()
		_update_selection_visuals()

func _on_close_requested() -> void:
	InputManager.return_context()

func _update_inventory():
	var items = inventory.get_items_list()
	var display_data = []
	
	for item in items:
		display_data.append({
			"left": item["id"],
			"right": "x" + str(item["amount"])
		})
	
	stripContainer.data = display_data
	stripContainer._update_grid_layout()
