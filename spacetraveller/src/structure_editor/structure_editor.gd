extends Node2D

signal open_save
signal open_load

@onready var ToolOption :PackedScene = preload("res://src/structure_editor/tool_option.tscn")
@export var Editor :StructureEditor
@export var TileIDLabel1 :Label
@export var TileIDLabel2 :Label
@export var TileGrid :FlowContainer
@export var ItemGrid :FlowContainer
@export var NpcGrid :FlowContainer
@export var LootGrid :FlowContainer
@export var EntityGroupGrid :FlowContainer
@export var ContentTabs :TabContainer
@export var ItemAmountInput :SpinBox
@export var NpcJobContainer :HBoxContainer
@export var NpcJobText :LineEdit
@export var NpcJobPopup :MenuButton
@export var NpcDialogueContainer :HBoxContainer
@export var NpcDialogueText :LineEdit
@export var NpcDialoguePopup :MenuButton
@export var SelectionVisual :Line2D
@export var editMenu :PopupMenu
@export var chunkMenu :PopupMenu
@export var ToolOptions :HBoxContainer
@export var LoadWindow :Window
@export var CoordsLabel :Label
@export var ChunkVisual :Line2D
@export var ZLevelLabel :Label
var World :GameWorld
var FastTilemap :FastTileMap
var spacing :int = 0

var CHUNK_SIZE = GameWorld.get_chunk_size()
var BUBBLE_SIZE :int = CHUNK_SIZE
var REGION_SIZE = GameWorld.get_region_size()
var TILE_SIZE = FastTileMap.get_tile_size()

var tools = {}
var active_tool
var active_selection : Rect2i = Rect2i()

var selectedChunkPos : Vector2i
var isMovingChunk : bool = false

var tileID1 :String
var tileID2 :String
var tileType1 :String = "tile"
var tileType2 :String = "tile"
var active_content_type :String = "tile"
var primary_ids_by_content_type :Dictionary = {}
var lastMousePos :Vector2i
var mousePos :Vector2i
var playerOffset :Vector2
var active_z :int = 0
var levels :Dictionary = {}
var structure_size :Vector2i = Vector2i(24, 24)
var current_structure_id :String = ""
var current_structure_type :String = "building"
var current_structure_path :String = "res://data/structures/structures.json"
var current_dungeon_room_entrances :Array = ["north", "east", "south", "west"]
var current_structure_entrances :Array = []
var editor_npc_entities :Dictionary = {}
var editor_live_item_rules :Dictionary = {}
var editor_npc_rules_by_level :Dictionary = {}
var editor_item_rules_by_level :Dictionary = {}
var editor_loot_table_rules_by_level :Dictionary = {}
var editor_loot_table_sprites :Dictionary = {}
var editor_entity_group_rules_by_level :Dictionary = {}
var editor_entity_group_sprites :Dictionary = {}
var editor_other_rules_by_level :Dictionary = {}

var undo_stack : Array = []
var redo_stack : Array = []
const MAX_UNDOS = 100
const LOOT_TABLE_ATLAS := Vector2i(71, 18)
const ENTITY_GROUP_ATLAS := Vector2i(72, 18)
const CONTENT_TYPES := {
	"tile": {
		"tab": "Tiles",
		"key": "tile",
		"erase_id": "void",
		"erase_type": "tile",
		"erase_label": "void"
	},
	"item": {
		"tab": "Items",
		"key": "items",
		"erase_id": "",
		"erase_type": "item_erase",
		"erase_label": "erase items",
		"erase_id_label_prefix": "erase item: "
	},
	"npc": {
		"tab": "NPCs",
		"key": "npc",
		"erase_id": "",
		"erase_type": "npc_erase",
		"erase_label": "erase NPC"
	},
	"loot_table": {
		"tab": "Loot",
		"key": "loot_table",
		"erase_id": "",
		"erase_type": "loot_table_erase",
		"erase_label": "erase loot table"
	},
	"entity_group": {
		"tab": "Entity Groups",
		"key": "entity_group",
		"erase_id": "",
		"erase_type": "entity_group_erase",
		"erase_label": "erase entity group"
	}
}
const CONTENT_CAPTURE_ORDER := ["tile", "npc", "item", "loot_table", "entity_group"]
const CONTENT_PICK_ORDER := ["npc", "item", "loot_table", "entity_group", "tile"]

func update_editor_visuals():
	if World:
		World.update_world_bubble_at_z(playerOffset, active_z, false)
	elif FastTilemap:
		FastTilemap.update_visuals(playerOffset, playerOffset)
	_refresh_loot_table_markers()
	_refresh_entity_group_markers()

func start_editor(offset :Vector2 = Vector2.ZERO) -> void:
	InputManager.structure_mode_changed.connect(_on_mode_changed)
	InputManager.structure_mouse_input.connect(_on_mouse_input)
	InputManager.structure_key_input.connect(_on_key_input)
	
	playerOffset = offset

	LoadWindow.World = World
	LoadWindow.FastTilemap = FastTilemap
	if World:
		Editor.set_world(World)
		World.set_active_z(active_z)
	Editor.set_tilemap(FastTilemap)
	BUBBLE_SIZE = FastTilemap.get_world_bubble_size()
	
	setup_tools()
	if ContentTabs and !ContentTabs.tab_changed.is_connected(_on_content_tab_changed):
		ContentTabs.tab_changed.connect(_on_content_tab_changed)
	
	select_entry("road_bricks", "tile")
	set_secondary_to_group_remover(active_content_type)
	if ContentTabs:
		_on_content_tab_changed(ContentTabs.current_tab)
	
	_on_mode_changed("pencil")
	
	selectedChunkPos = Vector2i(floor(offset.x / CHUNK_SIZE), floor(offset.y / CHUNK_SIZE)) * CHUNK_SIZE
	if !chunkMenu.index_pressed.is_connected(_on_chunk_index_pressed):
		chunkMenu.index_pressed.connect(_on_chunk_index_pressed)
	_update_chunk_visual()
	_update_z_label()
	
	TileGrid.start(spacing, TileDb)
	ItemGrid.start(spacing, ItemDb)
	NpcGrid.start(spacing, RaceDb)
	LootGrid.start(spacing, LootDb)
	EntityGroupGrid.start(spacing, EntityGroupDb)
	_setup_npc_option_menus()
	_update_npc_options_for_race("")
	
	TileGrid.selection_changed.connect(func(id, is_primary): _on_grid_selection_changed(id, "tile", is_primary))
	ItemGrid.selection_changed.connect(func(id, is_primary): _on_grid_selection_changed(id, "item", is_primary))
	NpcGrid.selection_changed.connect(func(id, is_primary): _on_grid_selection_changed(id, "npc", is_primary))
	LootGrid.selection_changed.connect(func(id, is_primary): _on_grid_selection_changed(id, "loot_table", is_primary))
	EntityGroupGrid.selection_changed.connect(func(id, is_primary): _on_grid_selection_changed(id, "entity_group", is_primary))
	
	update_editor_visuals()

func _exit_tree() -> void:
	Input.set_custom_mouse_cursor(null)

func setup_tools():
	tools = {
		"pencil": EditorTools.PencilTool.new(self),
		"line": EditorTools.LineTool.new(self),
		"rectangle": EditorTools.ShapeTool.new(self, StructureEditor.SHAPE_RECTANGLE),
		"ellipsis": EditorTools.ShapeTool.new(self, StructureEditor.SHAPE_ELLIPSIS),
		"eyedropper": EditorTools.EyedropperTool.new(self),
		"fill": EditorTools.FillTool.new(self),
		"selection": EditorTools.SelectionTool.new(self)
	}
	active_tool = tools["pencil"]
	
	for t in tools.keys():
		editMenu.add_item(t.capitalize())
	if !editMenu.index_pressed.is_connected(_on_edit_index_pressed):
		editMenu.index_pressed.connect(_on_edit_index_pressed)

func _process(_delta: float) -> void:
	mousePos = get_mouse_tile_pos()
	
	if mousePos != lastMousePos:
		on_tile_changed(mousePos)
		lastMousePos = mousePos
	
	var selection_size = Vector2i.ZERO
	if active_tool and active_tool is EditorTools.SelectionTool:
		selection_size = active_tool.selection_rect.size
	var global_pos = mousePos + Vector2i(playerOffset)
	CoordsLabel.update_text(global_pos, selection_size)

	if isMovingChunk:
		var abs_mouse = mousePos + Vector2i(playerOffset)
		var new_chunk_pos = Vector2i(floor(float(abs_mouse.x) / CHUNK_SIZE), floor(float(abs_mouse.y) / CHUNK_SIZE)) * CHUNK_SIZE
		if new_chunk_pos != selectedChunkPos:
			selectedChunkPos = new_chunk_pos
			_update_chunk_visual()

