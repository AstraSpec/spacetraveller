extends Window

@export var structureEditor :Node2D
@export var Editor :StructureEditor
@export var StructureID :TextEdit
@export var TypeText :LineEdit
@export var TypePopup :MenuButton
@export var DirsContainer :Control
@export var NorthCheck :CheckBox
@export var EastCheck :CheckBox
@export var SouthCheck :CheckBox
@export var WestCheck :CheckBox
@export var EntranceContainer :Control
@export var EntranceText :LineEdit

@export var CreateButton :Button
@export var ExistingButton :Button
@export var CreateFP :Button
@export var ExistingFP :Button

const DIR_FILEPATH :String = "res://data/structures/"
const DEFAULT_STRUCTURE_TYPE :String = "building"
var currentPath :String = "res://data/structures/structures.json"

func _ready() -> void:
	hide()
	DirsContainer.hide()
	EntranceContainer.hide()
	structureEditor.open_save.connect(open)
	CreateFP.text = DIR_FILEPATH
	ExistingFP.text = currentPath
	_configure_type_picker()

func open() -> void:
	visible = true
	_refresh_type_picker()
	_apply_current_structure_details()
	_apply_feature_type_lock()
	_update_structure_settings_visibility()

func _on_save_pressed() -> void:
	var newID = StructureID.text.strip_edges()
	if newID.is_empty(): return
	
	var structure_size = structureEditor.structure_size
	var structure_data :Dictionary = structureEditor.export_structure(newID, structure_size)
	if structure_data.is_empty():
		return
	var structure_type := _read_structure_type()
	structure_data["type"] = structure_type
	if structure_type == "crypt_room" or !structureEditor.current_dungeon_room_entrances.is_empty():
		structure_data["dungeon_room"] = {
			"entrances": _read_entrance_dirs()
		}
	var city_entrances: Array = []
	if _is_city_structure_type(structure_type):
		city_entrances = _read_city_entrances()
		if !city_entrances.is_empty():
			structure_data["entrance"] = city_entrances
	structureEditor.set_current_structure_details(newID, structure_type, structure_size, _read_entrance_dirs(), city_entrances, currentPath)
	DbAccess.save_structure(newID, structure_data, currentPath)
	visible = false

func _read_structure_type() -> String:
	if structureEditor.is_feature_structure():
		return "feature"
	var type_name := TypeText.text.strip_edges()
	return type_name if !type_name.is_empty() else DEFAULT_STRUCTURE_TYPE

func _read_entrance_dirs() -> Array:
	var dirs: Array = []
	if NorthCheck.button_pressed:
		dirs.append("north")
	if EastCheck.button_pressed:
		dirs.append("east")
	if SouthCheck.button_pressed:
		dirs.append("south")
	if WestCheck.button_pressed:
		dirs.append("west")
	return dirs

func _read_city_entrances() -> Array:
	if !EntranceText:
		return []
	var text := EntranceText.text.strip_edges()
	if text.is_empty():
		return []
	var parsed = JSON.parse_string(text)
	if parsed is Array:
		return parsed
	return []

func _apply_current_structure_details() -> void:
	StructureID.text = structureEditor.current_structure_id
	TypeText.text = structureEditor.current_structure_type
	_apply_entrance_dirs(structureEditor.current_dungeon_room_entrances)
	_apply_city_entrances(structureEditor.current_structure_entrances)
	if !structureEditor.current_structure_path.strip_edges().is_empty():
		currentPath = structureEditor.current_structure_path
	ExistingFP.text = currentPath
	CreateButton.button_pressed = false
	ExistingButton.button_pressed = true
	CreateFP.visible = false
	ExistingFP.visible = true

func _apply_entrance_dirs(dirs: Array) -> void:
	NorthCheck.button_pressed = dirs.has("north")
	EastCheck.button_pressed = dirs.has("east")
	SouthCheck.button_pressed = dirs.has("south")
	WestCheck.button_pressed = dirs.has("west")

func _apply_city_entrances(entrances: Array) -> void:
	EntranceText.text = _format_city_entrances(entrances)

