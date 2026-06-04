extends Button

signal hovered

func _ready() -> void:
	mouse_entered.connect(func(): hovered.emit())

func setup(_index: int, left: String, _right: String) -> void:
	text = left

func set_selected(is_selected: bool) -> void:
	modulate = Color(1.4, 1.4, 1.4) if is_selected else Color(1, 1, 1)

func set_font_color(color: Color) -> void:
	# Tint the button's own text. Hover/pressed variants are nudged so the
	# selection visual stays readable on top of the tint.
	add_theme_color_override("font_color", color)
	add_theme_color_override("font_hover_color", color * 1.2)
	add_theme_color_override("font_pressed_color", color * 0.8)
