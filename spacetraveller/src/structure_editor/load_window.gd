extends Window

@export var structureEditor :Node2D
@export var Editor :StructureEditor
@export var LoadContainer :ButtonListContainer
@export var LoadButton :Button
@export var DeleteButton :Button
var World :GameWorld
var FastTilemap :FastTileMap

var list_actions: ListActionsUI
var selectedID : String = ""
var _menu_mode_pushed: bool = false
var _last_click_id: String = ""
var _last_click_msec: int = 0
const DOUBLE_CLICK_MSEC := 350

func _ready() -> void:
	hide()
	structureEditor.open_load.connect(open)
	LoadContainer.activate_on_single_click = false
	
	# Initialize ListActionsUI logic
	list_actions = ListActionsUI.new()
	list_actions.list_container = LoadContainer
	list_actions.action_buttons = [LoadButton]
	list_actions.delete_button = DeleteButton
	add_child(list_actions)
	
	list_actions.action_triggered.connect(func(_data): _on_load_pressed())
	list_actions.delete_requested.connect(func(_data): _on_delete_pressed())
	list_actions.selection_changed.connect(_on_structure_selected)
	LoadContainer.item_clicked.connect(_on_structure_clicked)
	InputManager.ui_cancel.connect(_on_cancel_input)
	
	_update_buttons()

func _on_structure_selected(id: Variant) -> void:
	selectedID = str(id) if id != null else ""
	_update_buttons()

func _on_structure_clicked(_index: int, id: Variant) -> void:
	var clicked_id: String = str(id) if id != null else ""
	var now := Time.get_ticks_msec()
	if clicked_id != "" and clicked_id == _last_click_id and now - _last_click_msec <= DOUBLE_CLICK_MSEC:
		selectedID = clicked_id
		_on_load_pressed()
		_last_click_id = ""
		_last_click_msec = 0
	else:
		_last_click_id = clicked_id
		_last_click_msec = now

func open() -> void:
	popup_centered()
	selectedID = ""
	_last_click_id = ""
	_last_click_msec = 0
	if not _menu_mode_pushed:
		InputManager.push_mode(InputManager.InputMode.MENU)
		_menu_mode_pushed = true
	
	var structures = StructureDb.get_ids()
	if LoadContainer:
		LoadContainer.set_data(structures)
		if LoadContainer.get_button_count() > 0:
			LoadContainer.selected_index = 0
			var data = LoadContainer._get_data_for_button_index(0)
			_on_structure_selected(data)
		else:
			_update_buttons()

func _update_buttons() -> void:
	var hasSelection = selectedID != ""
	LoadButton.disabled = !hasSelection
	DeleteButton.disabled = !hasSelection

func _on_load_pressed() -> void:
	if selectedID == "": return
	
	var source_path := DbAccess.find_structure_file(selectedID)
	if source_path.is_empty():
		source_path = DbAccess.DIR_FILEPATH.path_join("structures.json")
	var raw_structure_data := DbAccess.get_structure_data(selectedID, source_path)
	var structure_data: Dictionary = {
		"id": selectedID,
		"type": StructureDb.get_structure_type(selectedID),
		"size": StructureDb.get_structure_size(selectedID),
		"levels": StructureDb.get_levels(selectedID),
		"filepath": source_path,
		"entrance": raw_structure_data.get("entrance", []),
		"dungeon_room": {
			"entrances": StructureDb.get_dungeon_room_entrances(selectedID)
		}
	}
	var structure_levels: Dictionary = structure_data["levels"]
	
	if structure_levels.is_empty():
		printerr("Failed to load raw data for: ", selectedID)
		return
		
	structureEditor.save_undo_state()
	structureEditor.import_structure(structure_data)
	structureEditor.update_editor_visuals()
	_close_window()

func _on_delete_pressed() -> void:
	if selectedID == "": return
	DbAccess.delete_structure(selectedID)
	selectedID = ""
	_last_click_id = ""
	_last_click_msec = 0
	var structures = StructureDb.get_ids()
	if LoadContainer:
		LoadContainer.set_data(structures)
		if LoadContainer.get_button_count() > 0:
			LoadContainer.selected_index = 0
			var data = LoadContainer._get_data_for_button_index(0)
			_on_structure_selected(data)
		else:
			_update_buttons()

func _on_close_pressed() -> void:
	_close_window()

func _on_close_requested() -> void:
	_close_window()

func _on_cancel_input() -> void:
	if visible:
		_close_window()

func _close_window() -> void:
	hide()
	if _menu_mode_pushed:
		if InputManager.current_mode == InputManager.InputMode.MENU:
			InputManager.pop_mode()
		_menu_mode_pushed = false
