extends Window

@export var structureEditor :Node2D
@export var Editor :StructureEditor
@export var StructureID :TextEdit
@export var WidthText :Control
@export var HeightText :Control

@export var CreateButton :Button
@export var ExistingButton :Button
@export var CreateFP :Button
@export var ExistingFP :Button

const DIR_FILEPATH :String = "res://data/structures/"
const DEFAULT_STRUCTURE_SIZE :int = 24
var currentPath :String = "res://data/structures/structures.json"

func _ready() -> void:
	structureEditor.open_save.connect(open)
	CreateFP.text = DIR_FILEPATH
	ExistingFP.text = currentPath
	_configure_size_input(WidthText)
	_configure_size_input(HeightText)

func open() -> void:
	visible = true

func _on_save_pressed() -> void:
	var newID = StructureID.text.strip_edges()
	if newID.is_empty(): return
	
	var structure_data :Dictionary = structureEditor.export_structure(newID, _read_structure_size())
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
