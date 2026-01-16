extends MarginContainer

@onready var StyleBoxLight = preload("res://src/ui/strip_container/style_box_light.tres")
@onready var StyleBoxDark = preload("res://src/ui/strip_container/style_box_dark.tres")

@onready var button :Button = $Button
@onready var left_label :Label = $HBoxContainer/Label1
@onready var right_label :Label = $HBoxContainer/Label2

func setup(index: int, left_text: String, right_text: String):
	var is_even = index % 2 == 0
	
	left_label.text = left_text
	right_label.text = right_text
	
	if is_even:
		button.add_theme_stylebox_override("normal", StyleBoxLight)
	else:
		button.add_theme_stylebox_override("normal", StyleBoxDark)
