extends Node

signal map_toggled(is_open: bool)
signal view_panned(relative: Vector2)
signal view_zoomed(zoom: int)
signal view_centered

signal debug_toggled
signal directional_input(direction: Vector2)

signal structure_mode_changed(mode :String)
enum MouseAction { PRESS, RELEASE, DRAG }
signal structure_mouse_input(button: String, action: MouseAction)

enum InputMode { GAMEPLAY, MAP, STRUCTURE }
var current_mode = InputMode.GAMEPLAY
var is_shift_pressed = false

const MOVE_TIMER : float = 0.3
const HOLD_MOVE_TIMER : float = 0.05
var time_since_move : float = 0.0
var key_held : bool = false
var last_input_direction : Vector2 = Vector2.ZERO

# Edge detection states
var prev_up: bool = false
var prev_down: bool = false
var prev_left: bool = false
var prev_right: bool = false

func _unhandled_input(event: InputEvent):
	if event.is_action_pressed("debug_mode"):
		debug_toggled.emit()

	if event.is_action("shift"):
		is_shift_pressed = event.is_pressed()

	if current_mode == InputMode.MAP:
		_handle_map_input(event)
		
		if event.is_action_pressed("open_map"):
			_toggle_map()
	
	elif current_mode == InputMode.STRUCTURE:
		_handle_map_input(event)
		_handle_structure_input(event)

func _handle_map_input(event: InputEvent):
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP and event.pressed:
			view_zoomed.emit(1)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN and event.pressed:
			view_zoomed.emit(-1)
	
	if event is InputEventMouseMotion and Input.is_mouse_button_pressed(MOUSE_BUTTON_MIDDLE):
		view_panned.emit(event.relative)
		
	if event.is_action_pressed("center_player"):
		view_centered.emit()

func _handle_structure_input(event: InputEvent):
	if event.is_action_pressed("structure_pencil"): 
		structure_mode_changed.emit("pencil")
	elif event.is_action_pressed("structure_line"): 
		structure_mode_changed.emit("line")
	elif event.is_action_pressed("structure_eyedropper"): 
		structure_mode_changed.emit("eyedropper")
	elif event.is_action_pressed("structure_fill"): 
		structure_mode_changed.emit("fill")
	elif event.is_action_pressed("structure_selection"): 
		structure_mode_changed.emit("selection")

	if event is InputEventMouseButton:
		var button = ""
		if event.button_index == MOUSE_BUTTON_LEFT: button = "left"
		elif event.button_index == MOUSE_BUTTON_RIGHT: button = "right"
		
		if button != "":
			if event.pressed:
				structure_mouse_input.emit(button, MouseAction.PRESS)
			else:
				structure_mouse_input.emit(button, MouseAction.RELEASE)
	
	elif event is InputEventMouseMotion:
		if event.button_mask & MOUSE_BUTTON_MASK_LEFT:
			structure_mouse_input.emit("left", MouseAction.DRAG)
		if event.button_mask & MOUSE_BUTTON_MASK_RIGHT:
			structure_mouse_input.emit("right", MouseAction.DRAG)

func _process(delta):
	time_since_move += delta
	
	var up = Input.is_action_pressed("w")
	var down = Input.is_action_pressed("s")
	var left = Input.is_action_pressed("a")
	var right = Input.is_action_pressed("d")
	
	# Detect NEW presses this frame
	var new_pressed = (up and !prev_up) or (down and !prev_down) or (left and !prev_left) or (right and !prev_right)
	
	# Just pressed direction
	var new_direction = Vector2.ZERO
	if up and !prev_up: new_direction.y -= 1
	if down and !prev_down: new_direction.y += 1
	if left and !prev_left: new_direction.x -= 1
	if right and !prev_right: new_direction.x += 1
	
	# Update edge detection
	prev_up = up; prev_down = down; prev_left = left; prev_right = right
	
	var current_raw_dir = Vector2(Input.get_axis("a", "d"), Input.get_axis("w", "s"))
	
	if current_raw_dir == Vector2.ZERO:
		last_input_direction = Vector2.ZERO
		key_held = false
		time_since_move = MOVE_TIMER if is_shift_pressed else 0.0
	
	elif current_raw_dir != last_input_direction and !is_shift_pressed and new_pressed:
		# New direction
		_trigger_direction(new_direction)
		last_input_direction = current_raw_dir
		key_held = false
		
	else:
		# Repeated direction
		var threshold = HOLD_MOVE_TIMER if key_held else MOVE_TIMER
		if time_since_move > threshold:
			key_held = true
			_trigger_direction(current_raw_dir)

func _trigger_direction(dir: Vector2):
	time_since_move = 0.0
	if current_mode == InputMode.GAMEPLAY:
		directional_input.emit(dir)
	else:
		view_panned.emit(-dir * FastTileMap.get_tile_size())

func _toggle_map():
	current_mode = InputMode.MAP if current_mode == InputMode.GAMEPLAY else InputMode.GAMEPLAY
	map_toggled.emit(current_mode == InputMode.MAP)
