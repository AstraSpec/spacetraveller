extends BaseWindow

@onready var map_control = $Map

func _ready() -> void:
	super._ready()
	visible = false
	InputManager.map_toggled.connect(_on_map_toggled)

func _on_map_toggled() -> void:
	var is_map_mode = InputManager.current_mode == InputManager.InputMode.MAP
	visible = is_map_mode
	if visible:
		map_control.center_view()

func _input(event: InputEvent) -> void:
	if visible and InputManager.current_mode == InputManager.InputMode.MAP:
		if InputManager.active_context:
			if InputManager.active_context.handle_input(event):
				get_viewport().set_input_as_handled()
