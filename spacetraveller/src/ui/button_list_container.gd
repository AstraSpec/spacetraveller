extends ScrollContainer
class_name ButtonListContainer

signal item_selected(index: int, data: Variant)
signal item_activated(index: int, data: Variant)
signal item_clicked(index: int, data: Variant)

@export var button_scene: PackedScene = preload("res://src/ui/strip_container/strip_button.tscn")
@export var menu_separation_scene: PackedScene = preload("res://src/ui/menu_seperation.tscn")
@export var columns: int = 2
@export var button_left_margin: int = 0
@export var button_separation: int = 0
@export var activate_on_single_click: bool = true

var selected_index: int = 0
var buttons: Array = []
var _items_data: Array = []
var _items_per_column: int = 0

@onready var Hbox: HBoxContainer = $HBox

func _ready() -> void:
	if not Hbox:
		# Fallback for dynamic creation
		Hbox = HBoxContainer.new()
		Hbox.name = "HBox"
		Hbox.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		Hbox.size_flags_vertical = Control.SIZE_EXPAND_FILL
		add_child(Hbox)
	_update_grid_layout()

func set_data(new_data: Array) -> void:
	_items_data = new_data
	_update_grid_layout()
	
	# Reset selection if out of bounds
	if buttons.is_empty():
		selected_index = 0
	else:
		selected_index = clamp(selected_index, 0, buttons.size() - 1)
	
	_update_selection_visuals()

func handle_directional_input(direction: Vector2) -> void:
	var count = buttons.size()
	if count == 0: return
	
	var old_index = selected_index
	
	if direction.y != 0:
		selected_index = (selected_index + int(direction.y) + count) % count
	
	if direction.x != 0 and columns > 1:
		selected_index = (selected_index + int(direction.x) * _items_per_column + count) % count
		
	if old_index != selected_index:
		_update_selection_visuals()
		var data = _get_data_for_button_index(selected_index)
		item_selected.emit(selected_index, data)

func _update_grid_layout():
	if not is_inside_tree(): return
	
	for child in Hbox.get_children():
		child.queue_free()
	
	buttons.clear()
	
	var total_items = _items_data.size()
	if total_items == 0: return
	
	_items_per_column = int(ceil(float(total_items) / columns))

	for c in range(columns):
		var column_vbox = VBoxContainer.new()
		column_vbox.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		column_vbox.add_theme_constant_override("separation", button_separation)
		Hbox.add_child(column_vbox)
		
		for i in range(_items_per_column):
			var actual_index = (c * _items_per_column) + i
			
			if actual_index < total_items:
				var item = _items_data[actual_index]
				if item is Dictionary and item.has("separator"):
					var sep = menu_separation_scene.instantiate()
					column_vbox.add_child(sep)
					if sep.has_node("SeperationLabel"):
						sep.get_node("SeperationLabel").text = str(item.get("separator", ""))
					elif sep.has_method("set_text"):
						sep.set_text(str(item.get("separator", "")))
				else:
					var btn_instance = button_scene.instantiate()
					if button_left_margin > 0:
						var wrapper = MarginContainer.new()
						wrapper.add_theme_constant_override("margin_left", button_left_margin)
						wrapper.add_child(btn_instance)
						column_vbox.add_child(wrapper)
					else:
						column_vbox.add_child(btn_instance)
					
					var button_index = buttons.size()
					buttons.append(btn_instance)
					
					# Setup button content
					_setup_button(btn_instance, button_index, item)
					
					# Connect signals
					if btn_instance.has_signal("hovered"):
						btn_instance.hovered.connect(_on_button_hovered.bind(button_index))
					if btn_instance.has_signal("pressed"):
						btn_instance.pressed.connect(_on_button_pressed.bind(button_index))

func _setup_button(btn: Control, index: int, item: Variant):
	var left_text = ""
	var right_text = ""

	if item is Dictionary:
		left_text = str(item.get("left", item.get("display_name", "")))
		right_text = str(item.get("right", item.get("quantity_text", "")))
	elif item is String:
		left_text = item

	if btn.has_method("setup"):
		btn.setup(index, left_text, right_text)
	elif btn is Button:
		btn.text = left_text

	# Optional per-row font color, used to tint quest-offer buttons etc.
	if item is Dictionary and item.has("font_color") and btn.has_method("set_font_color"):
		var c = item["font_color"]
		if c is Color:
			btn.set_font_color(c)

	if item is Dictionary and item.has("disabled"):
		var disabled := bool(item.get("disabled", false))
		if btn.has_method("set_disabled"):
			btn.set_disabled(disabled)
		elif btn is Button:
			btn.disabled = disabled

func _update_selection_visuals() -> void:
	for i in range(buttons.size()):
		var btn = buttons[i]
		var is_selected = (i == selected_index)
		if btn.has_method("set_selected"):
			btn.set_selected(is_selected)
		elif btn is Button:
			if is_selected:
				_grab_focus_when_ready(btn)

func _grab_focus_when_ready(btn: Button) -> void:
	if not is_instance_valid(btn):
		return
	if btn.is_inside_tree():
		btn.grab_focus()
	else:
		call_deferred("_grab_focus_deferred", btn)

func _grab_focus_deferred(btn: Button) -> void:
	if is_instance_valid(btn) and btn.is_inside_tree():
		btn.grab_focus()

func deselect() -> void:
	selected_index = -1
	_update_selection_visuals()

func _on_button_hovered(index: int) -> void:
	selected_index = index
	_update_selection_visuals()
	item_selected.emit(selected_index, _get_data_for_button_index(selected_index))

func _on_button_pressed(index: int) -> void:
	if not is_inside_tree(): return
	selected_index = index
	_update_selection_visuals()
	var data = _get_data_for_button_index(selected_index)
	item_clicked.emit(selected_index, data)
	item_selected.emit(selected_index, data)
	if activate_on_single_click:
		item_activated.emit(selected_index, data)

func _get_data_for_button_index(index: int) -> Variant:
	var button_count = 0
	for i in range(_items_data.size()):
		var item = _items_data[i]
		if not (item is Dictionary and item.has("separator")):
			if button_count == index:
				return item
			button_count += 1
	return null

func get_button_count() -> int:
	return buttons.size()

func get_button(index: int) -> Control:
	if index >= 0 and index < buttons.size():
		return buttons[index]
	return null
