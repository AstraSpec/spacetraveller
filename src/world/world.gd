extends Node2D

@export var tilesheet :Texture2D

const TILE_SIZE :int = 12
const WORLD_BUBBLE_SIZE :int = 32 # DIAMETER

var tile_RIDs :Array[RID]

# Initializes world bubble of tiles around player
func init_world_bubble() -> void:
	var center_position :Vector2 = Vector2(WORLD_BUBBLE_SIZE / 2, WORLD_BUBBLE_SIZE / 2)
	
	# Iterate through each position in the tile_RIDs array
	for i in WORLD_BUBBLE_SIZE * WORLD_BUBBLE_SIZE:
		var cell_position :Vector2 = Vector2(i / WORLD_BUBBLE_SIZE, i % WORLD_BUBBLE_SIZE)
		
		# Calculate the distance from the center the current cell
		var dx :int = center_position.x - cell_position.x
		var dy :int = center_position.y - cell_position.y
		var distance :int = sqrt(dx * dx + dy * dy)
		
		if distance < WORLD_BUBBLE_SIZE / 2:
			init_tile(cell_position)

# Initializes individual tile textures
func init_tile(cell_pos :Vector2) -> void:
	var tile_id :int = load_tile(cell_pos)
	
	var tile_instance :RID = RenderingServer.canvas_item_create()
	RenderingServer.canvas_item_set_parent(tile_instance, get_canvas_item())
	RenderingServer.canvas_item_add_texture_rect_region(
		tile_instance,
		Rect2(cell_pos.x * TILE_SIZE, cell_pos.y * TILE_SIZE, TILE_SIZE, TILE_SIZE),
		tilesheet,
		Rect2(1, 27, TILE_SIZE, TILE_SIZE))
	tile_RIDs.append(tile_instance)

# Obtains tile data TODO: rename this cell?
func load_tile(cell_pos :Vector2) -> int:
	# TODO: first check if tile has been loaded in the past
	
	
	
	return 0
