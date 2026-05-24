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

func _ready() -> void:
	structureEditor.open_load.connect(open)
	
	# Initialize ListActionsUI logic
	list_actions = ListActionsUI.new()
	list_actions.list_container = LoadContainer
	list_actions.action_buttons = [LoadButton]
	list_actions.delete_button = DeleteButton
	add_child(list_actions)
	
	list_actions.action_triggered.connect(func(_data): _on_load_pressed())
	list_actions.delete_requested.connect(func(_data): _on_delete_pressed())
	list_actions.selection_changed.connect(_on_structure_selected)
	
	_update_buttons()

func _on_structure_selected(id: Variant) -> void:
	selectedID = str(id) if id != null else ""
	_update_buttons()

func open() -> void:
	call_deferred("set_visible", true)
	selectedID = ""
	InputManager.push_mode(InputManager.InputMode.MENU)
	
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
	
	var blueprint = StructureDb.get_blueprint(selectedID)
	var palette = StructureDb.get_palette(selectedID)
	
	if blueprint == "" or palette.is_empty():
		printerr("Failed to load raw data for: ", selectedID)
		return
		
	structureEditor.save_undo_state()
	Editor.import_from_rle(blueprint, palette, structureEditor.selectedChunkPos)
	if World:
		World.update_world_bubble(structureEditor.playerOffset)
	elif FastTilemap:
		FastTilemap.update_visuals(structureEditor.playerOffset)
	visible = false
	InputManager.pop_mode()

func _on_delete_pressed() -> void:
	if selectedID == "": return
	DbAccess.delete_structure(selectedID)
	open()

func _on_close_pressed() -> void:
	visible = false
	InputManager.pop_mode()

func _on_close_requested() -> void:
	visible = false
	InputManager.pop_mode()
