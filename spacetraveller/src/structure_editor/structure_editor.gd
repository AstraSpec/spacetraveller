extends Node2D

signal open_save

@export var Editor :StructureEditor
@export var Background :TextureRect
@export var TileIDLabel1 :Label
@export var TileIDLabel2 :Label
@export var TileGrid :GridContainer
@export var Camera :Camera2D

@export var InputID   :TextEdit
@export var InputData :TextEdit

var CHUNK_SIZE = WorldGeneration.get_chunk_size()
var BUBBLE_SIZE :int = CHUNK_SIZE
const DRAG_SPEED :float = 1.75
const ZOOM_LVL :Array = [0.75, 1.0, 1.5, 2.0]

var REGION_SIZE = WorldGeneration.get_region_size()
var TILE_SIZE = FastTileMap.get_tile_size()

var zoom :int = 1

enum mode { PENCIL, LINE, EYEDROPPER, FILL }
var currentMode = mode.PENCIL

var tileID1 :String
var tileID2 :String
var lastMousePos :Vector2i
var mousePos :Vector2i

var isDrawingLine :bool = false
var lineStart :Vector2i
var lineButton :String = "left"

func _ready() -> void:
	InputManager.structure_mode_changed.connect(_on_mode_changed)
	InputManager.structure_mouse_input.connect(_on_mouse_input)
	
	InputManager.view_panned.connect(_view_panned)
	InputManager.view_zoomed.connect(_view_zoomed)
	InputManager.view_centered.connect(_view_centered)
	
	TileDb.initialize_data()
	StructureDb.initialize_data()
	
	Editor.set_world_bubble_size(BUBBLE_SIZE)
	Editor.init_world_bubble(Vector2i(0, 0), true)
	Editor.update_visuals(Vector2i(0, 0))
	
	Background.size = Vector2(
		CHUNK_SIZE * Editor.get_cell_size(),
		CHUNK_SIZE * Editor.get_cell_size()
	)
	
	Background.position -= Vector2(
		CHUNK_SIZE * Editor.get_cell_size() / 2,
		CHUNK_SIZE * Editor.get_cell_size() / 2
	)
	
	InputManager.current_mode = InputManager.InputMode.STRUCTURE
	
	select_tile("stone_bricks")
	select_tile("void", false)
	
	TileGrid.start()

func _view_panned(relative: Vector2):
	_update_camera_pos(Camera.position - relative * DRAG_SPEED / ZOOM_LVL[zoom])

func _view_zoomed(z :int):
	var oldZoom = zoom
	zoom = clamp(zoom + z, 0, ZOOM_LVL.size() - 1)
	
	if oldZoom != zoom:
		Camera.zoom = Vector2(ZOOM_LVL[zoom], ZOOM_LVL[zoom])
		_update_camera_pos(Camera.position)

func _view_centered():
	_update_camera_pos(Vector2.ZERO)

func _update_camera_pos(newPos: Vector2) -> void:
	Camera.position = newPos.floor()

func _process(_delta: float) -> void:
	mousePos = get_mouse_tile_pos()
	
	if mousePos != lastMousePos:
		_on_tile_changed(mousePos)
		lastMousePos = mousePos

func _on_mode_changed(m :String):
	if m == "pencil": currentMode = mode.PENCIL
	elif m == "line": currentMode = mode.LINE
	elif m == "eyedropper": currentMode = mode.EYEDROPPER
	elif m == "fill": currentMode = mode.FILL

func _on_mouse_input(button: String, action: InputManager.MouseAction):
	if action == InputManager.MouseAction.RELEASE:
		if currentMode == mode.LINE and isDrawingLine and button == lineButton:
			_commit_line()
		return

	if currentMode == mode.PENCIL:
		if button == "left":
			place_tile(tileID1)
		else:
			place_tile(tileID2)
	
	elif currentMode == mode.LINE:
		if action == InputManager.MouseAction.PRESS:
			if !isDrawingLine:
				isDrawingLine = true
				lineStart = mousePos
				lineButton = button
				_on_tile_changed(mousePos)
	
	elif currentMode == mode.EYEDROPPER:
		if action == InputManager.MouseAction.PRESS:
			if !is_inside_bubble(mousePos): return
			var id = Editor.get_tile_at(mousePos.x, mousePos.y)
			select_tile(id, button == "left")
	
	elif currentMode == mode.FILL:
		if action == InputManager.MouseAction.PRESS:
			if !is_inside_bubble(mousePos): return
			var tid = tileID1 if button == "left" else tileID2
			Editor.fill_tiles(mousePos.x, mousePos.y, tid)
			Editor.update_visuals(Vector2i(0, 0))

func _commit_line():
	var points = get_line_points(lineStart, mousePos)
	var tid = tileID1 if lineButton == "left" else tileID2
	for p in points:
		if is_inside_bubble(p):
			Editor.place_tile(p.x, p.y, tid)
	Editor.update_visuals(Vector2i(0, 0))
	isDrawingLine = false
	_on_tile_changed(mousePos)

func select_tile(id :String, is_primary :bool = true):
	if is_primary:
		tileID1 = id
	else:
		tileID2 = id
	
	TileIDLabel1.text = "ID1: " + tileID1
	TileIDLabel2.text = "ID2: " + tileID2

func _on_tile_changed(_pos: Vector2i):
	if currentMode == mode.FILL or currentMode == mode.EYEDROPPER:
		Editor.clear_preview_tiles()
	elif currentMode == mode.LINE and isDrawingLine:
		var points = get_line_points(lineStart, _pos)
		var tid = tileID1 if lineButton == "left" else tileID2
		Editor.update_preview_tiles(points, tid)
	else:
		Editor.update_preview_tiles([_pos], tileID1)

func place_tile(id: String):
	if !id or !is_inside_bubble(mousePos): return
	
	Editor.place_tile(mousePos.x, mousePos.y, id)
	Editor.update_visuals(Vector2i(0, 0))

func is_inside_bubble(pos: Vector2i) -> bool:
	var half = CHUNK_SIZE / 2
	return pos.x >= -half and pos.x < half and pos.y >= -half and pos.y < half

func _on_tile_grid_tile_selected(id: String, is_primary: bool) -> void:
	select_tile(id, is_primary)

func _on_download_button_pressed() -> void:
	var RLE :Dictionary = Editor.export_to_rle(InputID.text)
	InputData.text = JSON.stringify(RLE)

func _on_clear_button_pressed() -> void:
	Editor.clear_cache()
	Editor.update_visuals(Vector2i(0, 0))

func _on_load_button_pressed() -> void:
	var json_data = InputData.text.strip_edges()
	if json_data.is_empty(): return
	
	var json = JSON.new()
	if json.parse(json_data) != OK:
		printerr("JSON Parse Error: ", json.get_error_message(), " at line ", json.get_error_line())
		return
	
	var data = json.get_data()
	if typeof(data) != TYPE_DICTIONARY or !data.has("blueprint") or !data.has("palette"):
		printerr("JSON Error: Invalid format or missing required keys")
		return
	
	if data.has("id"): InputID.text = data["id"]
	
	Editor.import_from_rle(data["blueprint"], data["palette"])
	Editor.update_visuals(Vector2i(0, 0))

func get_mouse_tile_pos() -> Vector2i:
	var mouse_pos = get_global_mouse_position()
	var cell_size = Editor.get_cell_size()
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
	if index == 0: open_save.emit()
