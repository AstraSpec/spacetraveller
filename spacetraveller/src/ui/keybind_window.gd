extends BaseWindow
class_name KeybindWindow

@export var list_container: ButtonListContainer
@export var reset_button: Button

var list_actions: ListActionsUI
var current_action_id: String = ""
var is_rebinding: bool = false
var InputSettings = preload("res://src/input_settings.gd")

func _ready() -> void:
	super._ready()
	visible = false
	
	# Initialize ListActionsUI logic
	list_actions = ListActionsUI.new()
	list_actions.list_container = list_container
	# reset_button is global so it doesn't need to be in list_actions.action_buttons
	add_child(list_actions)
	
	list_actions.action_triggered.connect(_on_keybind_activated)
	
	if reset_button:
		reset_button.pressed.connect(_on_reset_pressed)
	
	InputManager.menu_toggled.connect(_on_menu_toggled)
	InputManager.input_captured.connect(_on_input_captured)
	
	# Initial populate
	refresh_list()

func _on_menu_toggled(id: String, is_open: bool, _params: Dictionary) -> void:
	if id == "keybinds":
		if is_open:
			open()
		else:
			visible = false
			is_rebinding = false

func open() -> void:
	call_deferred("set_visible", true)
	refresh_list()

func refresh_list() -> void:
	if list_container:
		var data = InputSettings.get_full_list()
		list_container.set_data(data)

func _on_keybind_activated(data: Variant) -> void:
	if not data or is_rebinding: return
	if not data is Dictionary or not data.has("id"): return
	
	current_action_id = data.id
	is_rebinding = true
	
	# Set visually that we are listening
	var btn = list_container.get_button(list_container.selected_index)
	if btn and btn.has_method("set_underline"):
		btn.set_underline(false, true)
	
	InputManager.start_capture()

func _on_input_captured(event: InputEvent) -> void:
	if not is_rebinding: return
	
	if event is InputEventKey and event.keycode == KEY_ESCAPE:
		_cancel_rebinding()
		return
	
	_rebind_action(current_action_id, event)

func _rebind_action(action_id: String, event: InputEvent) -> void:
	# Erase existing keyboard/joypad mappings for this action
	var old_events = InputMap.action_get_events(action_id)
	for old_event in old_events:
		if old_event is InputEventKey or old_event is InputEventJoypadButton:
			InputMap.action_erase_event(action_id, old_event)
	
	# Add the new event
	InputMap.action_add_event(action_id, event)
	
	is_rebinding = false
	refresh_list()

func _cancel_rebinding() -> void:
	is_rebinding = false
	refresh_list()

func _on_reset_pressed() -> void:
	# Use the startup snapshot from InputManager for a reliable reset
	for action_id in InputManager.default_mappings:
		# Clear current mappings for this action
		InputMap.action_erase_events(action_id)
		# Restore the defaults from our snapshot
		for event in InputManager.default_mappings[action_id]:
			InputMap.action_add_event(action_id, event)
	
	refresh_list()

func _on_close_requested() -> void:
	super._on_close_requested()
	visible = false
	is_rebinding = false
