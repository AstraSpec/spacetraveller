extends MarginContainer

signal hovered(index: int)

@onready var StyleBoxLight = preload("res://src/ui/strip_container/style_box_light.tres")
@onready var StyleBoxDark = preload("res://src/ui/strip_container/style_box_dark.tres")

@onready var button :Button = $Button
@onready var left_label :Label = $HBoxContainer/Label1
@onready var right_label :Label = $HBoxContainer/Label2

var item_index : int = -1

func _ready() -> void:
	button.mouse_entered.connect(_on_mouse_entered)

func setup(index: int, left_text: String, right_text: String):
	item_index = index
	var is_even = index % 2 == 0
	
	left_label.text = left_text
	right_label.text = right_text
	
	if is_even:
		button.add_theme_stylebox_override("normal", StyleBoxLight)
		button.add_theme_stylebox_override("hover", StyleBoxLight)
	else:
		button.add_theme_stylebox_override("normal", StyleBoxDark)
		button.add_theme_stylebox_override("hover", StyleBoxDark)

func set_selected(is_selected: bool):
	if is_selected:
		self.modulate = Color(1.5, 1.5, 1.5)
	else:
		self.modulate = Color(1, 1, 1)

func _on_mouse_entered():
	hovered.emit(item_index)
