extends ScrollContainer

@onready var stripButton = preload("res://src/ui/strip_container/strip_button.tscn")
@onready var Hbox : HBoxContainer = $HBox

var data :Array = []
var columns :int = 1

func _ready() -> void:
	_update_grid_layout()

func _update_grid_layout():
	if not is_inside_tree(): return
	
	for child in Hbox.get_children():
		child.queue_free()
	
	var total_items = data.size()
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
				var strip = stripButton.instantiate()
				column_vbox.add_child(strip)
				
				var left_text = ""
				var right_text = ""
				
				if item is Dictionary:
					left_text = str(item.get("left", ""))
					right_text = str(item.get("right", ""))
				else:
					left_text = str(item)
				
				if strip.has_method("setup"):
					strip.setup(actual_index, left_text, right_text)
