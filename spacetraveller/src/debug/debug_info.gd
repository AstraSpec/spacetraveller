extends Label

func _ready() -> void:
	InputManager.debug_toggled.connect(_debug_toggled)

func _debug_toggled() -> void:
	visible = !visible

func _process(_delta: float) -> void:
	if visible:
		text = str(Engine.get_frames_per_second())
