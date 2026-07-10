extends MarginContainer

signal hovered
signal pressed

@onready var StyleBoxLight = preload("res://src/ui/strip_container/style_box_light.tres")
@onready var StyleBoxDark = preload("res://src/ui/strip_container/style_box_dark.tres")

@onready var button :Button = $Button
@onready var left_label :RichTextLabel = $HBoxContainer/Label1
@onready var right_label :RichTextLabel = $HBoxContainer/Label2

var item_index : int = -1
var left_text: String = ""
var right_text: String = ""

func _ready() -> void:
	button.mouse_entered.connect(_on_mouse_entered)
	button.pressed.connect(_on_button_pressed)

func setup(index: int, p_left: String, p_right: String):
	item_index = index
	var is_even = index % 2 == 0
	left_text = p_left
	right_text = p_right
	
	_update_display()
	
	if is_even:
		button.add_theme_stylebox_override("normal", StyleBoxLight)
		button.add_theme_stylebox_override("hover", StyleBoxLight)
	else:
		button.add_theme_stylebox_override("normal", StyleBoxDark)
		button.add_theme_stylebox_override("hover", StyleBoxDark)

func _update_display(left: bool = false, right: bool = false, tag_open: String = "", tag_close: String = ""):
	if left:
		left_label.text = tag_open + left_text + tag_close
	else:
		left_label.text = left_text
	
	if right:
		right_label.text = tag_open + right_text + tag_close
	else:
		right_label.text = right_text

func set_underline(left: bool, right: bool):
	_update_display(left, right, "[u]", "[/u]")

func set_selected(is_selected: bool):
	if button.disabled:
		self.modulate = Color(0.55, 0.55, 0.55)
	elif is_selected:
		self.modulate = Color(1.5, 1.5, 1.5)
	else:
		self.modulate = Color(1, 1, 1)

func set_disabled(is_disabled: bool) -> void:
	button.disabled = is_disabled
	self.modulate = Color(0.55, 0.55, 0.55) if is_disabled else Color(1, 1, 1)

func _on_mouse_entered():
	hovered.emit()

func _on_button_pressed():
	pressed.emit()
