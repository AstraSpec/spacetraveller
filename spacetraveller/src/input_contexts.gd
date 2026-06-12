extends RefCounted
class_name InputContext

var manager: Node

func _init(p_manager: Node) -> void:
	manager = p_manager

func handle_input(_event: InputEvent) -> bool:
	return false

func process(_delta: float) -> void:
	pass

class DirectionalProcessor:
	var move_timer : float = 0.3
	var hold_move_timer : float = 0.05
	var time_since_move : float = 0.0
	var key_held : bool = false
	var last_input_direction : Vector2 = Vector2.ZERO

	func get_step_vector(delta: float, is_shift_pressed: bool) -> Vector2:
		time_since_move += delta
		
		# Get digital vector (rounded to handle potential analog floating precision)
		var current_raw_dir = Input.get_vector("left", "right", "up", "down").round()
		
		var center_held = Input.is_action_pressed("center")
		var center_just_pressed = Input.is_action_just_pressed("center")
		
		# Detect if any relevant action was JUST pressed this frame
		var just_pressed_dir = Vector2.ZERO
		if Input.is_action_just_pressed("up"): just_pressed_dir.y -= 1
		if Input.is_action_just_pressed("down"): just_pressed_dir.y += 1
		if Input.is_action_just_pressed("left"): just_pressed_dir.x -= 1
		if Input.is_action_just_pressed("right"): just_pressed_dir.x += 1
		
		var new_pressed = just_pressed_dir != Vector2.ZERO or center_just_pressed
		var has_input = current_raw_dir != Vector2.ZERO or center_held
		
		var effective_dir = current_raw_dir
		
		if not has_input:
			last_input_direction = Vector2.ZERO
			key_held = false
			time_since_move = move_timer if is_shift_pressed else 0.0
			return Vector2.INF
		
		if current_raw_dir == Vector2.ZERO and center_held:
			effective_dir = Vector2.ZERO
			if center_just_pressed:
				just_pressed_dir = Vector2.ZERO
				new_pressed = true
		
		if effective_dir != last_input_direction and !is_shift_pressed and new_pressed:
			time_since_move = 0.0
			last_input_direction = effective_dir
			key_held = false
			return effective_dir
		
		elif effective_dir == Vector2.ZERO and center_just_pressed and !is_shift_pressed:
			time_since_move = 0.0
			key_held = false
			return Vector2.ZERO
			
		else:
			var threshold = hold_move_timer if key_held else move_timer
			if time_since_move > threshold:
				time_since_move = 0.0
				key_held = true
				return effective_dir
		
		return Vector2.INF

class ViewProcessor:
	var manager: Node
	
	func _init(p_manager: Node):
		manager = p_manager
		
	func handle_input(event: InputEvent) -> bool:
		if event is InputEventMouseButton:
			if event.button_index == MOUSE_BUTTON_WHEEL_UP and event.pressed:
				manager.view_zoomed.emit(1)
				return true
			elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN and event.pressed:
				manager.view_zoomed.emit(-1)
				return true
		
		if event is InputEventMouseMotion and Input.is_mouse_button_pressed(MOUSE_BUTTON_MIDDLE):
			manager.view_panned.emit(event.relative)
			return true
			
		if event.is_action_pressed("center_player"):
			manager.view_centered.emit()
			return true
		
		return false

class ExplorationContext extends InputContext:
	var move_processor = DirectionalProcessor.new()

	func handle_input(event: InputEvent) -> bool:
		if event.is_action_pressed("action_smash"):
			manager.action_smash_requested.emit()
			return true
		elif event.is_action_pressed("action_close"):
			manager.action_close_requested.emit()
			return true
		elif event.is_action_pressed("action_pickup"):
			manager.action_pickup_requested.emit()
			return true
		elif event.is_action_pressed("action_examine"):
			manager.action_examine_requested.emit()
			return true
		elif event.is_action_pressed("ascend_level"):
			manager.action_ascend_requested.emit()
			return true
		elif event.is_action_pressed("descent_level"):
			manager.action_descend_requested.emit()
			return true
		elif event is InputEventMouseButton:
			if event.button_index == MOUSE_BUTTON_RIGHT and event.pressed:
				manager.exploration_right_click.emit(event.global_position)
				return true
		return false

	func process(delta: float) -> void:
		var step = move_processor.get_step_vector(delta, manager.is_shift_pressed)
		if step != Vector2.INF:
			manager.directional_input.emit(step)

class MapContext extends InputContext:
	var move_processor = DirectionalProcessor.new()
	var view_processor : ViewProcessor

	func _init(p_manager: Node):
		super._init(p_manager)
		view_processor = ViewProcessor.new(p_manager)

	func handle_input(event: InputEvent) -> bool:
		if event.is_action_pressed("ui_cancel"):
			manager.pop_mode()
			return true
			
		if view_processor.handle_input(event):
			return true
		return false

	func process(delta: float) -> void:
		var step = move_processor.get_step_vector(delta, manager.is_shift_pressed)
		if step != Vector2.INF and step != Vector2.ZERO:
			manager.view_panned.emit(-step * FastTileMap.get_tile_size())