func _on_mode_changed(m :String):
	if tools.has(m):
		isMovingChunk = false
		if active_tool:
			active_tool.on_deactivate()
		active_tool = tools[m]
		active_tool.on_hover(mousePos)
		_update_tool_options()
		
		var cursor = active_tool.get_cursor_config()
		Input.set_custom_mouse_cursor(cursor.tex, Input.CURSOR_ARROW, cursor.hot)

func _update_tool_options():
	for child in ToolOptions.get_children():
		child.queue_free()
		
	for config in active_tool.get_options_config():
		var opt = ToolOption.instantiate()
		ToolOptions.add_child(opt)
		
		var label = opt.get_node("Name")
		var button = opt.get_node("Button")
		
		label.text = config.label
		
		button.button_pressed = active_tool.options[config.name]
		button.toggled.connect(func(pressed): 
			active_tool.options[config.name] = pressed
		)

func _on_mouse_input(button: String, action: InputManager.MouseAction):
	if isMovingChunk:
		if action == InputManager.MouseAction.PRESS and button == "left":
			isMovingChunk = false
			_update_chunk_visual()
			_on_mode_changed("pencil")
		return
	
	match action:
		InputManager.MouseAction.PRESS:
			active_tool.on_press(button, mousePos)
		InputManager.MouseAction.RELEASE:
			active_tool.on_release(button, mousePos)
		InputManager.MouseAction.DRAG:
			active_tool.on_drag(button, mousePos)

func _on_key_input(key: String):
	match key:
		"undo":
			if active_tool and active_tool.has_method("on_key"):
				active_tool.on_key(key)
			undo()
			return
		"redo":
			if active_tool and active_tool.has_method("on_key"):
				active_tool.on_key(key)
			redo()
			return
		"ascend_level":
			change_z(1)
			return
		"descent_level":
			change_z(-1)
			return
	
	if active_tool and active_tool.has_method("on_key"):
		active_tool.on_key(key)

func _capture_editor_state() -> Dictionary:
	return {
		"tiles": World.get_tile_id_cache() if World else {},
		"npc_rules_by_level": editor_npc_rules_by_level.duplicate(true),
		"item_rules_by_level": editor_item_rules_by_level.duplicate(true),
		"loot_table_rules_by_level": editor_loot_table_rules_by_level.duplicate(true),
		"entity_group_rules_by_level": editor_entity_group_rules_by_level.duplicate(true),
		"other_rules_by_level": editor_other_rules_by_level.duplicate(true)
	}

func _restore_editor_state(state: Dictionary) -> void:
	if state.is_empty() or !World:
		return
	clear_editor_npcs()
	clear_editor_live_items()
	World.set_tile_id_cache(state.get("tiles", {}))
	editor_npc_rules_by_level = state.get("npc_rules_by_level", {}).duplicate(true)
	editor_item_rules_by_level = state.get("item_rules_by_level", {}).duplicate(true)
	editor_loot_table_rules_by_level = state.get("loot_table_rules_by_level", {}).duplicate(true)
	editor_entity_group_rules_by_level = state.get("entity_group_rules_by_level", {}).duplicate(true)
	editor_other_rules_by_level = state.get("other_rules_by_level", {}).duplicate(true)
	_apply_editor_rules_for_level(active_z)
	update_editor_visuals()

func save_undo_state():
	undo_stack.push_back(_capture_editor_state())
	if undo_stack.size() > MAX_UNDOS:
		undo_stack.pop_front()
	redo_stack.clear()

func undo():
	if undo_stack.is_empty(): return
	
	redo_stack.push_back(_capture_editor_state())
	var state = undo_stack.pop_back()
	_restore_editor_state(state)

func redo():
	if redo_stack.is_empty(): return
	
	undo_stack.push_back(_capture_editor_state())
	var state = redo_stack.pop_back()
	_restore_editor_state(state)

func _is_content_type(type: String) -> bool:
	return CONTENT_TYPES.has(type)

func _is_erase_type(type: String) -> bool:
	return !_content_type_for_erase_type(type).is_empty()

func _content_type_config(type: String) -> Dictionary:
	return CONTENT_TYPES.get(type, {})

func _content_type_for_erase_type(type: String) -> String:
	if type == "tile":
		return ""
	for content_type in CONTENT_TYPES.keys():
		var config: Dictionary = CONTENT_TYPES[content_type]
		if str(config.get("erase_type", "")) == type:
			return str(content_type)
	return ""

func _content_type_for_tab(tab: int) -> String:
	if !ContentTabs or tab < 0 or tab >= ContentTabs.get_child_count():
		return active_content_type
	var tab_name := str(ContentTabs.get_child(tab).name)
	for content_type in CONTENT_TYPES.keys():
		var config: Dictionary = CONTENT_TYPES[content_type]
		if str(config.get("tab", "")) == tab_name:
			return str(content_type)
	return active_content_type

func _on_content_tab_changed(tab: int) -> void:
	set_active_content_type(_content_type_for_tab(tab))

func _on_grid_selection_changed(id: String, type: String, is_primary: bool) -> void:
	if is_primary:
		select_entry(id, type, true)
	else:
		set_active_content_type(type)

func set_active_content_type(type: String, restore_primary: bool = true) -> void:
	if !_is_content_type(type):
		return
	active_content_type = type
	tileType1 = type
	if restore_primary:
		tileID1 = str(primary_ids_by_content_type.get(type, ""))
	if type == "npc":
		_update_npc_options_for_race(tileID1)
	set_secondary_to_group_remover(type)
	_update_selection_labels()
	if active_tool:
		active_tool.on_hover(mousePos)

func set_secondary_to_group_remover(type: String) -> void:
	var config := _content_type_config(type)
	if config.is_empty():
		config = _content_type_config("tile")
	tileID2 = str(config.get("erase_id", "void"))
	tileType2 = str(config.get("erase_type", "tile"))

func _label_for_entry(id: String, type: String) -> String:
	var content_type := _content_type_for_erase_type(type)
	if !content_type.is_empty():
		var config := _content_type_config(content_type)
		if id.is_empty():
			return str(config.get("erase_label", "erase"))
		return str(config.get("erase_id_label_prefix", str(config.get("erase_label", "erase")) + ": ")) + id
	return id

func _update_selection_labels() -> void:
	TileIDLabel1.text = "ID1: " + _label_for_entry(tileID1, tileType1)
	TileIDLabel2.text = "ID2: " + _label_for_entry(tileID2, tileType2)

func get_tool_entry_for_button(button: String) -> Dictionary:
	if button == "right":
		return { "id": tileID2, "type": tileType2 }
	return { "id": tileID1, "type": tileType1 }

func select_entry(id: String, type: String = "tile", is_primary: bool = true):
	if !is_primary:
		set_active_content_type(type)
		return
	if !_is_content_type(type):
		return
	active_content_type = type
	tileID1 = id
	tileType1 = type
	primary_ids_by_content_type[type] = id
	if type == "npc":
		_update_npc_options_for_race(id)
	set_secondary_to_group_remover(type)
	_update_selection_labels()

func on_tile_changed(_pos: Vector2i):
	if active_tool:
		active_tool.on_hover(_pos)

var item_amount: int:
	get: return int(ItemAmountInput.value) if ItemAmountInput else 1

func _setup_npc_option_menus() -> void:
	_populate_menu(NpcJobPopup, JobDb.get_ids(), _on_npc_job_index_pressed)
	if NpcJobText and !NpcJobText.text_changed.is_connected(_on_npc_job_text_changed):
		NpcJobText.text_changed.connect(_on_npc_job_text_changed)
	_refresh_npc_dialogue_menu()

func _populate_menu(button: MenuButton, values: Array, callback: Callable) -> void:
	if !button:
		return
	var popup := button.get_popup()
	popup.clear()
	for value in values:
		var text := str(value).strip_edges()
		if !text.is_empty():
			popup.add_item(text)
	if !popup.index_pressed.is_connected(callback):
		popup.index_pressed.connect(callback)

func _current_npc_job() -> String:
	return NpcJobText.text.strip_edges().to_lower() if NpcJobText else ""

