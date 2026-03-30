extends BaseWindow

@export var LoadContainer: ButtonListContainer
@export var LoadButton: Button
@export var DeleteButton: Button

var list_actions: ListActionsUI
var selectedID: String = ""

func _ready() -> void:
	super._ready()
	visible = false
	
	# Initialize ListActionsUI logic
	list_actions = ListActionsUI.new()
	list_actions.list_container = LoadContainer
	list_actions.action_buttons = [LoadButton]
	list_actions.delete_button = DeleteButton
	add_child(list_actions)
	
	list_actions.action_triggered.connect(func(_data): _on_load_pressed())
	list_actions.delete_requested.connect(func(_data): _on_delete_pressed())
	list_actions.selection_changed.connect(_on_save_selected)
	
	InputManager.menu_toggled.connect(_on_menu_toggled)
	_update_buttons()

func _on_save_selected(id: Variant) -> void:
	selectedID = str(id) if id != null else ""
	_update_buttons()

func _on_menu_toggled(id: String, is_open: bool, _params: Dictionary) -> void:
	if id == "load_game":
		if is_open:
			open()
		else:
			visible = false

func open() -> void:
	call_deferred("set_visible", true)
	selectedID = ""
	
	var saves = SaveManager.get_save_list()
	if LoadContainer:
		LoadContainer.set_data(saves)
		if LoadContainer.get_button_count() > 0:
			LoadContainer.selected_index = 0
			var data = LoadContainer._get_data_for_button_index(0)
			_on_save_selected(data)
		else:
			_update_buttons()

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
	_on_close_requested()

func _on_close_requested() -> void:
	super._on_close_requested()
	visible = false
