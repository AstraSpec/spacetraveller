extends Node
class_name ListActionsUI

## Reusable component that handles the interaction logic between a [ButtonListContainer]
## and its contextual action buttons (e.g., Load, Delete, etc.).

@export var list_container: ButtonListContainer
@export var action_buttons: Array[Button] = []
@export var delete_button: Button

signal action_triggered(data: Variant)
signal delete_requested(data: Variant)
signal selection_changed(data: Variant)

var current_data: Variant = null

func _ready() -> void:
	if list_container:
		list_container.item_selected.connect(_on_item_selected)
		list_container.item_activated.connect(_on_item_activated)
	
	InputManager.ui_directional_input.connect(_on_directional_input)
	InputManager.ui_accept.connect(_on_accept_input)
	InputManager.ui_delete.connect(_on_delete_input)
	
	_update_button_states()

func _on_item_selected(_idx: int, data: Variant) -> void:
	current_data = data
	_update_button_states()
	selection_changed.emit(data)

func _on_item_activated(_idx: int, data: Variant) -> void:
	if not _is_active(): return
	current_data = data
	action_triggered.emit(data)

func _on_directional_input(direction: Vector2) -> void:
	if not _is_active() or not list_container: return
	list_container.handle_directional_input(direction)

func _on_accept_input() -> void:
	if not _is_active() or current_data == null: return
	action_triggered.emit(current_data)

func _on_delete_input() -> void:
	if not _is_active() or current_data == null: return
	delete_requested.emit(current_data)

func _update_button_states() -> void:
	var has_selection = current_data != null
	for btn in action_buttons:
		if btn: btn.disabled = !has_selection
	if delete_button:
		delete_button.disabled = !has_selection

func _is_active() -> bool:
	if InputManager.is_capturing: return false
	
	# Only process input if the parent UI is actually visible
	var parent = get_parent()
	if parent is CanvasItem:
		return parent.is_visible_in_tree()
	if parent is Window:
		return parent.visible
	return false