func _get_dialogue_suggestions_for_job(job: String) -> Array:
	var seen := {}
	var result: Array = []
	if job.is_empty():
		return result
	for dialogue_variant in JobDb.get_dialogues(job):
		var dialogue_id := str(dialogue_variant).strip_edges()
		if dialogue_id.is_empty() or seen.has(dialogue_id):
			continue
		seen[dialogue_id] = true
		result.append(dialogue_id)
	return result

func _job_allows_dialogue(job: String, dialogue_id: String) -> bool:
	if dialogue_id.is_empty():
		return true
	return _get_dialogue_suggestions_for_job(job).has(dialogue_id)

func _selected_dialogue_id_for_job(job: String) -> String:
	var dialogue_id := NpcDialogueText.text.strip_edges() if NpcDialogueText else ""
	if dialogue_id.is_empty() or !_job_allows_dialogue(job, dialogue_id):
		return ""
	return dialogue_id

func _refresh_npc_dialogue_menu() -> void:
	var job := _current_npc_job()
	var dialogue_ids := _get_dialogue_suggestions_for_job(job)
	_populate_menu(NpcDialoguePopup, dialogue_ids, _on_npc_dialogue_index_pressed)
	if NpcDialogueText:
		var current := NpcDialogueText.text.strip_edges()
		if !current.is_empty() and !dialogue_ids.has(current):
			NpcDialogueText.text = ""

func _on_npc_job_index_pressed(index: int) -> void:
	var job := _menu_item_text(NpcJobPopup, index)
	if job.is_empty():
		return
	NpcJobText.text = job
	_refresh_npc_dialogue_menu()

func _on_npc_job_text_changed(_new_text: String) -> void:
	_refresh_npc_dialogue_menu()

func _on_npc_dialogue_index_pressed(index: int) -> void:
	var dialogue_id := _menu_item_text(NpcDialoguePopup, index)
	if dialogue_id.is_empty():
		return
	NpcDialogueText.text = dialogue_id

func _menu_item_text(button: MenuButton, index: int) -> String:
	if !button:
		return ""
	var popup := button.get_popup()
	if index < 0 or index >= popup.get_item_count():
		return ""
	return popup.get_item_text(index)

func _selected_npc_is_sapient() -> bool:
	return tileType1 == "npc" and _is_sapient_race(tileID1)

func _is_sapient_race(race_id: String) -> bool:
	return !race_id.is_empty() and RaceDb.has_tag(race_id, "SAPIENT")

func _update_npc_options_for_race(race_id: String) -> void:
	var sapient := _is_sapient_race(race_id)
	if NpcJobContainer:
		NpcJobContainer.visible = true
	if NpcDialogueContainer:
		NpcDialogueContainer.visible = sapient
	if NpcJobText:
		var job := NpcJobText.text.strip_edges().to_lower()
		if sapient:
			if job.is_empty() or job == "monster" or job == "animal":
				NpcJobText.text = "scavenger"
		elif job.is_empty() or (job != "monster" and job != "animal"):
			NpcJobText.text = "monster"
	if !sapient:
		if NpcDialogueText:
			NpcDialogueText.text = ""
	_refresh_npc_dialogue_menu()

func _npc_job_for_race(race_id: String) -> String:
	var job := NpcJobText.text.strip_edges().to_lower() if NpcJobText else ""
	if job.is_empty():
		return _default_job_for_race(race_id)
	return job

func _default_job_for_race(race_id: String) -> String:
	return "scavenger" if _is_sapient_race(race_id) else "monster"

func place_at(pos: Vector2i, id: String, type: String = "tile"):
	if !is_inside_bubble(pos): return
	place_entry(Vector2i(int(pos.x + playerOffset.x), int(pos.y + playerOffset.y)), id, type)
	update_editor_visuals()

func place_entry(world_pos: Vector2i, id: String, type: String = "tile", amount: int = -1):
	if id.is_empty() and !_is_erase_type(type):
		return
	match type:
		"item":
			var place_amount := amount if amount > 0 else item_amount
			World.drop_item(world_pos, id, place_amount)
			_add_item_rule(world_pos, id, place_amount)
		"item_erase":
			if id.is_empty():
				_remove_all_items_at(world_pos)
			else:
				var erase_amount := amount if amount > 0 else item_amount
				World.remove_ground_item(world_pos, id, erase_amount)
				_remove_item_rule(world_pos, id, erase_amount)
		"npc":
			if place_npc(world_pos, id):
				_set_npc_rule(world_pos, id)
		"npc_erase":
			erase_npc(world_pos)
			_remove_npc_rule(world_pos)
		"loot_table":
			_set_loot_table_rule(world_pos, id)
		"loot_table_erase":
			_remove_loot_table_rule(world_pos)
		"entity_group":
			_set_entity_group_rule(world_pos, id)
		"entity_group_erase":
			_remove_entity_group_rule(world_pos)
		_:
			World.place_tile(world_pos.x, world_pos.y, id)

func place_npc(world_pos: Vector2i, race_id: String) -> bool:
	if !World or race_id.is_empty():
		return false
	erase_npc(world_pos)
	if World.has_entity_at_cell(world_pos.x, world_pos.y):
		return false
	var entity_id: int = World.spawn_entity(world_pos.x, world_pos.y, race_id)
	if entity_id >= 0 and entity_id != 4294967295:
		editor_npc_entities[_npc_key(world_pos)] = entity_id
		return true
	return false

func erase_npc(world_pos: Vector2i) -> void:
	var key := _npc_key(world_pos)
	if !World or !editor_npc_entities.has(key):
		return
	World.despawn_entity(editor_npc_entities[key])
	editor_npc_entities.erase(key)

func clear_editor_npcs() -> void:
	if !World:
		editor_npc_entities.clear()
		return
	for key in editor_npc_entities.keys():
		World.despawn_entity(editor_npc_entities[key])
	editor_npc_entities.clear()

func _npc_key(world_pos: Vector2i) -> Vector3i:
	return Vector3i(world_pos.x, world_pos.y, active_z)

func clear_editor_live_items() -> void:
	if !World:
		editor_live_item_rules.clear()
		return
	for key in editor_live_item_rules.keys():
		var rule: Dictionary = editor_live_item_rules[key]
		var pos := _world_pos_from_local(_rule_pos_from_variant(rule.get("pos", [])))
		World.remove_ground_item(pos, str(rule.get("item_id", "")), int(rule.get("amount", 0)))
	editor_live_item_rules.clear()

func _active_level_key() -> String:
	return _level_key(active_z)

func _local_pos_from_world(world_pos: Vector2i) -> Vector2i:
	return world_pos - selectedChunkPos

func _world_pos_from_local(local_pos: Vector2i) -> Vector2i:
	return selectedChunkPos + local_pos

func _pos_key(pos: Vector2i) -> String:
	return "%d,%d" % [pos.x, pos.y]

func _item_rule_key(pos: Vector2i, item_id: String) -> String:
	return _pos_key(pos) + ":" + item_id

func _rule_pos_array(pos: Vector2i) -> Array:
	return [pos.x, pos.y]

func _get_level_rule_dict(store: Dictionary, key: String) -> Dictionary:
	if !store.has(key) or !(store[key] is Dictionary):
		store[key] = {}
	return store[key]

func _set_npc_rule(world_pos: Vector2i, race_id: String) -> void:
	var local_pos := _local_pos_from_world(world_pos)
	var rules := _get_level_rule_dict(editor_npc_rules_by_level, _active_level_key())
	var job := _npc_job_for_race(race_id)
	var rule := {
		"type": "spawn_entity",
		"entity": race_id,
		"pos": _rule_pos_array(local_pos),
		"job": job
	}
	if _is_sapient_race(race_id):
		var dialogue_id := _selected_dialogue_id_for_job(job)
		if !dialogue_id.is_empty():
			rule["dialogue_id"] = dialogue_id
	rules[_pos_key(local_pos)] = rule

func _remove_npc_rule(world_pos: Vector2i) -> void:
	var key := _active_level_key()
	if !editor_npc_rules_by_level.has(key):
		return
	var local_pos := _local_pos_from_world(world_pos)
	var rules: Dictionary = editor_npc_rules_by_level[key]
	rules.erase(_pos_key(local_pos))

func _add_item_rule(world_pos: Vector2i, item_id: String, amount: int) -> void:
	if amount <= 0:
		return
	var local_pos := _local_pos_from_world(world_pos)
	var level_key := _active_level_key()
	var rules := _get_level_rule_dict(editor_item_rules_by_level, level_key)
	var key := _item_rule_key(local_pos, item_id)
	var rule: Dictionary = rules.get(key, {
		"type": "spawn_item",
		"item_id": item_id,
		"amount": 0,
		"pos": _rule_pos_array(local_pos)
	})
	rule["amount"] = int(rule.get("amount", 0)) + amount
	rules[key] = rule
	editor_live_item_rules[key] = rule.duplicate(true)

