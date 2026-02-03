extends ScrollContainer

@onready var stripButton = preload("res://src/ui/strip_container/strip_button.tscn")
@onready var menuSeparationScene = preload("res://src/ui/menu_seperation.tscn")
@onready var Hbox : HBoxContainer = $HBox

@export var columns :int = 2
@export var button_left_margin :int = 0
var data :Array = []
var buttons : Array = []

func _ready() -> void:
	_update_grid_layout()

func _update_grid_layout():
	if not is_inside_tree(): return
	
	for child in Hbox.get_children():
		child.queue_free()
	
	buttons.clear()
	
	var total_items = data.size()
	if total_items == 0: return
	
	var items_per_column = ceil(float(total_items) / columns)

	for c in range(columns):
		var column_vbox = VBoxContainer.new()
		column_vbox.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		column_vbox.add_theme_constant_override("separation", 0) # Keeps strips tight
		Hbox.add_child(column_vbox)
		
		for i in range(items_per_column):
			var actual_index = (c * items_per_column) + i
			
			if actual_index < total_items:
				var item = data[actual_index]
				if item is Dictionary and item.has("separator"):
					var sep = menuSeparationScene.instantiate()
					column_vbox.add_child(sep)
					if sep.SeperationLabel:
						sep.SeperationLabel.text = str(item.get("separator", ""))
				else:
					var strip = stripButton.instantiate()
					if button_left_margin > 0:
						var wrapper = MarginContainer.new()
						wrapper.add_theme_constant_override("margin_left", button_left_margin)
						wrapper.add_child(strip)
						column_vbox.add_child(wrapper)
					else:
						column_vbox.add_child(strip)
					var button_index = buttons.size()
					buttons.append(strip)
					
					var left_text = ""
					var right_text = ""
					
					if item is Dictionary:
						left_text = str(item.get("left", ""))
						right_text = str(item.get("right", ""))
					else:
						left_text = str(item)
					
					if strip.has_method("setup"):
						strip.setup(button_index, left_text, right_text)

func get_button(index: int) -> Control:
	if index >= 0 and index < buttons.size():
		return buttons[index]
	return null

func get_button_count() -> int:
	return buttons.size()
