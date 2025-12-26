extends Node2D

@export var StructureEditor_ :StructureEditor
@export var Background :TextureRect
@export var TileIDLabel :Label

@export var InputID     :TextEdit

const BUBBLE_SIZE :int = 32

var tileID :String

func _ready() -> void:
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
	
	select_tile("stone_bricks")

func select_tile(id :String):
	tileID = id
	TileIDLabel.text = "ID: " + id

func _unhandled_input(_event: InputEvent) -> void:
	if Input.is_action_pressed("left_mouse"):
		place_tile()
	elif Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT):
		place_tile("void")

func place_tile(id: String = tileID):
	if !id: return
	
	var mouse_pos = get_global_mouse_position()
	var cell_size = StructureEditor_.get_cell_size()
	var x = floor(mouse_pos.x / cell_size)
	var y = floor(mouse_pos.y / cell_size)
	
	StructureEditor_.place_tile(x, y, id)
	StructureEditor_.update_visuals(Vector2i(0, 0))

func _on_tile_grid_tile_selected(id: String) -> void:
	select_tile(id)

func _on_download_button_pressed() -> void:
	var RLE :Dictionary = StructureEditor_.export_to_rle(InputID.text)
	print(RLE)

func _on_clear_button_pressed() -> void:
	StructureEditor_.clear_cache()
	StructureEditor_.update_visuals(Vector2i(0, 0))