func _remove_item_rule(world_pos: Vector2i, item_id: String, amount: int) -> void:
	if amount <= 0:
		return
	var level_key := _active_level_key()
	if !editor_item_rules_by_level.has(level_key):
		return
	var local_pos := _local_pos_from_world(world_pos)
	var key := _item_rule_key(local_pos, item_id)
	var rules: Dictionary = editor_item_rules_by_level[level_key]
	if !rules.has(key):
		return
	var rule: Dictionary = rules[key]
	var remaining := int(rule.get("amount", 0)) - amount
	if remaining > 0:
		rule["amount"] = remaining
		rules[key] = rule
		editor_live_item_rules[key] = rule.duplicate(true)
	else:
		rules.erase(key)
		editor_live_item_rules.erase(key)

func _remove_all_items_at(world_pos: Vector2i) -> void:
	if !World:
		return
	var local_pos := _local_pos_from_world(world_pos)
	var level_key := _active_level_key()
	var item_rule_keys := _item_rule_keys_at(local_pos)
	if editor_item_rules_by_level.has(level_key):
		var item_rules: Dictionary = editor_item_rules_by_level[level_key]
		for item_key in item_rule_keys:
			if !item_rules.has(item_key):
				continue
			var rule: Dictionary = item_rules[item_key]
			World.remove_ground_item(world_pos, str(rule.get("item_id", "")), int(rule.get("amount", 0)))
			item_rules.erase(item_key)
			editor_live_item_rules.erase(item_key)
	if World.has_item(world_pos):
		for item_variant in World.get_items_at(world_pos):
			if !(item_variant is Dictionary):
				continue
			var live_item: Dictionary = item_variant
			World.remove_ground_item(world_pos, str(live_item.get("id", "")), int(live_item.get("amount", 0)))

func _set_loot_table_rule(world_pos: Vector2i, loot_table_id: String) -> void:
	if loot_table_id.is_empty():
		return
	var local_pos := _local_pos_from_world(world_pos)
	var rules := _get_level_rule_dict(editor_loot_table_rules_by_level, _active_level_key())
	rules[_pos_key(local_pos)] = {
		"type": "spawn_loot_table",
		"loot_table": loot_table_id,
		"pos": _rule_pos_array(local_pos)
	}
	_refresh_loot_table_markers()

func _remove_loot_table_rule(world_pos: Vector2i) -> void:
	var key := _active_level_key()
	if !editor_loot_table_rules_by_level.has(key):
		return
	var local_pos := _local_pos_from_world(world_pos)
	var rules: Dictionary = editor_loot_table_rules_by_level[key]
	rules.erase(_pos_key(local_pos))
	_refresh_loot_table_markers()

func _set_entity_group_rule(world_pos: Vector2i, entity_group_id: String) -> void:
	if entity_group_id.is_empty():
		return
	var local_pos := _local_pos_from_world(world_pos)
	var rules := _get_level_rule_dict(editor_entity_group_rules_by_level, _active_level_key())
	rules[_pos_key(local_pos)] = {
		"type": "spawn_entity_group",
		"entity_group": entity_group_id,
		"pos": _rule_pos_array(local_pos)
	}
	_refresh_entity_group_markers()

func _remove_entity_group_rule(world_pos: Vector2i) -> void:
	var key := _active_level_key()
	if !editor_entity_group_rules_by_level.has(key):
		return
	var local_pos := _local_pos_from_world(world_pos)
	var rules: Dictionary = editor_entity_group_rules_by_level[key]
	rules.erase(_pos_key(local_pos))
	_refresh_entity_group_markers()

func _world_pos_from_editor_pos(editor_pos: Vector2i) -> Vector2i:
	return Vector2i(int(editor_pos.x + playerOffset.x), int(editor_pos.y + playerOffset.y))

func _item_rule_keys_at(local_pos: Vector2i) -> Array:
	var result: Array = []
	var level_key := _active_level_key()
	var rules: Dictionary = editor_item_rules_by_level.get(level_key, {})
	var local_key := _pos_key(local_pos) + ":"
	for rule_key in rules.keys():
		if str(rule_key).begins_with(local_key):
			result.append(rule_key)
	result.sort()
	return result

func _content_key(type: String) -> String:
	return str(_content_type_config(type).get("key", ""))

func _single_rule_store_for_content_type(type: String) -> Dictionary:
	match type:
		"npc":
			return editor_npc_rules_by_level
		"loot_table":
			return editor_loot_table_rules_by_level
		"entity_group":
			return editor_entity_group_rules_by_level
	return {}

func _content_id_from_rule(type: String, rule: Dictionary) -> String:
	match type:
		"npc":
			return str(rule.get("entity", rule.get("race_id", ""))).strip_edges()
		"item":
			return str(rule.get("item_id", "")).strip_edges()
		"loot_table":
			return str(rule.get("loot_table", "")).strip_edges()
		"entity_group":
			return str(rule.get("entity_group", rule.get("entity_group_id", ""))).strip_edges()
	return ""

func _get_item_contents_at(world_pos: Vector2i, local_pos: Vector2i, level_key: String) -> Array:
	var item_rules: Dictionary = editor_item_rules_by_level.get(level_key, {})
	var items: Array = []
	for item_key in _item_rule_keys_at(local_pos):
		if item_rules.has(item_key):
			items.append(item_rules[item_key].duplicate(true))
	if items.is_empty() and World.has_item(world_pos):
		for item_variant in World.get_items_at(world_pos):
			if !(item_variant is Dictionary):
				continue
			var live_item: Dictionary = item_variant
			var item_id := str(live_item.get("id", "")).strip_edges()
			var amount := int(live_item.get("amount", 0))
			if item_id.is_empty() or amount <= 0:
				continue
			items.append({
				"type": "spawn_item",
				"item_id": item_id,
				"amount": amount,
				"pos": _rule_pos_array(local_pos)
			})
	return items

func _content_at_type(type: String, world_pos: Vector2i, local_pos: Vector2i, level_key: String, pos_key: String) -> Variant:
	match type:
		"tile":
			var tile_id := str(World.get_tile_at(world_pos.x, world_pos.y))
			if !tile_id.is_empty() and tile_id != "void":
				return tile_id
		"item":
			var items := _get_item_contents_at(world_pos, local_pos, level_key)
			if !items.is_empty():
				return items
		_:
			var store := _single_rule_store_for_content_type(type)
			var rules: Dictionary = store.get(level_key, {})
			if rules.has(pos_key):
				return rules[pos_key].duplicate(true)
	return null

func _content_value_from_contents(contents: Dictionary, type: String) -> Variant:
	var key := _content_key(type)
	if key.is_empty() or !contents.has(key):
		return null
	return contents[key]

func _apply_selected_content_options(type: String, rule: Dictionary, is_primary: bool) -> void:
	if !is_primary:
		return
	if type == "npc":
		if NpcJobText:
			var job := str(rule.get("job", "")).strip_edges()
			if !job.is_empty():
				NpcJobText.text = job
		_refresh_npc_dialogue_menu()
		if NpcDialogueText:
			NpcDialogueText.text = str(rule.get("dialogue_id", "")).strip_edges()
			_refresh_npc_dialogue_menu()
	elif type == "item" and ItemAmountInput:
		ItemAmountInput.value = max(1, int(rule.get("amount", 1)))

func _select_content_value(type: String, value: Variant, is_primary: bool) -> bool:
	if type == "tile":
		var tile_id := str(value).strip_edges()
		if tile_id.is_empty():
			return false
		select_entry(tile_id, "tile", is_primary)
		return true
	if type == "item":
		if !(value is Array):
			return false
		var item_values: Array = value
		if item_values.is_empty() or !(item_values[0] is Dictionary):
			return false
		var item_rule: Dictionary = item_values[0]
		var item_id := _content_id_from_rule(type, item_rule)
		if item_id.is_empty():
			return false
		select_entry(item_id, type, is_primary)
		_apply_selected_content_options(type, item_rule, is_primary)
		return true
	if !(value is Dictionary):
		return false
	var rule: Dictionary = value
	var id := _content_id_from_rule(type, rule)
	if id.is_empty():
		return false
	select_entry(id, type, is_primary)
	_apply_selected_content_options(type, rule, is_primary)
	return true

