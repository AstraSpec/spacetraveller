extends Node

signal map_toggled(is_open: bool)
signal view_panned(relative: Vector2)
signal view_zoomed(zoom: int)
signal view_centered

signal debug_toggled
signal directional_input(direction: Vector2)
signal map_directional_input(direction: Vector2)
signal structure_directional_input(direction: Vector2)
signal inventory_directional_input(direction: Vector2)
signal inventory_item_selected
signal inventory_drop_requested(all: bool)
signal inventory_item_dropped(item_id: String, amount: int)

signal inventory_toggled(is_open: bool)

signal structure_mode_changed(mode :String)
signal structure_key_input(key :String)
signal structure_mouse_input(button: String, action: MouseAction)
enum MouseAction { PRESS, RELEASE, DRAG }

enum InputMode { EXPLORATION, MAP, STRUCTURE, INVENTORY }
var current_mode: InputMode = InputMode.EXPLORATION:
	set(value):
		if current_mode == value: return
		var old_mode = current_mode
		current_mode = value
		_update_context()
		_handle_mode_transition(old_mode, value)

var is_shift_pressed = false

var contexts = {}
var active_context: InputContext

func _ready() -> void:
	contexts = {
		InputMode.EXPLORATION: InputContext.ExplorationContext.new(self),
		InputMode.MAP: InputContext.MapContext.new(self),
		InputMode.STRUCTURE: InputContext.StructureContext.new(self),
		InputMode.INVENTORY: InputContext.InventoryContext.new(self)
	}
	_update_context()

func _update_context() -> void:
	active_context = contexts.get(current_mode)

func _handle_mode_transition(old_mode: InputMode, new_mode: InputMode):
	if old_mode == InputMode.INVENTORY or new_mode == InputMode.INVENTORY:
		inventory_toggled.emit(new_mode == InputMode.INVENTORY)
	
	if old_mode == InputMode.MAP or new_mode == InputMode.MAP:
		map_toggled.emit(new_mode == InputMode.MAP)

func _unhandled_input(event: InputEvent):
	# Global inputs
	if event.is_action_pressed("debug_mode"):
		debug_toggled.emit()
		get_viewport().set_input_as_handled()
		return

	if event.is_action("shift"):
		is_shift_pressed = event.is_pressed()
	
	if active_context:
		if active_context.handle_input(event):
			get_viewport().set_input_as_handled()

func _process(delta):
	if active_context:
		active_context.process(delta)

# Helper for UI or other components to set mode
func set_mode(mode: InputMode):
	current_mode = mode

func exit_to_exploration():
	current_mode = InputMode.EXPLORATION
