extends Node2D

signal open_save
signal open_load

@onready var ToolOption :PackedScene = preload("res://src/structure_editor/tool_option.tscn")
@export var Editor :StructureEditor
@export var TileIDLabel1 :Label
@export var TileIDLabel2 :Label
@export var TileGrid :GridContainer
@export var ItemGrid :GridContainer
@export var ItemAmountInput :SpinBox
@export var SelectionVisual :Line2D
@export var editMenu :PopupMenu
@export var chunkMenu :PopupMenu
@export var ToolOptions :HBoxContainer
@export var LoadWindow :Window
@export var CoordsLabel :Label
@export var ChunkVisual :Line2D
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
var lastMousePos :Vector2i
var mousePos :Vector2i
var playerOffset :Vector2

var undo_stack : Array = []
var redo_stack : Array = []
const MAX_UNDOS = 100

func update_editor_visuals():
	if World:
		World.update_world_bubble(playerOffset)
	elif FastTilemap:
		FastTilemap.update_visuals(playerOffset)

func start_editor(offset :Vector2 = Vector2.ZERO) -> void:
	InputManager.structure_mode_changed.connect(_on_mode_changed)
	InputManager.structure_mouse_input.connect(_on_mouse_input)
	InputManager.structure_key_input.connect(_on_key_input)
	
	playerOffset = offset

	LoadWindow.World = World
	LoadWindow.FastTilemap = FastTilemap
	if World:
		Editor.set_world(World)
	Editor.set_tilemap(FastTilemap)
	BUBBLE_SIZE = FastTilemap.get_world_bubble_size()
	
	setup_tools()
	
	select_entry("road_bricks", "tile")
	select_entry("void", "tile", false)
	
	_on_mode_changed("pencil")
	
	selectedChunkPos = Vector2i(floor(offset.x / CHUNK_SIZE), floor(offset.y / CHUNK_SIZE)) * CHUNK_SIZE
	if !chunkMenu.index_pressed.is_connected(_on_chunk_index_pressed):
		chunkMenu.index_pressed.connect(_on_chunk_index_pressed)
	_update_chunk_visual()
	
	TileGrid.start(spacing, TileDb)
	ItemGrid.start(spacing, ItemDb)
	
	TileGrid.selection_changed.connect(func(id, is_primary): select_entry(id, "tile", is_primary))
	ItemGrid.selection_changed.connect(func(id, is_primary): select_entry(id, "item", is_primary))
	
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
		"undo": undo()
		"redo": redo()
	
	if active_tool and active_tool.has_method("on_key"):
		active_tool.on_key(key)

func save_undo_state():
	undo_stack.push_back(World.get_tile_id_cache())
	if undo_stack.size() > MAX_UNDOS:
		undo_stack.pop_front()
	redo_stack.clear()

func undo():
	if undo_stack.is_empty(): return
	
	redo_stack.push_back(World.get_tile_id_cache())
	var state = undo_stack.pop_back()
	World.set_tile_id_cache(state)
	update_editor_visuals()

func redo():
	if redo_stack.is_empty(): return
	
	undo_stack.push_back(World.get_tile_id_cache())
	var state = redo_stack.pop_back()
	World.set_tile_id_cache(state)
	update_editor_visuals()

func select_entry(id: String, type: String = "tile", is_primary: bool = true):
	if is_primary:
		tileID1 = id
		tileType1 = type
	else:
		tileID2 = id
		tileType2 = type
	
	TileIDLabel1.text = "ID1: " + tileID1
	TileIDLabel2.text = "ID2: " + tileID2

func on_tile_changed(_pos: Vector2i):
	if active_tool:
		active_tool.on_hover(_pos)

var item_amount: int:
	get: return int(ItemAmountInput.value) if ItemAmountInput else 1

func place_at(pos: Vector2i, id: String, type: String = "tile"):
	if !id or !is_inside_bubble(pos): return
	place_entry(Vector2i(int(pos.x + playerOffset.x), int(pos.y + playerOffset.y)), id, type)
	update_editor_visuals()

func place_entry(world_pos: Vector2i, id: String, type: String = "tile", amount: int = -1):
	match type:
		"item":
			World.drop_item(world_pos, id, amount if amount > 0 else item_amount)
		_:
			World.place_tile(world_pos.x, world_pos.y, id)

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
	for x in range(selectedChunkPos.x, selectedChunkPos.x + CHUNK_SIZE):
		for y in range(selectedChunkPos.y, selectedChunkPos.y + CHUNK_SIZE):
			World.place_tile(x, y, "void")
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
		save_undo_state()
		for x in range(selectedChunkPos.x, selectedChunkPos.x + CHUNK_SIZE):
			for y in range(selectedChunkPos.y, selectedChunkPos.y + CHUNK_SIZE):
				World.place_tile(x, y, "void")
		update_editor_visuals()
	elif index == 1: open_save.emit()
	elif index == 2: open_load.emit()

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