func _erase_rule_at(store: Dictionary, world_pos: Vector2i) -> void:
	var level_key := _active_level_key()
	if !store.has(level_key):
		return
	var local_pos := _local_pos_from_world(world_pos)
	var rules: Dictionary = store[level_key]
	rules.erase(_pos_key(local_pos))

func _clear_content_type_at(type: String, world_pos: Vector2i) -> void:
	match type:
		"tile":
			World.place_tile(world_pos.x, world_pos.y, "void")
		"item":
			_remove_all_items_at(world_pos)
		"npc":
			erase_npc(world_pos)
			_erase_rule_at(editor_npc_rules_by_level, world_pos)
		"loot_table":
			_erase_rule_at(editor_loot_table_rules_by_level, world_pos)
		"entity_group":
			_erase_rule_at(editor_entity_group_rules_by_level, world_pos)

func _place_content_value(type: String, world_pos: Vector2i, local_pos: Vector2i, value: Variant) -> void:
	match type:
		"tile":
			World.place_tile(world_pos.x, world_pos.y, str(value))
		"item":
			if !(value is Array):
				return
			var item_values: Array = value
			for item_rule_variant in item_values:
				if !(item_rule_variant is Dictionary):
					continue
				var item_rule: Dictionary = item_rule_variant
				var item_id := _content_id_from_rule(type, item_rule)
				var amount := int(item_rule.get("amount", 0))
				if !item_id.is_empty() and amount > 0:
					place_entry(world_pos, item_id, "item", amount)
		"npc":
			if value is Dictionary:
				var npc_rule: Dictionary = value
				_store_npc_content(local_pos, npc_rule)
		"loot_table":
			if value is Dictionary:
				var loot_table_rule: Dictionary = value
				_store_loot_table_content(local_pos, loot_table_rule)
		"entity_group":
			if value is Dictionary:
				var entity_group_rule: Dictionary = value
				_store_entity_group_content(local_pos, entity_group_rule)

func get_editor_contents_at(editor_pos: Vector2i) -> Dictionary:
	if !World or !is_inside_bubble(editor_pos):
		return {}
	var world_pos := _world_pos_from_editor_pos(editor_pos)
	var local_pos := _local_pos_from_world(world_pos)
	var level_key := _active_level_key()
	var contents := {}
	var pos_key := _pos_key(local_pos)
	for content_type in CONTENT_CAPTURE_ORDER:
		var value: Variant = _content_at_type(str(content_type), world_pos, local_pos, level_key, pos_key)
		if value == null:
			continue
		var key := _content_key(str(content_type))
		if !key.is_empty():
			contents[key] = value

	return contents

func _clear_editor_contents_at(editor_pos: Vector2i) -> void:
	if !World or !is_inside_bubble(editor_pos):
		return
	var world_pos := _world_pos_from_editor_pos(editor_pos)
	for content_type in CONTENT_CAPTURE_ORDER:
		_clear_content_type_at(str(content_type), world_pos)

func capture_editor_contents(rect: Rect2i, clear_map: bool = false) -> Dictionary:
	var contents_by_pos := {}
	for x in range(rect.position.x, rect.end.x):
		for y in range(rect.position.y, rect.end.y):
			var editor_pos := Vector2i(x, y)
			var contents := get_editor_contents_at(editor_pos)
			if contents.is_empty():
				continue
			contents_by_pos[editor_pos - rect.position] = contents
			if clear_map:
				_clear_editor_contents_at(editor_pos)
	if clear_map:
		_refresh_loot_table_markers()
		_refresh_entity_group_markers()
	return contents_by_pos

func _store_npc_content(local_pos: Vector2i, rule: Dictionary) -> void:
	var race_id := str(rule.get("entity", rule.get("race_id", ""))).strip_edges()
	if race_id.is_empty():
		return
	var world_pos := _world_pos_from_local(local_pos)
	if !place_npc(world_pos, race_id):
		return
	var stored_rule := rule.duplicate(true)
	stored_rule["type"] = "spawn_entity"
	stored_rule["entity"] = race_id
	stored_rule["pos"] = _rule_pos_array(local_pos)
	var rules := _get_level_rule_dict(editor_npc_rules_by_level, _active_level_key())
	rules[_pos_key(local_pos)] = stored_rule

func _store_loot_table_content(local_pos: Vector2i, rule: Dictionary) -> void:
	var loot_table_id := str(rule.get("loot_table", "")).strip_edges()
	if loot_table_id.is_empty():
		return
	var stored_rule := rule.duplicate(true)
	stored_rule["type"] = "spawn_loot_table"
	stored_rule["loot_table"] = loot_table_id
	stored_rule["pos"] = _rule_pos_array(local_pos)
	var rules := _get_level_rule_dict(editor_loot_table_rules_by_level, _active_level_key())
	rules[_pos_key(local_pos)] = stored_rule

func _store_entity_group_content(local_pos: Vector2i, rule: Dictionary) -> void:
	var entity_group_id := str(rule.get("entity_group", rule.get("entity_group_id", ""))).strip_edges()
	if entity_group_id.is_empty():
		return
	var stored_rule := rule.duplicate(true)
	stored_rule["type"] = "spawn_entity_group"
	stored_rule["entity_group"] = entity_group_id
	stored_rule["pos"] = _rule_pos_array(local_pos)
	var rules := _get_level_rule_dict(editor_entity_group_rules_by_level, _active_level_key())
	rules[_pos_key(local_pos)] = stored_rule

func place_editor_contents(origin: Vector2i, contents_by_pos: Dictionary) -> void:
	if !World:
		return
	for rel_pos_variant in contents_by_pos.keys():
		if !(rel_pos_variant is Vector2i):
			continue
		var rel_pos: Vector2i = rel_pos_variant
		var editor_pos := origin + rel_pos
		if !is_inside_bubble(editor_pos):
			continue
		var contents_variant: Variant = contents_by_pos[rel_pos_variant]
		if !(contents_variant is Dictionary):
			continue
		var contents: Dictionary = contents_variant
		var world_pos := _world_pos_from_editor_pos(editor_pos)
		var local_pos := _local_pos_from_world(world_pos)
		for content_type in CONTENT_CAPTURE_ORDER:
			var value: Variant = _content_value_from_contents(contents, str(content_type))
			if value != null:
				_place_content_value(str(content_type), world_pos, local_pos, value)
	_refresh_loot_table_markers()
	_refresh_entity_group_markers()

func select_editor_content_at(editor_pos: Vector2i, is_primary: bool = true, type_filter: String = "") -> bool:
	if type_filter == "tile":
		if !World or !is_inside_bubble(editor_pos):
			return false
		var world_pos := _world_pos_from_editor_pos(editor_pos)
		select_entry(str(World.get_tile_at(world_pos.x, world_pos.y)), "tile", is_primary)
		return true

	var contents := get_editor_contents_at(editor_pos)
	if contents.is_empty():
		return false
	if !type_filter.is_empty():
		return _select_content_value(type_filter, _content_value_from_contents(contents, type_filter), is_primary)
	for content_type in CONTENT_PICK_ORDER:
		if _select_content_value(str(content_type), _content_value_from_contents(contents, str(content_type)), is_primary):
			return true
	return false

func clear_editor_loot_table_markers() -> void:
	for marker in editor_loot_table_sprites.values():
		if is_instance_valid(marker):
			marker.queue_free()
	editor_loot_table_sprites.clear()

func _refresh_loot_table_markers() -> void:
	clear_editor_loot_table_markers()
	if !FastTilemap:
		return
	var tilesheet: Texture2D = FastTilemap.get_tilesheet()
	if !tilesheet:
		return
	var rules: Dictionary = editor_loot_table_rules_by_level.get(_active_level_key(), {})
	var cell_size := FastTilemap.get_cell_size()
	for rule_key in rules.keys():
		var rule: Dictionary = rules[rule_key]
		var loot_table_id := str(rule.get("loot_table", ""))
		if loot_table_id.is_empty():
			continue
		var atlas: Vector2i = LOOT_TABLE_ATLAS
		if atlas.x < 0 or atlas.y < 0:
			continue
		var local_pos := _rule_pos_from_variant(rule.get("pos", []))
		var world_pos := _world_pos_from_local(local_pos)
		var atlas_texture := AtlasTexture.new()
		atlas_texture.atlas = tilesheet
		atlas_texture.region = Rect2(
			1 + atlas.x * (TILE_SIZE + 1),
			1 + atlas.y * (TILE_SIZE + 1),
			TILE_SIZE,
			TILE_SIZE
		)
		var marker := Sprite2D.new()
		marker.texture = atlas_texture
		marker.centered = false
		marker.modulate = Color(1, 1, 1, 0.7)
		marker.z_index = 20
		marker.position = (Vector2(world_pos.x, world_pos.y) - playerOffset) * cell_size
		add_child(marker)
		editor_loot_table_sprites[rule_key] = marker

