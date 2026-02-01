@tool
extends HBoxContainer

@export var Label1 :Label
@export var Label2 :Label

@export_group("Label 1 Style")
@export var label1_color: Color = Color.WHITE:
	set(value):
		label1_color = value
		if Label1: Label1.add_theme_color_override("font_color", value)

@export var label1_font_size: int = 16:
	set(value):
		label1_font_size = value
		if Label1: Label1.add_theme_font_size_override("font_size", value)

@export var label1_text: String = "This is a label":
	set(value):
		label1_text = value
		if Label1: Label1.text = value

@export_group("Label 2 Style")
@export var label2_color: Color = Color.WHITE:
	set(value):
		label2_color = value
		if Label2: Label2.add_theme_color_override("font_color", value)

@export var label2_font_size: int = 16:
	set(value):
		label2_font_size = value
		if Label2: Label2.add_theme_font_size_override("font_size", value)

@export var label2_text: String = "Wow!":
	set(value):
		label2_text = value
		if Label2: Label2.text = value

func _ready() -> void:
	if Label1:
		Label1.add_theme_color_override("font_color", label1_color)
		Label1.add_theme_font_size_override("font_size", label1_font_size)
	if Label2:
		Label2.add_theme_color_override("font_color", label2_color)
		Label2.add_theme_font_size_override("font_size", label2_font_size)
