extends BaseWindow

@export var menu_id: String
@export var tabs: TabContainer
@export var single_tab: BaseListTab
@export var default_tab_on_open: String = ""

var current_tab: Control
var menu_params: Dictionary = {}

func _ready() -> void:
	super._ready()
	visible = false
	InputManager.menu_toggled.connect(_on_menu_toggled)
	InputManager.ui_directional_input.connect(_on_directional_input)
	InputManager.ui_accept.connect(_on_accept_input)
	InputManager.ui_drop_requested.connect(_on_drop_input)
	InputManager.ui_wear_requested.connect(_on_wear_input)
	InputManager.ui_wield_requested.connect(_on_wield_input)
	InputManager.menu_close_requested.connect(_on_menu_close_requested)
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

func _on_menu_toggled(id: String, is_open: bool, params: Dictionary) -> void:
	if id == menu_id:
		if visible and not is_open:
			_notify_menu_closed()
			menu_params.clear()
		visible = is_open
		if visible:
			menu_params = params.duplicate(true)
			if params.has("tab"):
				_switch_to_tab_by_name(params["tab"])
			elif not default_tab_on_open.is_empty():
				_switch_to_tab_by_name(default_tab_on_open)
			
			_update_active_tab(true)
	else:
		# If another menu is opened, close this one
		if is_open:
			if visible:
				_notify_menu_closed()
			menu_params.clear()
			visible = false

func request_close() -> void:
	if not visible:
		return
	if _tabs_request_menu_close():
		return
	InputManager.pop_mode()

func _on_menu_close_requested(id: String) -> void:
	if id == menu_id:
		request_close()

func _tabs_request_menu_close() -> bool:
	if tabs:
		for child in tabs.get_children():
			if child.has_method("request_menu_close") and bool(child.request_menu_close()):
				return true
	elif single_tab and single_tab.has_method("request_menu_close"):
		return bool(single_tab.request_menu_close())
	return false

func _switch_to_tab_by_name(tab_name: String) -> void:
	if not tabs: return
	for i in range(tabs.get_tab_count()):
		if tabs.get_tab_title(i).to_lower() == tab_name.to_lower():
			tabs.current_tab = i
			return

func _on_tab_changed(_tab_index: int) -> void:
	_update_active_tab()

func _update_active_tab(apply_params: bool = false):
	if tabs:
		current_tab = tabs.get_current_tab_control()
	elif single_tab:
		current_tab = single_tab
	else:
		current_tab = null
		
	if current_tab:
		if apply_params and current_tab.has_method("set_params"):
			current_tab.call("set_params", menu_params)
		elif current_tab.has_method("refresh_view"):
			current_tab.call("refresh_view")

func _notify_menu_closed() -> void:
	if tabs:
		for child in tabs.get_children():
			if child.has_method("on_menu_closed"):
				child.on_menu_closed()
	elif single_tab and single_tab.has_method("on_menu_closed"):
		single_tab.on_menu_closed()

func _on_directional_input(direction: Vector2) -> void:
	if not visible or not current_tab: return
	if current_tab.has_method("handle_directional_input"):
		current_tab.call("handle_directional_input", direction)

func _on_accept_input():
	if not visible or not current_tab: return
	if current_tab.has_method("_on_item_activated"):
		current_tab.call("_on_item_activated")

func _on_drop_input(all: bool):
	if not visible or not current_tab: return
	if current_tab.has_method("handle_action"):
		current_tab.call("handle_action", "drop", {"all": all})

func _on_wear_input():
	if not visible or not current_tab: return
	if current_tab.has_method("handle_action"):
		current_tab.call("handle_action", "wear")

func _on_wield_input():
	if not visible or not current_tab: return
	if current_tab.has_method("handle_action"):
		current_tab.call("handle_action", "wield")
