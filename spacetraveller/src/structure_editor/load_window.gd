extends Window

@export var structureEditor :Node2D
@export var Editor :StructureEditor
@export var LoadContainer :VBoxContainer
@export var LoadButton :Button
@export var DeleteButton :Button

@onready var StructureLoad = preload("res://src/structure_editor/structure_button.tscn")

var selected_id : String = ""

func _ready() -> void:
	structureEditor.open_load.connect(open)
	_update_buttons()

func open() -> void:
	visible = true
	selected_id = ""
	_update_buttons()
	
	for child in LoadContainer.get_children():
		child.queue_free()
		
	var structures = StructureDb.get_ids()
	for id in structures:
		var instance = StructureLoad.instantiate()
		LoadContainer.add_child(instance)
		
		instance.text = id
		
		instance.pressed.connect(_on_structure_selected.bind(id))

func _on_structure_selected(id: String) -> void:
	selected_id = id
	_update_buttons()

func _update_buttons() -> void:
	var has_selection = selected_id != ""
	LoadButton.disabled = !has_selection
	DeleteButton.disabled = !has_selection

func _on_load_pressed() -> void:
	if selected_id == "": return
	
	var blueprint = StructureDb.get_blueprint(selected_id)
	var palette = StructureDb.get_palette(selected_id)
	
	if blueprint == "" or palette.is_empty():
		printerr("Failed to load raw data for: ", selected_id)
		return
		
	Editor.import_from_rle(blueprint, palette)
	Editor.update_visuals(Vector2i(0, 0))
	visible = false

func _on_delete_pressed() -> void:
	pass

func _on_close_pressed() -> void:
	visible = false

func _on_close_requested() -> void:
	visible = false
