extends Window

@export var structureEditor :Node2D
@export var Editor :StructureEditor
@export var StructureID :TextEdit

@export var CreateButton :Button
@export var ExistingButton :Button
@export var CreateFP :Button
@export var ExistingFP :Button

const DIR_FILEPATH :String = "res://data/structures/"
var currentPath :String = "res://data/structures/structures.json"

func _ready() -> void:
	structureEditor.open_save.connect(open)
	CreateFP.text = DIR_FILEPATH
	ExistingFP.text = currentPath

func open() -> void:
	visible = true

func _on_save_pressed() -> void:
	var newID = StructureID.text.strip_edges()
	if newID.is_empty(): return
	
	var newRLE :Dictionary = Editor.export_to_rle(newID)
	DbAccess.save_structure(newID, newRLE, currentPath)
	visible = false

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
