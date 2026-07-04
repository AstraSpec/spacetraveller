extends Button

signal hovered

func _ready() -> void:
	focus_mode = Control.FOCUS_NONE
	mouse_entered.connect(func(): hovered.emit())

func setup(_index: int, left: String, right: String) -> void:
	text = left if right.is_empty() else "%s    %s" % [left, right]

func set_selected(is_selected: bool) -> void:
	modulate = Color(1.22, 1.22, 1.18) if is_selected else Color(1, 1, 1)

func set_font_color(color: Color) -> void:
	add_theme_color_override("font_color", color)
	add_theme_color_override("font_hover_color", color * 1.15)
	add_theme_color_override("font_pressed_color", color * 0.85)
