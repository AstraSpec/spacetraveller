extends Node
class_name ListValueCycleUI

## Reusable component that handles horizontal input for 1-column lists
## to cycle through data-defined options or toggle values.

@export var list_container: ButtonListContainer

signal value_changed(id: String, new_value: Variant)

func _ready() -> void:
	InputManager.ui_directional_input.connect(_on_directional_input)

func _on_directional_input(direction: Vector2) -> void:
	if not _is_active() or not list_container: return
	
	# Always handle vertical navigation
	if direction.y != 0:
		list_container.handle_directional_input(Vector2(0, direction.y))
	
	# Only cycle on horizontal input if in 1-column mode
	if direction.x != 0 and list_container.columns == 1:
		var index = list_container.selected_index
		var data = list_container._get_data_for_button_index(index)
		if not data or not data is Dictionary: return
		
		if data.has("options"):
			_cycle_options(data, int(direction.x))
		elif data.get("type") == "toggle":
			_toggle_value(data)

func _cycle_options(data: Dictionary, delta: int) -> void:
	var options = data["options"]
	var current = data.get("current", 0)
	var next = (current + delta + options.size()) % options.size()
	
	value_changed.emit(data["id"], options[next])

func _toggle_value(data: Dictionary) -> void:
	var current = data.get("value", false)
	value_changed.emit(data["id"], !current)

func _is_active() -> bool:
	var parent = get_parent()
	if parent is CanvasItem:
		return parent.is_visible_in_tree()
	if parent is Window:
		return parent.visible
	return false
