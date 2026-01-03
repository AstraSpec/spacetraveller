extends Window

@export var structureEditor :Node2D
@export var Editor :StructureEditor
@export var StructureID :TextEdit

const DEFAULT_FILEPATH :String = "res://data/structures/structures.json"

func _ready() -> void:
	structureEditor.open_save.connect(open)

func open() -> void:
	visible = true

func _on_save_pressed() -> void:
	var new_id = StructureID.text.strip_edges()
	if new_id.is_empty(): return
	
	var new_rle :Dictionary = Editor.export_to_rle(new_id)
	
	var structures_array :Array = []
	
	if FileAccess.file_exists(DEFAULT_FILEPATH):
		var read_file = FileAccess.open(DEFAULT_FILEPATH, FileAccess.READ)
		var json = JSON.new()
		if json.parse(read_file.get_as_text()) == OK:
			structures_array = json.get_data()
		read_file.close()
	
	var found = false
	for i in range(structures_array.size()):
		if structures_array[i]["id"] == new_id:
			structures_array[i] = new_rle
			found = true
			break
	
	if not found:
		structures_array.append(new_rle)
		
	var write_file = FileAccess.open(DEFAULT_FILEPATH, FileAccess.WRITE)
	write_file.store_string(JSON.stringify(structures_array, "    "))
	write_file.close()
	
	StructureDb.initialize_data()
	visible = false

func _on_close_pressed() -> void:
	visible = false

func _on_close_requested() -> void:
	visible = false
