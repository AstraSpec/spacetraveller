extends Window

@export var structureEditor :Node2D
@export var Editor :StructureEditor
@export var StructureID :TextEdit
@export var FPButton :Button

const DIR_FILEPATH :String = "res://data/structures/"
var currentPath :String = "res://data/structures/structures.json"

func _ready() -> void:
	structureEditor.open_save.connect(open)
	FPButton.text = currentPath

func open() -> void:
	visible = true

func _on_save_pressed() -> void:
	var newID = StructureID.text.strip_edges()
	if newID.is_empty(): return
	
	var newRLE :Dictionary = Editor.export_to_rle(newID)
	DbAccess.save_structure(newID, newRLE, currentPath)
	visible = false

func _on_close_pressed() -> void:
	visible = false

func _on_close_requested() -> void:
	visible = false

func _on_fp_button_pressed() -> void:
	var globalPath = ProjectSettings.globalize_path(DIR_FILEPATH)
	var filters = ["*.json ; JSON Files"]
	
	DisplayServer.file_dialog_show(
		"Select a Folder",
		globalPath,
		"",
		false,
		DisplayServer.FILE_DIALOG_MODE_OPEN_FILE,
		filters,
		_on_file_selected)

func _on_file_selected(status: bool, selected_paths: PackedStringArray, _selected_filter_index: int):
	if status and selected_paths.size() > 0:
		var path = selected_paths[0]
		if not path.ends_with(".json"):
			path += ".json"
			
		var localPath = ProjectSettings.localize_path(path)
		currentPath = localPath
		FPButton.text = localPath
