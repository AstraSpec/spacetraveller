extends BaseWindow

@export var tabs: TabContainer
@export var start_button: Button
@export var cancel_button: Button
@export var ConfirmationPopup :Window

var current_tab: BaseListTab

func _ready() -> void:
	super._ready()
	visible = false

	tabs.tab_changed.connect(_on_tab_changed)
	start_button.pressed.connect(_on_start_pressed)
	cancel_button.pressed.connect(request_close)

	InputManager.menu_toggled.connect(_on_menu_toggled)
	InputManager.menu_close_requested.connect(_on_menu_close_requested)
	InputManager.ui_directional_input.connect(_on_directional_input)
	InputManager.ui_accept.connect(_on_accept_input)
	InputManager.ui_next_tab.connect(_on_next_tab)
	InputManager.ui_prev_tab.connect(_on_prev_tab)

func _on_menu_toggled(id: String, is_open: bool, params: Dictionary) -> void:
	if id == "new_game":
		visible = is_open
		if visible:
			open(params)
	elif is_open:
		visible = false

func open(_params: Dictionary = {}) -> void:
	if ScenarioDb.get_ids().is_empty():
		ScenarioDb.initialize_data()

	tabs.current_tab = 0
	_update_active_tab()

func request_close() -> void:
	if not visible:
		return
	InputManager.pop_mode()

func _on_menu_close_requested(id: String) -> void:
	if id == "new_game":
		request_close()

func _on_next_tab() -> void:
	if not visible:
		return
	tabs.current_tab = (tabs.current_tab + 1) % tabs.get_tab_count()

func _on_prev_tab() -> void:
	if not visible:
		return
	tabs.current_tab = (tabs.current_tab - 1 + tabs.get_tab_count()) % tabs.get_tab_count()

func _on_tab_changed(_tab_index: int) -> void:
	_update_active_tab()

func _update_active_tab() -> void:
	current_tab = tabs.get_current_tab_control() as BaseListTab
	current_tab.refresh_view()
	_update_start_button()

func _on_directional_input(direction: Vector2) -> void:
	if not visible:
		return
	current_tab.handle_directional_input(direction)
	_update_start_button()

func _on_accept_input() -> void:
	if not visible:
		return
	_on_start_pressed()

func _on_start_pressed() -> void:
	if not visible:
		return
	if _get_selected_scenario_id().is_empty():
		return

	ConfirmationPopup.show_confirm("Start this game?", [
		{"label": "No"},
		{"label": "Yes", "callback": Callable(self, "_confirm_start_game")},
	])

func _confirm_start_game() -> void:
	var scenario_id: String = _get_selected_scenario_id()
	SaveManager.begin_new_game({
		"scenario_id": scenario_id,
	})

	if InputManager.current_mode == InputManager.InputMode.MENU and InputManager.active_menu_id == "new_game":
		InputManager.pop_mode()

	get_tree().change_scene_to_file("res://main.tscn")

func _get_selected_scenario_id() -> String:
	return str(current_tab.call("get_selected_scenario_id"))

func _update_start_button() -> void:
	start_button.disabled = _get_selected_scenario_id().is_empty()