func clear_editor_entity_group_markers() -> void:
	for marker in editor_entity_group_sprites.values():
		if is_instance_valid(marker):
			marker.queue_free()
	editor_entity_group_sprites.clear()

func _refresh_entity_group_markers() -> void:
	clear_editor_entity_group_markers()
	if !FastTilemap:
		return
	var tilesheet: Texture2D = FastTilemap.get_tilesheet()
	if !tilesheet:
		return
	var rules: Dictionary = editor_entity_group_rules_by_level.get(_active_level_key(), {})
	var cell_size := FastTilemap.get_cell_size()
	for rule_key in rules.keys():
		var rule: Dictionary = rules[rule_key]
		var entity_group_id := str(rule.get("entity_group", ""))
		if entity_group_id.is_empty():
			continue
		var atlas: Vector2i = ENTITY_GROUP_ATLAS
		if atlas.x < 0 or atlas.y < 0:
			continue
		var local_pos := _rule_pos_from_variant(rule.get("pos", []))
		var world_pos := _world_pos_from_local(local_pos)
		var atlas_texture := AtlasTexture.new()
		atlas_texture.atlas = tilesheet
		atlas_texture.region = Rect2(
			1 + atlas.x * (TILE_SIZE + 1),
			1 + atlas.y * (TILE_SIZE + 1),
			TILE_SIZE,
			TILE_SIZE
		)
		var marker := Sprite2D.new()
		marker.texture = atlas_texture
		marker.centered = false
		marker.modulate = Color(1, 1, 1, 0.7)
		marker.z_index = 20
		marker.position = (Vector2(world_pos.x, world_pos.y) - playerOffset) * cell_size
		add_child(marker)
		editor_entity_group_sprites[rule_key] = marker

func _clear_editor_rule_state() -> void:
	editor_npc_rules_by_level.clear()
	editor_item_rules_by_level.clear()
	editor_loot_table_rules_by_level.clear()
	clear_editor_loot_table_markers()
	editor_entity_group_rules_by_level.clear()
	clear_editor_entity_group_markers()
	editor_other_rules_by_level.clear()

func commit_shape(shape_type: int, p1: Vector2i, p2: Vector2i, filled: bool, perfect: bool, id: String, type: String, amount: int = -1):
	var points = Editor.get_shape_points(shape_type, p1, p2, filled, perfect)
	for p in points:
		place_entry(p, id, type, amount)
	update_editor_visuals()

func is_inside_bubble(pos: Vector2i) -> bool:
	var half = BUBBLE_SIZE / 2
	return pos.x >= -half and pos.x < half and pos.y >= -half and pos.y < half

func _on_clear_button_pressed() -> void:
	save_undo_state()
	_clear_current_chunk(true)
	update_editor_visuals()

func get_mouse_tile_pos() -> Vector2i:
	var mouse_pos = get_global_mouse_position()
	var cell_size = FastTilemap.get_cell_size()
	return Vector2i(floor(mouse_pos.x / cell_size), floor(mouse_pos.y / cell_size))

func get_line_points(start: Vector2i, end: Vector2i) -> Array[Vector2i]:
	var points : Array[Vector2i] = []
	var x0 = start.x
	var y0 = start.y
	var x1 = end.x
	var y1 = end.y
	
	var dx = abs(x1 - x0)
	var dy = -abs(y1 - y0)
	var sx = 1 if x0 < x1 else -1
	var sy = 1 if y0 < y1 else -1
	var err = dx + dy
	
	while true:
		points.append(Vector2i(x0, y0))
		if x0 == x1 and y0 == y1: break
		var e2 = 2 * err
		if e2 >= dy:
			err += dy
			x0 += sx
		if e2 <= dx:
			err += dx
			y0 += sy
	return points

func _on_file_index_pressed(index: int) -> void:
	if index == 0:
		open_save.emit()
	elif index == 1:
		open_load.emit()

func _on_edit_index_pressed(index: int) -> void:
	var names = tools.keys()
	if index < names.size():
		_on_mode_changed(names[index])

func _on_chunk_index_pressed(index: int) -> void:
	if index == 0:
		isMovingChunk = !isMovingChunk
		if isMovingChunk:
			if active_tool:
				active_tool.on_deactivate()
			active_tool = null
			Input.set_custom_mouse_cursor(null)
			Editor.clear_preview_tiles()
		else:
			_on_mode_changed("pencil")

func _update_chunk_visual():
	var cell_size = FastTilemap.get_cell_size()
	var size = CHUNK_SIZE * cell_size
	ChunkVisual.points = PackedVector2Array([
		Vector2(0, 0),
		Vector2(size, 0),
		Vector2(size, size),
		Vector2(0, size)
	])
	var rel_pos = selectedChunkPos - Vector2i(playerOffset)
	ChunkVisual.position = Vector2(rel_pos) * cell_size
	update_editor_visuals()

func _level_key(z: int) -> String:
	return str(z)

func _update_z_label() -> void:
	if ZLevelLabel:
		ZLevelLabel.text = "Z: " + str(active_z)

func _reset_level_edit_state() -> void:
	undo_stack.clear()
	redo_stack.clear()
	active_selection = Rect2i()
	if active_tool:
		active_tool.on_deactivate()
	if tools.has("selection"):
		var selection_tool = tools["selection"]
		selection_tool.selection_rect = Rect2i()
		selection_tool.is_selecting = false
		selection_tool.is_moving = false
		selection_tool.is_floating = false
		selection_tool.move_offset = Vector2i.ZERO
		selection_tool.captured_contents.clear()
	Editor.clear_preview_tiles()

func _clear_current_chunk(clear_rules: bool = false) -> void:
	if !World:
		return
	clear_editor_npcs()
	clear_editor_live_items()
	clear_editor_loot_table_markers()
	clear_editor_entity_group_markers()
	if clear_rules:
		var key := _active_level_key()
		editor_npc_rules_by_level.erase(key)
		editor_item_rules_by_level.erase(key)
		editor_loot_table_rules_by_level.erase(key)
		editor_entity_group_rules_by_level.erase(key)
		editor_other_rules_by_level.erase(key)
	for x in range(selectedChunkPos.x, selectedChunkPos.x + CHUNK_SIZE):
		for y in range(selectedChunkPos.y, selectedChunkPos.y + CHUNK_SIZE):
			World.place_tile(x, y, "void")

func _level_has_non_void(level_data: Dictionary) -> bool:
	var palette: Array = level_data.get("palette", [])
	var blueprint: String = str(level_data.get("blueprint", ""))
	if palette.is_empty() or blueprint.is_empty():
		return false
	var rle: String = blueprint.replace("(", "").replace(")", "").replace("[", "").replace("]", "")
	for raw_part in rle.split(","):
		var part: String = raw_part.strip_edges()
		if part.is_empty():
			continue
		var pieces: PackedStringArray = part.split("x")
		if pieces.size() != 2:
			continue
		var count: int = int(pieces[0])
		var palette_index: int = int(pieces[1])
		if count <= 0 or palette_index < 0 or palette_index >= palette.size():
			continue
		if str(palette[palette_index]) != "void":
			return true
	return false

func _level_has_rules(level_data: Dictionary) -> bool:
	if !level_data.has("rules"):
		return false
	var rules: Variant = level_data["rules"]
	return rules is Array and !rules.is_empty()

func _normalize_structure_size(size: Vector2i) -> Vector2i:
	var normalized := size
	if normalized.x <= 0:
		normalized.x = CHUNK_SIZE
	if normalized.y <= 0:
		normalized.y = CHUNK_SIZE
	normalized.x = clampi(normalized.x, 1, CHUNK_SIZE)
	normalized.y = clampi(normalized.y, 1, CHUNK_SIZE)
	return normalized

func _structure_size_from_variant(value: Variant, fallback: Vector2i = Vector2i(24, 24)) -> Vector2i:
	var size := fallback
	if value is Vector2i:
		size = value
	elif value is Vector2:
		size = Vector2i(int(value.x), int(value.y))
	elif value is Array and value.size() >= 2:
		size = Vector2i(int(value[0]), int(value[1]))
	return _normalize_structure_size(size)

