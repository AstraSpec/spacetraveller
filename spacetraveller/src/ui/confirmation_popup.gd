extends Window
class_name ConfirmationPopup

var actions: Array = []
var buttons: Array = []
var selected_index: int = 0
var mode_pushed: bool = false

@export var _message_label: RichTextLabel
@export var _button_row: HBoxContainer

func _ready() -> void:
	visible = false
	InputManager.confirmation_directional_input.connect(_on_directional_input)
	InputManager.confirmation_accept.connect(_on_accept)
	InputManager.confirmation_cancel.connect(_on_cancel)

func show_confirm(message: String, _actions: Array) -> void:
	if _actions.is_empty():
		return

	actions = _normalize_actions(_actions)
	selected_index = _find_action("No")
	_message_label.text = message
	_rebuild_buttons()
	visible = true
	grab_focus()
	_update_selection()

	if not mode_pushed:
		mode_pushed = true
		InputManager.push_mode(InputManager.InputMode.CONFIRMATION)

func close_popup() -> void:
	visible = false
	actions.clear()
	buttons.clear()
	for child in _button_row.get_children():
		child.queue_free()

	if mode_pushed:
		mode_pushed = false
		InputManager.pop_mode()

func _rebuild_buttons() -> void:
	for child in _button_row.get_children():
		child.queue_free()
	buttons.clear()

	for i in range(actions.size()):
		var button := Button.new()
		button.text = str(actions[i].get("label", ""))
		button.focus_mode = Control.FOCUS_NONE
		button.custom_minimum_size = Vector2(96, 32)
		_set_button_background(button, false)
		button.pressed.connect(_choose_option.bind(i))
		_button_row.add_child(button)
		buttons.append(button)

func _on_directional_input(direction: Vector2) -> void:
	if not visible or buttons.is_empty():
		return
	if direction.x < 0:
		selected_index = (selected_index - 1 + buttons.size()) % buttons.size()
	elif direction.x > 0:
		selected_index = (selected_index + 1) % buttons.size()
	elif direction.y < 0:
		selected_index = (selected_index - 1 + buttons.size()) % buttons.size()
	elif direction.y > 0:
		selected_index = (selected_index + 1) % buttons.size()
	_update_selection()

func _on_accept() -> void:
	if visible:
		_choose_option(selected_index)

func _on_cancel() -> void:
	if not visible:
		return
	var cancel_index := _find_action("Cancel")
	if cancel_index < 0:
		cancel_index = _find_action("No")
	if cancel_index < 0:
		cancel_index = actions.size() - 1
	_choose_option(cancel_index)

func _choose_option(index: int) -> void:
	if index < 0 or index >= actions.size():
		return
	var action: Dictionary = actions[index]
	var callback: Callable = action.get("callback", Callable())
	close_popup()
	if callback.is_valid():
		callback.call()

func _update_selection() -> void:
	for i in range(buttons.size()):
		_set_button_background(buttons[i], i == selected_index)

func _set_button_background(button: Button, selected: bool) -> void:
	var style := StyleBoxFlat.new()
	style.bg_color = Color(0.33, 0.33, 0.33) if selected else Color(0.08, 0.08, 0.08)
	button.add_theme_stylebox_override("normal", style)

func _normalize_actions(raw_actions: Array) -> Array:
	var normalized: Array = []
	for raw_action in raw_actions:
		if raw_action is Dictionary:
			normalized.append(raw_action.duplicate())
		else:
			normalized.append({"label": str(raw_action)})
	return normalized

func _find_action(label: String) -> int:
	for i in range(actions.size()):
		if str(actions[i].get("label", "")) == label:
			return i
	return 0 if not actions.is_empty() else -1
