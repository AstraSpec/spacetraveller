extends Button

signal hovered

func _ready() -> void:
	mouse_entered.connect(func(): hovered.emit())

func setup(_index: int, left: String, _right: String) -> void:
	text = left

func set_selected(is_selected: bool) -> void:
	modulate = Color(1.4, 1.4, 1.4) if is_selected else Color(1, 1, 1)
