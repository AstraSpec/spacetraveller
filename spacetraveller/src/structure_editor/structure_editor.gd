extends Node2D

signal open_save
signal open_load

@onready var ToolOption :PackedScene = preload("res://src/structure_editor/tool_option.tscn")
@export var Editor :StructureEditor
@export var TileIDLabel1 :Label
@export var TileIDLabel2 :Label
@export var TileGrid :GridContainer
@export var SelectionVisual :Line2D
@export var editMenu :PopupMenu
@export var ToolOptions :HBoxContainer
@export var LoadWindow :Window
var FastTilemap :FastTileMap
var spacing :int = 0

var CHUNK_SIZE = WorldGeneration.get_chunk_size()
var BUBBLE_SIZE :int = CHUNK_SIZE
var REGION_SIZE = WorldGeneration.get_region_size()
var TILE_SIZE = FastTileMap.get_tile_size()

var tools = {}
var active_tool
var active_selection : Rect2i = Rect2i()

var tileID1 :String
var tileID2 :String
var lastMousePos :Vector2i
var mousePos :Vector2i
var playerOffset :Vector2

var undo_stack : Array = []
var redo_stack : Array = []
const MAX_UNDOS = 100

func start_editor(offset :Vector2 = Vector2.ZERO) -> void:
	InputManager.structure_mode_changed.connect(_on_mode_changed)
	InputManager.structure_mouse_input.connect(_on_mouse_input)
	InputManager.structure_key_input.connect(_on_key_input)
	
	InputManager.set_mode(InputManager.InputMode.STRUCTURE)

	playerOffset = offset
	
	LoadWindow.FastTilemap = FastTilemap
	Editor.set_tilemap(FastTilemap)
	BUBBLE_SIZE = FastTilemap.get_world_bubble_size()
	
	setup_tools()
	
	select_tile("road_bricks")
	select_tile("void", false)
	
	_on_mode_changed("pencil")
	
	TileGrid.start(spacing)
	FastTilemap.update_visuals(playerOffset)

func setup_tools():
	tools = {
		"pencil": EditorTools.PencilTool.new(self),
		"line": EditorTools.LineTool.new(self),
		"rectangle": EditorTools.RectangleTool.new(self),
		"ellipsis": EditorTools.EllipsisTool.new(self),
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

func _on_mode_changed(m :String):
	if tools.has(m):
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

func save_undo_state():
	undo_stack.push_back(FastTilemap.get_tile_id_cache())
	if undo_stack.size() > MAX_UNDOS:
		undo_stack.pop_front()
	redo_stack.clear()

func undo():
	if undo_stack.is_empty(): return
	
	redo_stack.push_back(FastTilemap.get_tile_id_cache())
	var state = undo_stack.pop_back()
	FastTilemap.set_tile_id_cache(state)
	FastTilemap.update_visuals(playerOffset)

func redo():
	if redo_stack.is_empty(): return
	
	undo_stack.push_back(FastTilemap.get_tile_id_cache())
	var state = redo_stack.pop_back()
	FastTilemap.set_tile_id_cache(state)
	FastTilemap.update_visuals(playerOffset)

func select_tile(id :String, is_primary :bool = true):
	if is_primary:
		tileID1 = id
	else:
		tileID2 = id
	
	TileIDLabel1.text = "ID1: " + tileID1
	TileIDLabel2.text = "ID2: " + tileID2

func on_tile_changed(_pos: Vector2i):
	active_tool.on_hover(_pos)

func place_tile_at(pos: Vector2i, id: String):
	if !id or !is_inside_bubble(pos): return
	
	FastTilemap.place_tile(pos.x + playerOffset.x, pos.y + playerOffset.y, id)
	FastTilemap.update_visuals(playerOffset)

func is_inside_bubble(pos: Vector2i) -> bool:
	var half = BUBBLE_SIZE / 2
	return pos.x >= -half and pos.x < half and pos.y >= -half and pos.y < half

func _on_tile_grid_tile_selected(id: String, is_primary: bool) -> void:
	select_tile(id, is_primary)

func _on_clear_button_pressed() -> void:
	save_undo_state()
	FastTilemap.clear_cache()
	FastTilemap.update_visuals(playerOffset)

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
		FastTilemap.clear_cache()
		FastTilemap.update_visuals(playerOffset)
	elif index == 1: open_save.emit()
	elif index == 2: open_load.emit()

func _on_edit_index_pressed(index: int) -> void:
	var names = tools.keys()
	if index < names.size():
		_on_mode_changed(names[index])