func _format_city_entrances(entrances: Array) -> String:
	if entrances.is_empty():
		return ""
	var parts: Array = []
	for entrance in entrances:
		parts.append(str(int(entrance)))
	return "[" + ", ".join(parts) + "]"

func _configure_type_picker() -> void:
	TypeText.text = DEFAULT_STRUCTURE_TYPE
	if !TypeText.text_changed.is_connected(_on_type_text_changed):
		TypeText.text_changed.connect(_on_type_text_changed)
	TypePopup.text = "Pick"
	var _popup := TypePopup.get_popup()
	if !_popup.index_pressed.is_connected(_on_type_popup_index_pressed):
		_popup.index_pressed.connect(_on_type_popup_index_pressed)
	_refresh_type_picker()
	_apply_feature_type_lock()
	_update_structure_settings_visibility()

func _refresh_type_picker() -> void:
	var _popup := TypePopup.get_popup()
	_popup.clear()
	for type_name in _get_structure_types():
		_popup.add_item(type_name)

func _get_structure_types() -> Array:
	var seen := {}
	var result: Array = []
	var add_type := func(type_name: String) -> void:
		type_name = type_name.strip_edges()
		if type_name.is_empty() or seen.has(type_name):
			return
		seen[type_name] = true
		result.append(type_name)
	add_type.call(DEFAULT_STRUCTURE_TYPE)
	for type_name in StructureDb.get_structure_types():
		var normalized_type := str(type_name).strip_edges()
		if normalized_type != "feature":
			add_type.call(normalized_type)
	return result

func _on_type_popup_index_pressed(index: int) -> void:
	if structureEditor.is_feature_structure():
		return
	var _popup := TypePopup.get_popup()
	if index < 0 or index >= _popup.get_item_count():
		return
	TypeText.text = _popup.get_item_text(index)
	_update_structure_settings_visibility()

func _on_type_text_changed(_new_text: String) -> void:
	_update_structure_settings_visibility()

func _apply_feature_type_lock() -> void:
	var feature_selected = structureEditor.is_feature_structure()
	if feature_selected:
		TypeText.text = "feature"
	TypeText.editable = !feature_selected
	TypePopup.disabled = feature_selected

func _update_structure_settings_visibility() -> void:
	var structure_type := _read_structure_type()
	DirsContainer.visible = structure_type == "crypt_room"
	EntranceContainer.visible = _is_city_structure_type(structure_type)

func _is_city_structure_type(structure_type: String) -> bool:
	structure_type = structure_type.strip_edges()
	if structure_type.is_empty():
		return false
	return ChunkDb.is_city_structure_type(structure_type)

func _on_create_button_pressed() -> void:
	CreateFP.visible = true
	ExistingFP.visible = false

func _on_existing_button_pressed() -> void:
	CreateFP.visible = false
	ExistingFP.visible = true

func _on_close_pressed() -> void:
	visible = false

func _on_close_requested() -> void:
	visible = false

func _on_create_fp_pressed() -> void:
	show_dialog(DisplayServer.FILE_DIALOG_MODE_SAVE_FILE)

func _on_existing_fp_pressed() -> void:
	show_dialog(DisplayServer.FILE_DIALOG_MODE_OPEN_FILE)

func show_dialog(dialogMode):
	var globalPath = ProjectSettings.globalize_path(DIR_FILEPATH)
	var filters = ["*.json ; JSON Files"]
	
	DisplayServer.file_dialog_show(
		"Select a Folder",
		globalPath,
		"",
		false,
		dialogMode,
		filters,
		_on_file_selected)

func _on_file_selected(status: bool, selected_paths: PackedStringArray, _selected_filter_index: int):
	if status and selected_paths.size() > 0:
		var path = selected_paths[0]
		if not path.ends_with(".json"):
			path += ".json"
			
		var localPath = ProjectSettings.localize_path(path)
		currentPath = localPath
		
		if CreateFP.visible:
			CreateFP.text = localPath
		else:
			ExistingFP.text = localPath
