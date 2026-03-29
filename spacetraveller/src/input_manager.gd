extends Node

signal map_toggled
signal view_panned(relative: Vector2)
signal view_zoomed(zoom: int)
signal view_centered

signal debug_toggled
signal directional_input(direction: Vector2)
signal ui_directional_input(direction: Vector2)
signal ui_accept
signal ui_cancel
signal ui_delete
signal ui_next_tab
signal ui_prev_tab
signal ui_drop_requested(all: bool)
signal ui_wear_requested
signal inventory_item_dropped(item_id: String, amount: int)

signal menu_toggled(id: String, is_open: bool, params: Dictionary)
signal structure_editor_toggled(active: bool)

signal action_smash_requested
signal action_pickup_requested

signal structure_mode_changed(mode :String)
signal structure_key_input(key :String)
signal structure_mouse_input(button: String, action: MouseAction)
enum MouseAction { PRESS, RELEASE, DRAG }

enum InputMode { EXPLORATION, MAP, STRUCTURE, MENU }
var current_mode: InputMode = InputMode.EXPLORATION
var _mode_stack: Array[InputMode] = []
var active_menu_id: String = ""
var is_shift_pressed = false
var is_ctrl_pressed = false

var contexts = {}
var active_context: InputContext

func _ready() -> void:
	contexts = {
		InputMode.EXPLORATION: InputContext.ExplorationContext.new(self),
		InputMode.MAP: InputContext.MapContext.new(self),
		InputMode.STRUCTURE: InputContext.StructureContext.new(self),
		InputMode.MENU: InputContext.MenuContext.new(self)
	}
	active_context = contexts.get(current_mode)
	ui_cancel.connect(_on_ui_cancel)

func _on_ui_cancel():
	if current_mode == InputMode.MENU:
		pop_mode()

func push_mode(mode: InputMode, _params: Dictionary = {}):
	_mode_stack.append(current_mode)
	set_mode(mode, _params)

func pop_mode():
	if _mode_stack.is_empty():
		return
	
	if current_mode == InputMode.MENU and active_menu_id != "":
		menu_toggled.emit(active_menu_id, false, {})
		active_menu_id = ""
		
	var prev_mode = _mode_stack.pop_back()
	set_mode(prev_mode)

func reset_stack(initial_mode: InputMode = InputMode.EXPLORATION):
	_mode_stack.clear()
	set_mode(initial_mode)

func _unhandled_input(event: InputEvent):
	# Global inputs
	if event.is_action_pressed("debug_mode"):
		debug_toggled.emit()
		get_viewport().set_input_as_handled()
		return

	if event.is_action("shift"):
		is_shift_pressed = event.is_pressed()
	
	if event.is_action("ctrl"):
		is_ctrl_pressed = event.is_pressed()
	
	if active_context:
		if active_context.handle_input(event):
			get_viewport().set_input_as_handled()
			return
	
	# Transition inputs
	if event.is_action_pressed("open_inventory"):
		if current_mode == InputMode.EXPLORATION or current_mode == InputMode.MENU:
			toggle_menu("inventory")
			get_viewport().set_input_as_handled()
			return

	if event.is_action_pressed("open_actions"):
		if current_mode == InputMode.EXPLORATION or current_mode == InputMode.MENU:
			toggle_menu("actions")
			get_viewport().set_input_as_handled()
			return

	if event.is_action_pressed("open_nearby"):
		if current_mode == InputMode.EXPLORATION or current_mode == InputMode.MENU:
			toggle_menu("nearby")
			get_viewport().set_input_as_handled()
			return

	if event.is_action_pressed("open_map"):
		if current_mode == InputMode.MAP:
			pop_mode()
		elif current_mode == InputMode.EXPLORATION:
			push_mode(InputMode.MAP)
		get_viewport().set_input_as_handled()
		return

	if event.is_action_pressed("open_structure_mode"):
		if current_mode == InputMode.STRUCTURE:
			pop_mode()
		elif current_mode == InputMode.EXPLORATION:
			push_mode(InputMode.STRUCTURE)
		get_viewport().set_input_as_handled()
		return

	if event.is_action_pressed("ui_cancel"):
		if current_mode == InputMode.EXPLORATION:
			toggle_menu("esc")
			get_viewport().set_input_as_handled()
			return

func _process(delta):
	if active_context:
		active_context.process(delta)

# Helper for UI or other components to set mode
func set_mode(mode: InputMode, _params: Dictionary = {}):
	if current_mode == mode and mode != InputMode.MENU: return
	
	var old_mode = current_mode
	current_mode = mode
	active_context = contexts.get(current_mode)
	
	# Handle UI signals
	if old_mode == InputMode.MENU and current_mode != InputMode.MENU:
		if active_menu_id != "":
			menu_toggled.emit(active_menu_id, false, {})
			active_menu_id = ""
	
	if old_mode == InputMode.MAP or current_mode == InputMode.MAP:
		map_toggled.emit()
	
	if old_mode == InputMode.STRUCTURE or current_mode == InputMode.STRUCTURE:
		structure_editor_toggled.emit(current_mode == InputMode.STRUCTURE)

func toggle_menu(id: String, params: Dictionary = {}):
	if current_mode == InputMode.MENU and active_menu_id == id and params.is_empty():
		pop_mode()
	else:
		active_menu_id = id
		push_mode(InputMode.MENU, params)
		menu_toggled.emit(active_menu_id, true, params)