func _decode_level_tiles(level_data: Dictionary, source_size: Vector2i) -> Array:
	var total := source_size.x * source_size.y
	var tiles: Array = []
	tiles.resize(total)
	for i in range(total):
		tiles[i] = "void"

	var palette: Array = level_data.get("palette", [])
	var blueprint: String = str(level_data.get("blueprint", ""))
	var rle: String = blueprint.replace("(", "").replace(")", "").replace("[", "").replace("]", "")
	var current_pos := 0

	for raw_part in rle.split(","):
		var part: String = raw_part.strip_edges()
		if part.is_empty():
			continue
		var pieces: PackedStringArray = part.split("x")
		if pieces.size() != 2:
			continue
		var count: int = int(pieces[0])
		var palette_index: int = int(pieces[1])
		var tile_id := "void"
		if palette_index >= 0 and palette_index < palette.size():
			tile_id = str(palette[palette_index])
		for _i in range(count):
			if current_pos >= total:
				return tiles
			tiles[current_pos] = tile_id
			current_pos += 1

	return tiles

func _append_rle_run(parts: PackedStringArray, palette: Array, id_to_index: Dictionary, tile_id: String, count: int) -> void:
	if count <= 0:
		return
	if !id_to_index.has(tile_id):
		id_to_index[tile_id] = palette.size()
		palette.append(tile_id)
	parts.append("%dx%d" % [count, int(id_to_index[tile_id])])

func _encode_tiles_to_level(tiles: Array) -> Dictionary:
	var palette: Array = []
	var id_to_index: Dictionary = {}
	var parts := PackedStringArray()
	var current_id := ""
	var count := 0

	for tile_id_variant in tiles:
		var tile_id := str(tile_id_variant)
		if tile_id.is_empty():
			tile_id = "void"
		if tile_id == current_id:
			count += 1
		else:
			_append_rle_run(parts, palette, id_to_index, current_id, count)
			current_id = tile_id
			count = 1
	_append_rle_run(parts, palette, id_to_index, current_id, count)

	return {
		"palette": palette,
		"blueprint": "(" + ", ".join(parts) + ")"
	}

func _rule_pos_from_variant(value: Variant) -> Vector2i:
	if value is Vector2i:
		return value
	if value is Vector2:
		return Vector2i(int(value.x), int(value.y))
	if value is Array and value.size() >= 2:
		return Vector2i(int(value[0]), int(value[1]))
	return Vector2i.ZERO

func _filter_rules_for_size(rules: Array, size: Vector2i) -> Array:
	var filtered: Array = []
	for rule_variant in rules:
		if !(rule_variant is Dictionary):
			continue
		var rule: Dictionary = rule_variant
		if rule.has("pos"):
			var pos := _rule_pos_from_variant(rule["pos"])
			if pos.x < 0 or pos.x >= size.x or pos.y < 0 or pos.y >= size.y:
				continue
		filtered.append(rule.duplicate(true))
	return filtered

func _is_spawn_entity_rule(rule: Dictionary) -> bool:
	var type_name := str(rule.get("type", ""))
	return type_name == "spawn_entity" or type_name == "spawn_point"

func _is_spawn_item_rule(rule: Dictionary) -> bool:
	return str(rule.get("type", "")) == "spawn_item"

func _is_spawn_loot_table_rule(rule: Dictionary) -> bool:
	return str(rule.get("type", "")) == "spawn_loot_table"

func _is_spawn_entity_group_rule(rule: Dictionary) -> bool:
	return str(rule.get("type", "")) == "spawn_entity_group"

func _split_editor_rules_for_level(key: String, level_data: Dictionary) -> void:
	var unmanaged: Array = []
	var raw_rules: Variant = level_data.get("rules", [])
	if raw_rules is Array:
		for rule_variant in raw_rules:
			if !(rule_variant is Dictionary):
				continue
			var rule: Dictionary = rule_variant
			if _is_spawn_entity_rule(rule):
				var race_id := str(rule.get("entity", rule.get("race_id", "")))
				if !race_id.is_empty() and rule.has("pos"):
					var pos := _rule_pos_from_variant(rule["pos"])
					var rules := _get_level_rule_dict(editor_npc_rules_by_level, key)
					var stored_rule := {
						"type": "spawn_entity",
						"entity": race_id,
						"pos": _rule_pos_array(pos),
						"job": str(rule.get("job", "")).strip_edges()
					}
					if str(stored_rule["job"]).is_empty():
						stored_rule["job"] = _default_job_for_race(race_id)
					if _is_sapient_race(race_id):
						var dialogue_id := str(rule.get("dialogue_id", "")).strip_edges()
						if !dialogue_id.is_empty() and _job_allows_dialogue(str(stored_rule["job"]), dialogue_id):
							stored_rule["dialogue_id"] = dialogue_id
					rules[_pos_key(pos)] = stored_rule
					continue
			elif _is_spawn_item_rule(rule):
				var item_id := str(rule.get("item_id", ""))
				var amount := int(rule.get("amount", 0))
				if !item_id.is_empty() and amount > 0 and rule.has("pos"):
					var pos := _rule_pos_from_variant(rule["pos"])
					var rules := _get_level_rule_dict(editor_item_rules_by_level, key)
					var stored_rule := rule.duplicate(true)
					stored_rule["type"] = "spawn_item"
					stored_rule["item_id"] = item_id
					stored_rule["amount"] = amount
					stored_rule["pos"] = _rule_pos_array(pos)
					rules[_item_rule_key(pos, item_id)] = stored_rule
					continue
			elif _is_spawn_loot_table_rule(rule):
				var loot_table_id := str(rule.get("loot_table", "")).strip_edges()
				if !loot_table_id.is_empty() and rule.has("pos"):
					var pos := _rule_pos_from_variant(rule["pos"])
					var rules := _get_level_rule_dict(editor_loot_table_rules_by_level, key)
					rules[_pos_key(pos)] = {
						"type": "spawn_loot_table",
						"loot_table": loot_table_id,
						"pos": _rule_pos_array(pos)
					}
					continue
			elif _is_spawn_entity_group_rule(rule):
				var entity_group_id := str(rule.get("entity_group", rule.get("entity_group_id", ""))).strip_edges()
				if !entity_group_id.is_empty() and rule.has("pos"):
					var pos := _rule_pos_from_variant(rule["pos"])
					var rules := _get_level_rule_dict(editor_entity_group_rules_by_level, key)
					rules[_pos_key(pos)] = {
						"type": "spawn_entity_group",
						"entity_group": entity_group_id,
						"pos": _rule_pos_array(pos)
					}
					continue
			unmanaged.append(rule.duplicate(true))
	if unmanaged.is_empty():
		level_data.erase("rules")
	else:
		level_data["rules"] = unmanaged
	editor_other_rules_by_level[key] = unmanaged

func _split_editor_rules_from_levels(imported_levels: Dictionary) -> void:
	for key in imported_levels.keys():
		var value: Variant = imported_levels[key]
		if value is Dictionary:
			_split_editor_rules_for_level(str(key), value)

func _rules_for_level(key: String) -> Array:
	var result: Array = []
	for rule in editor_other_rules_by_level.get(key, []):
		if rule is Dictionary:
			result.append(rule.duplicate(true))
	var npc_rules: Dictionary = editor_npc_rules_by_level.get(key, {})
	var npc_keys: Array = npc_rules.keys()
	npc_keys.sort()
	for npc_key in npc_keys:
		result.append(npc_rules[npc_key].duplicate(true))
	var item_rules: Dictionary = editor_item_rules_by_level.get(key, {})
	var item_keys: Array = item_rules.keys()
	item_keys.sort()
	for item_key in item_keys:
		result.append(item_rules[item_key].duplicate(true))
	var loot_table_rules: Dictionary = editor_loot_table_rules_by_level.get(key, {})
	var loot_table_keys: Array = loot_table_rules.keys()
	loot_table_keys.sort()
	for loot_table_key in loot_table_keys:
		result.append(loot_table_rules[loot_table_key].duplicate(true))
	var entity_group_rules: Dictionary = editor_entity_group_rules_by_level.get(key, {})
	var entity_group_keys: Array = entity_group_rules.keys()
	entity_group_keys.sort()
	for entity_group_key in entity_group_keys:
		result.append(entity_group_rules[entity_group_key].duplicate(true))
	return result

func _sync_level_rules_into(key: String, level_data: Dictionary) -> void:
	var rules := _rules_for_level(key)
	if rules.is_empty():
		level_data.erase("rules")
	else:
		level_data["rules"] = rules

