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
		
		# Detect if any relevant action was JUST pressed this frame
		var just_pressed_dir = Vector2.ZERO
		if Input.is_action_just_pressed("up"): just_pressed_dir.y -= 1
		if Input.is_action_just_pressed("down"): just_pressed_dir.y += 1
		if Input.is_action_just_pressed("left"): just_pressed_dir.x -= 1
		if Input.is_action_just_pressed("right"): just_pressed_dir.x += 1
		
		var new_pressed = just_pressed_dir != Vector2.ZERO
		
		if current_raw_dir == Vector2.ZERO:
			last_input_direction = Vector2.ZERO
			key_held = false
			time_since_move = move_timer if is_shift_pressed else 0.0
			return Vector2.ZERO
		
		elif current_raw_dir != last_input_direction and !is_shift_pressed and new_pressed:
			# New direction
			time_since_move = 0.0
			last_input_direction = current_raw_dir
			key_held = false
			return just_pressed_dir
			
		else:
			# Repeated direction
			var threshold = hold_move_timer if key_held else move_timer
			if time_since_move > threshold:
				time_since_move = 0.0
				key_held = true
				return current_raw_dir
		
		return Vector2.ZERO

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

	func process(delta: float) -> void:
		var step = move_processor.get_step_vector(delta, manager.is_shift_pressed)
		if step != Vector2.ZERO:
			manager.directional_input.emit(step)

class MapContext extends InputContext:
	var move_processor = DirectionalProcessor.new()
	var view_processor : ViewProcessor

	func _init(p_manager: Node):
		super._init(p_manager)
		view_processor = ViewProcessor.new(p_manager)

	func handle_input(event: InputEvent) -> bool:
		if view_processor.handle_input(event):
			return true
		return false

	func process(delta: float) -> void:
		var step = move_processor.get_step_vector(delta, manager.is_shift_pressed)
		if step != Vector2.ZERO:
			manager.view_panned.emit(-step * FastTileMap.get_tile_size())

class StructureContext extends InputContext:
	var move_processor = DirectionalProcessor.new()
	var view_processor : ViewProcessor

	func _init(p_manager: Node):
		super._init(p_manager)
		view_processor = ViewProcessor.new(p_manager)

	func handle_input(event: InputEvent) -> bool:
		if view_processor.handle_input(event):
			return true
		
		if event.is_action_pressed("structure_undo"): 
			manager.structure_key_input.emit("undo")
			return true
		elif event.is_action_pressed("structure_redo"): 
			manager.structure_key_input.emit("redo")
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
		if step != Vector2.ZERO:
			manager.view_panned.emit(-step * FastTileMap.get_tile_size())

class InventoryContext extends InputContext:
	var move_processor = DirectionalProcessor.new()

	func handle_input(event: InputEvent) -> bool:
		if event.is_action_pressed("ui_accept"):
			manager.inventory_item_selected.emit()
			return true
		elif event.is_action_pressed("drop_item"):
			manager.inventory_drop_requested.emit(manager.is_shift_pressed)
			return true
		return false

	func process(delta: float) -> void:
		var step = move_processor.get_step_vector(delta, manager.is_shift_pressed)
		if step != Vector2.ZERO:
			manager.inventory_directional_input.emit(step)
