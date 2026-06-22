extends Window

@export var structureEditor :Node2D
@export var Editor :StructureEditor
@export var StructureID :TextEdit
@export var WidthText :Control
@export var HeightText :Control
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
const DEFAULT_STRUCTURE_SIZE :int = 24
const DEFAULT_STRUCTURE_TYPE :String = "building"
var currentPath :String = "res://data/structures/structures.json"

func _ready() -> void:
	hide()
	if DirsContainer:
		DirsContainer.hide()
	if EntranceContainer:
		EntranceContainer.hide()
	structureEditor.open_save.connect(open)
	CreateFP.text = DIR_FILEPATH
	ExistingFP.text = currentPath
	_configure_size_input(WidthText)
	_configure_size_input(HeightText)
	_configure_type_picker()

func open() -> void:
	visible = true
	_refresh_type_picker()
	_apply_current_structure_details()
	_update_structure_settings_visibility()

func _on_save_pressed() -> void:
	var newID = StructureID.text.strip_edges()
	if newID.is_empty(): return
	
	var structure_data :Dictionary = structureEditor.export_structure(newID, _read_structure_size())
	var structure_type := _read_structure_type()
	structure_data["type"] = structure_type
	if structure_type == "crypt_room":
		structure_data["dungeon_room"] = {
			"entrances": _read_entrance_dirs()
		}
	var city_entrances: Array = []
	if _is_city_structure_type(structure_type):
		city_entrances = _read_city_entrances()
		if !city_entrances.is_empty():
			structure_data["entrance"] = city_entrances
	structureEditor.set_current_structure_details(newID, structure_type, _read_structure_size(), _read_entrance_dirs(), city_entrances, currentPath)
	DbAccess.save_structure(newID, structure_data, currentPath)
	visible = false

func _configure_size_input(input: Control) -> void:
	if input is SpinBox:
		input.min_value = 1
		input.max_value = DEFAULT_STRUCTURE_SIZE
		input.step = 1
		input.rounded = true
		input.value = DEFAULT_STRUCTURE_SIZE
		input.allow_greater = false
		input.allow_lesser = false

func _read_structure_size() -> Vector2i:
	return Vector2i(_read_dimension(WidthText), _read_dimension(HeightText))

func _read_dimension(input: Control) -> int:
	if input is SpinBox:
		var spin_value := int(round(input.value))
		return spin_value if spin_value >= 1 and spin_value <= DEFAULT_STRUCTURE_SIZE else DEFAULT_STRUCTURE_SIZE
	if input != null:
		var text := str(input.get("text")).strip_edges()
		if text.is_valid_int():
			var text_value := int(text)
			return text_value if text_value >= 1 and text_value <= DEFAULT_STRUCTURE_SIZE else DEFAULT_STRUCTURE_SIZE
	return DEFAULT_STRUCTURE_SIZE

func _read_structure_type() -> String:
	if !TypeText:
		return DEFAULT_STRUCTURE_TYPE
	var type_name := TypeText.text.strip_edges()
	return type_name if !type_name.is_empty() else DEFAULT_STRUCTURE_TYPE

func _read_entrance_dirs() -> Array:
	var dirs: Array = []
	if NorthCheck and NorthCheck.button_pressed:
		dirs.append("north")
	if EastCheck and EastCheck.button_pressed:
		dirs.append("east")
	if SouthCheck and SouthCheck.button_pressed:
		dirs.append("south")
	if WestCheck and WestCheck.button_pressed:
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
	if !structureEditor:
		return
	if StructureID:
		StructureID.text = structureEditor.current_structure_id
	_set_dimension(WidthText, structureEditor.structure_size.x)
	_set_dimension(HeightText, structureEditor.structure_size.y)
	if TypeText:
		TypeText.text = structureEditor.current_structure_type
	_apply_entrance_dirs(structureEditor.current_dungeon_room_entrances)
	_apply_city_entrances(structureEditor.current_structure_entrances)
	if !structureEditor.current_structure_path.strip_edges().is_empty():
		currentPath = structureEditor.current_structure_path
	if ExistingFP:
		ExistingFP.text = currentPath
	if CreateButton and ExistingButton:
		CreateButton.button_pressed = false
		ExistingButton.button_pressed = true
	if CreateFP:
		CreateFP.visible = false
	if ExistingFP:
		ExistingFP.visible = true

func _set_dimension(input: Control, value: int) -> void:
	var dimension := int(clamp(value, 1, DEFAULT_STRUCTURE_SIZE))
	if input is SpinBox:
		input.value = dimension
	elif input != null:
		input.set("text", str(dimension))

func _apply_entrance_dirs(dirs: Array) -> void:
	if NorthCheck:
		NorthCheck.button_pressed = dirs.has("north")
	if EastCheck:
		EastCheck.button_pressed = dirs.has("east")
	if SouthCheck:
		SouthCheck.button_pressed = dirs.has("south")
	if WestCheck:
		WestCheck.button_pressed = dirs.has("west")

func _apply_city_entrances(entrances: Array) -> void:
	if !EntranceText:
		return
	EntranceText.text = _format_city_entrances(entrances)

func _format_city_entrances(entrances: Array) -> String:
	if entrances.is_empty():
		return ""
	var parts: Array = []
	for entrance in entrances:
		parts.append(str(int(entrance)))
	return "[" + ", ".join(parts) + "]"

func _configure_type_picker() -> void:
	if TypeText:
		TypeText.text = DEFAULT_STRUCTURE_TYPE
		if !TypeText.text_changed.is_connected(_on_type_text_changed):
			TypeText.text_changed.connect(_on_type_text_changed)
	if TypePopup:
		TypePopup.text = "Pick"
		var _popup := TypePopup.get_popup()
		if !_popup.index_pressed.is_connected(_on_type_popup_index_pressed):
			_popup.index_pressed.connect(_on_type_popup_index_pressed)
	_refresh_type_picker()
	_update_structure_settings_visibility()

func _refresh_type_picker() -> void:
	if !TypePopup:
		return
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
		add_type.call(str(type_name))
	return result

func _on_type_popup_index_pressed(index: int) -> void:
	if !TypeText or !TypePopup:
		return
	var _popup := TypePopup.get_popup()
	if index < 0 or index >= _popup.get_item_count():
		return
	TypeText.text = _popup.get_item_text(index)
	_update_structure_settings_visibility()

func _on_type_text_changed(_new_text: String) -> void:
	_update_structure_settings_visibility()

func _update_structure_settings_visibility() -> void:
	var structure_type := _read_structure_type()
	if DirsContainer:
		DirsContainer.visible = structure_type == "crypt_room"
	if EntranceContainer:
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
