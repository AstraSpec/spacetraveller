extends BaseWindow
class_name OptionsWindow

@export var list_container: ButtonListContainer
@export var reset_button: Button

var cycle_logic: ListValueCycleUI

func _ready() -> void:
	super._ready()
	visible = false
	
	cycle_logic = ListValueCycleUI.new()
	cycle_logic.list_container = list_container
	add_child(cycle_logic)
	
	cycle_logic.value_changed.connect(_on_value_changed)
	if reset_button:
		reset_button.pressed.connect(_on_reset_pressed)
	
	InputManager.menu_toggled.connect(_on_menu_toggled)
	
	refresh_list()

func _on_menu_toggled(id: String, is_open: bool, _params: Dictionary) -> void:
	if id == "options":
		if is_open:
			open()
		else:
			visible = false

func open() -> void:
	call_deferred("set_visible", true)
	refresh_list()

func refresh_list() -> void:
	if not list_container: return
	
	var data = []
	for key in SettingsManager.registry:
		var def = SettingsManager.registry[key]
		if not def.has("ui"): continue
		
		var current_val = SettingsManager.get_val(key)
		var item = {
			"id": key,
			"left": def.ui.label,
		}
		
		if def.ui.type == "cycle":
			var options = def.ui.get_options.call()
			item["options"] = options
			item["current"] = current_val
			item["right"] = str(options[current_val])
		elif def.ui.type == "toggle":
			item["type"] = "toggle"
			item["value"] = current_val
			item["right"] = "On" if current_val else "Off"
			
		data.append(item)
	
	list_container.set_data(data)

func _on_value_changed(id: String, new_value: Variant) -> void:
	var def = SettingsManager.registry.get(id)
	if not def: return
	
	var final_value = new_value
	
	if def.ui.type == "cycle":
		var options = def.ui.get_options.call()
		var idx = options.find(new_value)
		if idx != -1:
			final_value = idx
		else:
			return
	
	SettingsManager.update_and_apply(id, final_value)
	refresh_list()

func _on_reset_pressed() -> void:
	# Reset all registered settings to their defaults
	for key in SettingsManager.registry:
		var def = SettingsManager.registry[key]
		SettingsManager.update_and_apply(key, def.default, false)
	
	SettingsManager.save_settings()
	refresh_list()