class StructureContext extends InputContext:
	var move_processor = DirectionalProcessor.new()
	var view_processor : ViewProcessor

	func _init(p_manager: Node):
		super._init(p_manager)
		view_processor = ViewProcessor.new(p_manager)

	func handle_input(event: InputEvent) -> bool:
		if event.is_action_pressed("ui_cancel"):
			manager.pop_mode()
			return true
			
		if view_processor.handle_input(event):
			return true
		
		if event.is_action_pressed("structure_undo"): 
			manager.structure_key_input.emit("undo")
			return true
		elif event.is_action_pressed("structure_redo"): 
			manager.structure_key_input.emit("redo")
			return true
		elif event.is_action_pressed("ascend_level"):
			manager.structure_key_input.emit("ascend_level")
			return true
		elif event.is_action_pressed("descent_level"):
			manager.structure_key_input.emit("descent_level")
			return true
		elif event.is_action_pressed("structure_pencil"): 
			manager.structure_mode_changed.emit("pencil")
			return true
		elif event.is_action_pressed("structure_line"): 
			manager.structure_mode_changed.emit("line")
			return true
		elif event.is_action_pressed("structure_eyedropper"): 
			manager.structure_mode_changed.emit("eyedropper")
			return true
		elif event.is_action_pressed("structure_fill"): 
			manager.structure_mode_changed.emit("fill")
			return true
		elif event.is_action_pressed("structure_selection"): 
			manager.structure_mode_changed.emit("selection")
			return true
		elif event.is_action_pressed("structure_ellipsis"): 
			manager.structure_mode_changed.emit("ellipsis")
			return true
		elif event.is_action_pressed("structure_rectangle"): 
			manager.structure_mode_changed.emit("rectangle")
			return true
		elif event.is_action_pressed("delete"): 
			manager.structure_key_input.emit("delete")
			return true
		elif event.is_action_pressed("copy"): 
			manager.structure_key_input.emit("copy")
			return true
		elif event.is_action_pressed("cut"): 
			manager.structure_key_input.emit("cut")
			return true
		elif event.is_action_pressed("paste"): 
			manager.structure_key_input.emit("paste")
			return true
		
		if event is InputEventMouseButton:
			var button = ""
			if event.button_index == MOUSE_BUTTON_LEFT: button = "left"
			elif event.button_index == MOUSE_BUTTON_RIGHT: button = "right"
			
			if button != "":
				if event.pressed:
					manager.structure_mouse_input.emit(button, manager.MouseAction.PRESS)
					return true
				else:
					manager.structure_mouse_input.emit(button, manager.MouseAction.RELEASE)
					return true
		
		elif event is InputEventMouseMotion:
			if event.button_mask & MOUSE_BUTTON_MASK_LEFT:
				manager.structure_mouse_input.emit("left", manager.MouseAction.DRAG)
				return true
			if event.button_mask & MOUSE_BUTTON_MASK_RIGHT:
				manager.structure_mouse_input.emit("right", manager.MouseAction.DRAG)
				return true
		
		return false

	func process(delta: float) -> void:
		var step = move_processor.get_step_vector(delta, manager.is_shift_pressed)
		if step != Vector2.INF and step != Vector2.ZERO:
			manager.view_panned.emit(-step * FastTileMap.get_tile_size())

class MenuContext extends InputContext:
	var move_processor = DirectionalProcessor.new()

	func handle_input(event: InputEvent) -> bool:
		if event.is_action_pressed("ui_accept"):
			manager.ui_accept.emit()
			return true
		elif event.is_action_pressed("ui_cancel"):
			manager.ui_cancel.emit()
			return true
		elif event.is_action_pressed("ui_focus_next"):
			if manager.is_shift_pressed:
				manager.ui_prev_tab.emit()
			else:
				manager.ui_next_tab.emit()
			return true
		elif event.is_action_pressed("drop_item"):
			manager.ui_drop_requested.emit(manager.is_shift_pressed)
			return true
		elif event.is_action_pressed("wear_item"):
			manager.ui_wear_requested.emit()
			return true
		elif event.is_action_pressed("wield_item"):
			manager.ui_wield_requested.emit()
			return true
		elif event.is_action_pressed("delete"):
			manager.ui_delete.emit()
			return true
		return false

	func process(delta: float) -> void:
		var step = move_processor.get_step_vector(delta, manager.is_shift_pressed)
		if step != Vector2.INF and step != Vector2.ZERO:
			manager.ui_directional_input.emit(step)
