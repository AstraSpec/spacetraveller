extends Label

func _ready() -> void:
	InputManager.debug_toggled.connect(_debug_toggled)
	visible = InputManager.debug_mode_enabled

func _debug_toggled(enabled: bool) -> void:
	visible = enabled

func _process(_delta: float) -> void:
	if visible:
		text = str(Engine.get_frames_per_second())
