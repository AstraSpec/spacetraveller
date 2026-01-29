extends Window

@export var menu_id: String
@export var tabs: TabContainer
@export var single_tab: BaseListTab

var current_tab: BaseListTab

func _ready() -> void:
	visible = false
	InputManager.menu_toggled.connect(_on_menu_toggled)
	InputManager.ui_directional_input.connect(_on_directional_input)
	InputManager.ui_accept.connect(_on_accept_input)
	InputManager.ui_drop_requested.connect(_on_drop_input)
	InputManager.ui_cancel.connect(_on_close_requested)
	InputManager.ui_next_tab.connect(_on_next_tab)
	InputManager.ui_prev_tab.connect(_on_prev_tab)
	
	if tabs:
		tabs.tab_changed.connect(_on_tab_changed)

func _on_next_tab() -> void:
	if not visible or not tabs: return
	tabs.current_tab = (tabs.current_tab + 1) % tabs.get_tab_count()

func _on_prev_tab() -> void:
	if not visible or not tabs: return
	tabs.current_tab = (tabs.current_tab - 1 + tabs.get_tab_count()) % tabs.get_tab_count()

func _on_menu_toggled(id: String, is_open: bool) -> void:
	if id == menu_id:
		visible = is_open
		if visible:
			_update_active_tab()
	else:
		if is_open:
			visible = false

func _on_tab_changed(_tab_index: int) -> void:
	_update_active_tab()

func _update_active_tab():
	if tabs:
		current_tab = tabs.get_current_tab_control() as BaseListTab
	elif single_tab:
		current_tab = single_tab
	else:
		current_tab = null
		
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