func _sync_all_level_rules() -> void:
	var seen := {}
	for key in levels.keys():
		seen[str(key)] = true
	for key in editor_other_rules_by_level.keys():
		seen[str(key)] = true
	for key in editor_npc_rules_by_level.keys():
		seen[str(key)] = true
	for key in editor_item_rules_by_level.keys():
		seen[str(key)] = true
	for key in editor_loot_table_rules_by_level.keys():
		seen[str(key)] = true
	for key in editor_entity_group_rules_by_level.keys():
		seen[str(key)] = true
	for key in seen.keys():
		var level_data: Dictionary = levels.get(key, {})
		_sync_level_rules_into(key, level_data)
		if _level_has_non_void(level_data) or _level_has_rules(level_data):
			levels[key] = level_data
		else:
			levels.erase(key)

func _apply_editor_rules_for_level(z: int) -> void:
	var key := _level_key(z)
	var npc_rules: Dictionary = editor_npc_rules_by_level.get(key, {})
	var npc_keys: Array = npc_rules.keys()
	npc_keys.sort()
	for npc_key in npc_keys:
		var rule: Dictionary = npc_rules[npc_key]
		var race_id := str(rule.get("entity", rule.get("race_id", "")))
		if race_id.is_empty():
			continue
		place_npc(_world_pos_from_local(_rule_pos_from_variant(rule.get("pos", []))), race_id)
	var item_rules: Dictionary = editor_item_rules_by_level.get(key, {})
	var item_keys: Array = item_rules.keys()
	item_keys.sort()
	for item_key in item_keys:
		var rule: Dictionary = item_rules[item_key]
		var item_id := str(rule.get("item_id", ""))
		var amount := int(rule.get("amount", 0))
		if item_id.is_empty() or amount <= 0:
			continue
		var pos := _world_pos_from_local(_rule_pos_from_variant(rule.get("pos", [])))
		World.drop_item(pos, item_id, amount)
		editor_live_item_rules[item_key] = rule.duplicate(true)
	_refresh_loot_table_markers()
	_refresh_entity_group_markers()

func _compact_level_to_size(level_data: Dictionary, source_size: Vector2i, target_size: Vector2i) -> Dictionary:
	var source_tiles := _decode_level_tiles(level_data, source_size)
	var target_tiles: Array = []
	target_tiles.resize(target_size.x * target_size.y)

	for y in range(target_size.y):
		for x in range(target_size.x):
			var target_index := y * target_size.x + x
			var tile_id := "void"
			if x < source_size.x and y < source_size.y:
				var source_index := y * source_size.x + x
				if source_index >= 0 and source_index < source_tiles.size():
					tile_id = str(source_tiles[source_index])
			target_tiles[target_index] = tile_id

	var compact_level := _encode_tiles_to_level(target_tiles)
	var rules := _filter_rules_for_size(level_data.get("rules", []), target_size)
	if level_data.has("rules"):
		compact_level["rules"] = rules
	return compact_level

func _capture_active_level() -> void:
	if !World:
		return
	if active_tool:
		active_tool.on_deactivate()
	var key: String = _level_key(active_z)
	var level_data: Dictionary = Editor.export_to_rle("", selectedChunkPos, active_z, structure_size)
	level_data.erase("id")
	_sync_level_rules_into(key, level_data)
	if _level_has_non_void(level_data) or _level_has_rules(level_data):
		levels[key] = level_data
	else:
		levels.erase(key)

func _apply_level(z: int) -> void:
	if !World:
		return
	clear_editor_npcs()
	clear_editor_live_items()
	World.set_active_z(z)
	active_z = z
	_clear_current_chunk()
	var key: String = _level_key(z)
	if levels.has(key):
		var level_data: Dictionary = levels[key]
		Editor.import_from_rle(level_data.get("blueprint", ""), level_data.get("palette", []), selectedChunkPos, z, structure_size)
	_apply_editor_rules_for_level(z)
	_reset_level_edit_state()
	_update_z_label()
	update_editor_visuals()

func change_z(delta: int) -> void:
	if delta == 0 or !World:
		return
	_capture_active_level()
	_apply_level(active_z + delta)

func new_structure() -> void:
	if !World:
		return
	if active_tool:
		active_tool.on_deactivate()
	clear_editor_npcs()
	clear_editor_live_items()
	_clear_editor_rule_state()
	levels.clear()
	structure_size = Vector2i(CHUNK_SIZE, CHUNK_SIZE)
	set_current_structure_details("", "building", structure_size, ["north", "east", "south", "west"], [], "res://data/structures/structures.json")
	active_z = 0
	World.set_active_z(active_z)
	_clear_current_chunk()
	_reset_level_edit_state()
	_update_z_label()
	update_editor_visuals()

func import_structure(structure_data: Dictionary) -> void:
	if !World:
		return
	if active_tool:
		active_tool.on_deactivate()
	clear_editor_npcs()
	clear_editor_live_items()
	_clear_editor_rule_state()
	var imported_levels: Dictionary = {}
	var root_size := _structure_size_from_variant(structure_data.get("size", []))
	structure_size = root_size
	var dungeon_entrances: Array = []
	var dungeon_room_var: Variant = structure_data.get("dungeon_room", {})
	if dungeon_room_var is Dictionary:
		var dungeon_room: Dictionary = dungeon_room_var
		dungeon_entrances = dungeon_room.get("entrances", []).duplicate()
	var structure_entrances: Array = []
	var structure_entrance_var: Variant = structure_data.get("entrance", [])
	if structure_entrance_var is Array:
		structure_entrances = structure_entrance_var.duplicate()
	set_current_structure_details(
		str(structure_data.get("id", "")),
		str(structure_data.get("type", "building")),
		root_size,
		dungeon_entrances,
		structure_entrances,
		str(structure_data.get("filepath", "res://data/structures/structures.json"))
	)
	if structure_data.has("levels") and structure_data["levels"] is Dictionary:
		var raw_levels: Dictionary = structure_data["levels"]
		for key in raw_levels.keys():
			var value: Variant = raw_levels[key]
			if value is Dictionary:
				imported_levels[str(key)] = value.duplicate(true)
	elif structure_data.has("blueprint") or structure_data.has("palette") or structure_data.has("rules"):
		var level_zero: Dictionary = {}
		level_zero["blueprint"] = structure_data.get("blueprint", "")
		level_zero["palette"] = structure_data.get("palette", [])
		if structure_data.has("rules"):
			level_zero["rules"] = structure_data["rules"]
		imported_levels["0"] = level_zero

	_split_editor_rules_from_levels(imported_levels)
	var levels_to_clear: Array = levels.keys()
	for key in imported_levels.keys():
		if !levels_to_clear.has(key):
			levels_to_clear.append(key)
	if !levels_to_clear.has(_level_key(active_z)):
		levels_to_clear.append(_level_key(active_z))

	for key in levels_to_clear:
		World.set_active_z(int(str(key)))
		_clear_current_chunk()

	levels = imported_levels
	_apply_level(0)

func set_current_structure_details(id: String, type: String, size: Vector2i, dungeon_entrances: Array = [], structure_entrances: Array = [], filepath: String = "") -> void:
	current_structure_id = id.strip_edges()
	current_structure_type = type.strip_edges() if !type.strip_edges().is_empty() else "building"
	structure_size = _normalize_structure_size(size)
	current_dungeon_room_entrances = dungeon_entrances.duplicate()
	current_structure_entrances = structure_entrances.duplicate()
	if !filepath.strip_edges().is_empty():
		current_structure_path = filepath.strip_edges()

func export_structure(id: String, footprint: Vector2i = Vector2i(24, 24)) -> Dictionary:
	_capture_active_level()
	_sync_all_level_rules()
	footprint = _normalize_structure_size(footprint)
	var source_size := structure_size
	var result_levels: Dictionary = {}
	var result: Dictionary = {
		"id": id,
		"size": [footprint.x, footprint.y],
		"levels": result_levels
	}
	var sorted_keys: Array = levels.keys()
	sorted_keys.sort_custom(func(a, b): return int(str(a)) < int(str(b)))
	for key in sorted_keys:
		var level_data: Dictionary = levels[key]
		var compact_level := _compact_level_to_size(level_data, source_size, footprint)
		if _level_has_non_void(compact_level) or _level_has_rules(compact_level):
			result_levels[str(key)] = compact_level
	if result_levels.is_empty():
		var blank_level: Dictionary = Editor.export_to_rle("", selectedChunkPos, 0, footprint)
		blank_level.erase("id")
		result_levels["0"] = blank_level
	structure_size = footprint
	return result
