extends Window

@export var structureEditor :Node2D
@export var Editor :StructureEditor
@export var LoadContainer :VBoxContainer
@export var LoadButton :Button
@export var DeleteButton :Button

@onready var StructureLoad = preload("res://src/structure_editor/structure_button.tscn")

var selectedID : String = ""

func _ready() -> void:
	structureEditor.open_load.connect(open)
	_update_buttons()

func open() -> void:
	visible = true
	selectedID = ""
	_update_buttons()
	
	for child in LoadContainer.get_children():
		child.queue_free()
		
	var structures = StructureDb.get_ids()
	for id in structures:
		var instance = StructureLoad.instantiate()
		LoadContainer.add_child(instance)
		
		instance.text = id
		instance.pressed.connect(_on_structure_selected.bind(id))
		
		instance.gui_input.connect(func(event):
			if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT and event.double_click:
				_on_structure_selected(id)
				_on_load_pressed()
		)

func _on_structure_selected(id: String) -> void:
	selectedID = id
	_update_buttons()

func _update_buttons() -> void:
	var hasSelection = selectedID != ""
	LoadButton.disabled = !hasSelection
	DeleteButton.disabled = !hasSelection

func _on_load_pressed() -> void:
	if selectedID == "": return
	
	var blueprint = StructureDb.get_blueprint(selectedID)
	var palette = StructureDb.get_palette(selectedID)
	
	if blueprint == "" or palette.is_empty():
		printerr("Failed to load raw data for: ", selectedID)
		return
		
	structureEditor.save_undo_state()
	Editor.import_from_rle(blueprint, palette)
	Editor.update_visuals(Vector2i(0, 0))
	visible = false

func _on_delete_pressed() -> void:
	if selectedID == "": return
	DbAccess.delete_structure(selectedID)
	open()

func _on_close_pressed() -> void:
	visible = false

func _on_close_requested() -> void:
	visible = false
