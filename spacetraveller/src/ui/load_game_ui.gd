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
	InputManager.ui_cancel.connect(_on_close_pressed_signal)
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

func _on_menu_toggled(id: String, is_open: bool, _params: Dictionary) -> void:
	if id == "load_game":
		if is_open:
			open()
		else:
			visible = false

func open() -> void:
	visible = true
	selectedID = ""
	_update_buttons()
	
	var saves = SaveManager.get_save_list()
	if LoadContainer:
		LoadContainer.set_data(saves)

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
	SaveManager.load_game(selectedID)
	InputManager.set_mode(InputManager.InputMode.EXPLORATION)
	visible = false

func _on_delete_pressed() -> void:
	if selectedID == "": return
	SaveManager.delete_save(selectedID)
	open()

func _on_close_pressed() -> void:
	InputManager.set_mode(InputManager.InputMode.EXPLORATION)
	visible = false

func _on_close_requested() -> void:
	super._on_close_requested()
	visible = false
