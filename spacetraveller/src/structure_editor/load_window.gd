extends Window

@export var structureEditor :Node2D
@export var Editor :StructureEditor
@export var LoadContainer :ButtonListContainer
@export var LoadButton :Button
@export var DeleteButton :Button
var FastTilemap :FastTileMap

var selectedID : String = ""

func _ready() -> void:
	structureEditor.open_load.connect(open)
	LoadContainer.item_selected.connect(_on_structure_selected_idx)
	LoadContainer.item_activated.connect(_on_structure_activated)
	
	InputManager.ui_directional_input.connect(_on_directional_input)
	InputManager.ui_accept.connect(_on_accept_input)
	InputManager.ui_delete.connect(_on_delete_pressed_signal)
	
	_update_buttons()

func _on_delete_pressed_signal() -> void:
	if not visible: return
	_on_delete_pressed()

func _on_directional_input(direction: Vector2) -> void:
	if not visible: return
	LoadContainer.handle_directional_input(direction)

func _on_accept_input() -> void:
	if not visible: return
	_on_load_pressed()

func _on_close_requested_signal() -> void:
	if not visible: return
	_on_close_pressed()

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
			_on_structure_selected_idx(0, data)
		else:
			_update_buttons()

func _on_structure_selected_idx(_idx: int, id: Variant) -> void:
	selectedID = str(id)
	_update_buttons()

func _on_structure_activated(_idx: int, id: Variant) -> void:
	selectedID = str(id)
	_on_load_pressed()

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
	if FastTilemap.has_method("update_world_bubble"):
		FastTilemap.update_world_bubble(structureEditor.playerOffset)
	else:
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
