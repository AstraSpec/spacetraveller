extends Node2D

@export var StructureEditor_ :StructureEditor
@export var Background :TextureRect
@export var TileIDLabel :Label
@export var TileGrid :GridContainer
@export var Camera :Camera2D

@export var InputID   :TextEdit
@export var InputData :TextEdit

const BUBBLE_SIZE :int = 32
const DRAG_SPEED :float = 1.75
const ZOOM_LVL :Array = [0.75, 1.0, 1.5, 2.0]

var REGION_SIZE = WorldGeneration.get_region_size()
var TILE_SIZE = FastTileMap.get_tile_size()

var zoom :int = 1

enum mode { PENCIL, LINE, EYEDROPPER, FILL }
var currentMode = mode.PENCIL

var tileID :String
var lastMousePos :Vector2i
var mousePos :Vector2i

var isDrawingLine :bool = false
var lineStart :Vector2i

func _ready() -> void:
	InputManager.structure_mode_changed.connect(_on_mode_changed)
	InputManager.structure_mouse_input.connect(_on_mouse_input)
	
	InputManager.view_panned.connect(_view_panned)
	InputManager.view_zoomed.connect(_view_zoomed)
	InputManager.view_centered.connect(_view_centered)
	
	TileDb.initialize_data()
	StructureDb.initialize_data()
	
	StructureEditor_.set_world_bubble_size(BUBBLE_SIZE)
	StructureEditor_.init_world_bubble(Vector2i(0, 0), true)
	StructureEditor_.update_visuals(Vector2i(0, 0))
	
	Background.size = Vector2(
		BUBBLE_SIZE * StructureEditor_.get_cell_size(),
		BUBBLE_SIZE * StructureEditor_.get_cell_size()
	)
	
	Background.position -= Vector2(
		BUBBLE_SIZE * StructureEditor_.get_cell_size() / 2,
		BUBBLE_SIZE * StructureEditor_.get_cell_size() / 2
	)
	
	InputManager.current_mode = InputManager.InputMode.STRUCTURE
	
	select_tile("stone_bricks")
	
	TileGrid.start(StructureEditor_.get_spacing())

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
	
	if currentMode == mode.LINE and isDrawingLine:
		if !Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT):
			# Commit line
			var points = get_line_points(lineStart, mousePos)
			for p in points:
				StructureEditor_.place_tile(p.x, p.y, tileID)
			StructureEditor_.update_visuals(Vector2i(0, 0))
			isDrawingLine = false
			_on_tile_changed(mousePos)
	
	if mousePos != lastMousePos:
		_on_tile_changed(mousePos)
		lastMousePos = mousePos

func _on_mode_changed(m :String):
	if m == "pencil": currentMode = mode.PENCIL
	elif m == "line": currentMode = mode.LINE
	elif m == "eyedropper": currentMode = mode.EYEDROPPER
	elif m == "fill": currentMode = mode.FILL

func _on_mouse_input(_button :String):
	if currentMode == mode.PENCIL:
		place_tile()
	
	elif currentMode == mode.LINE:
		if !isDrawingLine:
			isDrawingLine = true
			lineStart = mousePos
			_on_tile_changed(mousePos)
	
	elif currentMode == mode.EYEDROPPER:
		var id = StructureEditor_.get_tile_at(mousePos.x, mousePos.y)
		select_tile(id)
	
	elif currentMode == mode.FILL:
		StructureEditor_.fill_tiles(mousePos.x, mousePos.y, tileID)
		StructureEditor_.update_visuals(Vector2i(0, 0))

func select_tile(id :String):
	tileID = id
	TileIDLabel.text = "ID: " + id

func _on_tile_changed(_pos: Vector2i):
	if currentMode == mode.FILL or currentMode == mode.EYEDROPPER:
		StructureEditor_.clear_preview_tiles()
	elif currentMode == mode.LINE and isDrawingLine:
		var points = get_line_points(lineStart, _pos)
		StructureEditor_.update_preview_tiles(points, tileID)
	else:
		StructureEditor_.update_preview_tiles([_pos], tileID)

func place_tile(id: String = tileID):
	if !id: return
	
	StructureEditor_.place_tile(mousePos.x, mousePos.y, id)
	StructureEditor_.update_visuals(Vector2i(0, 0))

func _on_tile_grid_tile_selected(id: String) -> void:
	select_tile(id)

func _on_download_button_pressed() -> void:
	var RLE :Dictionary = StructureEditor_.export_to_rle(InputID.text)
	print(RLE)
	InputData.text = JSON.stringify(RLE)

func _on_clear_button_pressed() -> void:
	StructureEditor_.clear_cache()
	StructureEditor_.update_visuals(Vector2i(0, 0))

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
	
	StructureEditor_.import_from_rle(data["blueprint"], data["palette"])
	StructureEditor_.update_visuals(Vector2i(0, 0))

func get_mouse_tile_pos() -> Vector2i:
	var mouse_pos = get_global_mouse_position()
	var cell_size = StructureEditor_.get_cell_size()
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
