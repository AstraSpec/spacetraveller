extends Window

@export var tabs: TabContainer
var current_tab: BaseListTab

func _ready() -> void:
	visible = false
	InputManager.menu_toggled.connect(_on_menu_toggled)
	InputManager.ui_directional_input.connect(_on_directional_input)
	InputManager.ui_accept.connect(_on_accept_input)
	InputManager.ui_drop_requested.connect(_on_drop_input)
	InputManager.ui_cancel.connect(_on_close_requested)
	
	if tabs:
		tabs.tab_changed.connect(_on_tab_changed)

func _on_menu_toggled() -> void:
	visible = !visible
	if visible:
		_update_active_tab()

func _on_tab_changed(_tab_index: int) -> void:
	_update_active_tab()

func _update_active_tab():
	if not tabs: return
	
	current_tab = tabs.get_current_tab_control() as BaseListTab
	if current_tab:
		current_tab.refresh_view()

func _on_directional_input(direction: Vector2) -> void:
	if not visible or not current_tab: return
	current_tab.handle_directional_input(direction)

func _on_accept_input():
	if not visible or not current_tab: return
	current_tab._on_item_activated()

func _on_drop_input(all: bool):
	if not visible or not current_tab: return
	if current_tab.has_method("handle_action"):
		current_tab.handle_action("drop", {"all": all})

func _on_close_requested() -> void:
	InputManager.set_mode(InputManager.InputMode.EXPLORATION)
