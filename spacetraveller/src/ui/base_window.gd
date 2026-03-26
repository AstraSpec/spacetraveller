extends Window
class_name BaseWindow

enum SizePreset { LARGE, TALL, SMALL }

@export var size_preset: SizePreset = SizePreset.LARGE

func _ready() -> void:
	unfocusable = true
	initial_position = Window.WINDOW_INITIAL_POSITION_CENTER_MAIN_WINDOW_SCREEN
	
	close_requested.connect(_on_close_requested)
	get_tree().root.size_changed.connect(_update_window_size)
	
	_update_window_size()

func _update_window_size() -> void:
	var screen_size = get_tree().root.size
	var target_size = Vector2i.ZERO
	
	match size_preset:
		SizePreset.LARGE:
			target_size = Vector2i(screen_size.x * 0.8, screen_size.y * 0.8)
		SizePreset.TALL:
			target_size = Vector2i(screen_size.x * 0.4, screen_size.y * 0.8)
		SizePreset.SMALL:
			target_size = Vector2i(screen_size.x * 0.4, screen_size.y * 0.4)
			
	size = target_size
	
	# Center the window after resizing
	var pos = (screen_size - target_size) / 2
	position = pos

func _on_close_requested() -> void:
	InputManager.set_mode(InputManager.InputMode.EXPLORATION)
