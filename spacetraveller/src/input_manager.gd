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
signal ui_drop_requested(all: bool)
signal inventory_item_dropped(item_id: String, amount: int)

signal menu_toggled

signal action_smash_requested
signal action_pickup_requested

signal structure_mode_changed(mode :String)
signal structure_key_input(key :String)
signal structure_mouse_input(button: String, action: MouseAction)
enum MouseAction { PRESS, RELEASE, DRAG }

enum InputMode { EXPLORATION, MAP, STRUCTURE, MENU }
var current_mode: InputMode = InputMode.EXPLORATION
var is_shift_pressed = false

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

func _unhandled_input(event: InputEvent):
	# Global inputs
	if event.is_action_pressed("debug_mode"):
		debug_toggled.emit()
		get_viewport().set_input_as_handled()
		return

	if event.is_action("shift"):
		is_shift_pressed = event.is_pressed()
	
	# Transition inputs
	if event.is_action_pressed("open_inventory"):
		if current_mode == InputMode.MENU:
			set_mode(InputMode.EXPLORATION)
		elif current_mode == InputMode.EXPLORATION:
			set_mode(InputMode.MENU)
		get_viewport().set_input_as_handled()
		return
		
	if event.is_action_pressed("open_map"):
		if current_mode == InputMode.MAP:
			set_mode(InputMode.EXPLORATION)
		elif current_mode == InputMode.EXPLORATION:
			set_mode(InputMode.MAP)
		get_viewport().set_input_as_handled()
		return

	if active_context:
		if active_context.handle_input(event):
			get_viewport().set_input_as_handled()

func _process(delta):
	if active_context:
		active_context.process(delta)

# Helper for UI or other components to set mode
func set_mode(mode: InputMode):
	if current_mode == mode: return
	
	var old_mode = current_mode
	current_mode = mode
	active_context = contexts.get(current_mode)
	
	# Handle UI signals
	if old_mode == InputMode.MENU or current_mode == InputMode.MENU:
		menu_toggled.emit()
	
	if old_mode == InputMode.MAP or current_mode == InputMode.MAP:
		map_toggled.emit()
