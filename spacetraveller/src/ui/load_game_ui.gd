extends BaseWindow

@export var LoadContainer: ButtonListContainer
@export var LoadButton: Button
@export var DeleteButton: Button

var selectedID: String = ""

func _ready() -> void:
	super._ready()
	visible = false
	InputManager.menu_toggled.connect(_on_menu_toggled)
	InputManager.ui_directional_input.connect(_on_directional_input)
	InputManager.ui_accept.connect(_on_accept_input)
	InputManager.ui_delete.connect(_on_delete_pressed_signal)
	LoadContainer.item_selected.connect(_on_save_selected_idx)
	LoadContainer.item_activated.connect(_on_save_activated)
	_update_buttons()

func _on_directional_input(direction: Vector2) -> void:
	if not visible: return
	LoadContainer.handle_directional_input(direction)

func _on_accept_input() -> void:
	if not visible: return
	_on_load_pressed()

func _on_close_pressed_signal() -> void:
	if not visible: return
	_on_close_pressed()

func _on_delete_pressed_signal() -> void:
	if not visible: return
	_on_delete_pressed()

func _on_menu_toggled(id: String, is_open: bool, _params: Dictionary) -> void:
	if id == "load_game":
		if is_open:
			open()
		else:
			visible = false

func open() -> void:
	call_deferred("set_visible", true)
	selectedID = ""
	InputManager.push_mode(InputManager.InputMode.MENU)
	
	var saves = SaveManager.get_save_list()
	if LoadContainer:
		LoadContainer.set_data(saves)
		if LoadContainer.get_button_count() > 0:
			LoadContainer.selected_index = 0
			var data = LoadContainer._get_data_for_button_index(0)
			_on_save_selected_idx(0, data)
		else:
			_update_buttons()

func _on_save_selected_idx(_idx: int, id: Variant) -> void:
	selectedID = str(id)
	_update_buttons()

func _on_save_activated(_idx: int, id: Variant) -> void:
	selectedID = str(id)
	_on_load_pressed()

func _update_buttons() -> void:
	var hasSelection = selectedID != ""
	LoadButton.disabled = !hasSelection
	DeleteButton.disabled = !hasSelection

func _on_load_pressed() -> void:
	if selectedID == "": return
	
	if not SaveManager.Player:
		# We are on title screen
		if SaveManager.load_save_to_memory(selectedID):
			get_tree().change_scene_to_file("res://main.tscn")
	else:
		# We are in-game
		SaveManager.load_game(selectedID)
		InputManager.pop_mode()
	
	visible = false

func _on_delete_pressed() -> void:
	if selectedID == "": return
	SaveManager.delete_save(selectedID)
	open()

func _on_close_pressed() -> void:
	InputManager.pop_mode()
	visible = false

func _on_close_requested() -> void:
	super._on_close_requested()
	visible = false
