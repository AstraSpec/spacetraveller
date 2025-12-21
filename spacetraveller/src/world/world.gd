extends WorldGeneration

@export var Tilesheet :Texture2D

@onready var BiomeNoise :FastNoiseLite = preload("res://noise/biome_noise.tres")

var seed_ :int = 0

const TILE_SIZE :int = 12
const WORLD_BUBBLE_SIZE :int = 64 # DIAMETER
const WORLD_BUBBLE_RADIUS: int = WORLD_BUBBLE_SIZE / 2
const TILE_Y_GROUND :int = 27
const TILE_Y_WALL :int = 53

var tileRIDs :Dictionary
var tileData :Dictionary

func _ready() -> void:
	BiomeNoise.seed = seed_

# Initializes world bubble of tiles around player
func init_world_bubble(playerPos :Vector2i) -> void:
	var centrePos :Vector2i = Vector2i(WORLD_BUBBLE_RADIUS, WORLD_BUBBLE_RADIUS)

	# Iterate through each position in the tile_RIDs array
	for i in WORLD_BUBBLE_SIZE * WORLD_BUBBLE_SIZE:
		var tileOffset :Vector2i = Vector2i(i / WORLD_BUBBLE_SIZE as int, i % WORLD_BUBBLE_SIZE) - centrePos
		
		# Calculate the distance from the center the current tile
		if tileOffset.length() < WORLD_BUBBLE_RADIUS:
			init_tile(tileOffset, playerPos)
	
	update_world_bubble(playerPos)

# Initializes individual tile textures
func init_tile(tileOffset :Vector2i, playerPos :Vector2i) -> void:
	var tileRID :RID = RenderingServer.canvas_item_create()
	RenderingServer.canvas_item_set_parent(tileRID, get_canvas_item())
	render_tile(tileRID, tileOffset, playerPos)
	
	tileRIDs[tileOffset] = tileRID

# Updates world bubble of tiles around player
func update_world_bubble(playerPos :Vector2i) -> void:
	for tileOffset in tileRIDs:
		update_tile(tileRIDs[tileOffset], tileOffset, playerPos)
	
	# Batch compute occlusion
	var occlusion_results = compute_occlusion_batch(tileRIDs.keys(), playerPos, tileData)
	for tileOffset in occlusion_results:
		apply_occlusion(tileRIDs[tileOffset], tileOffset, playerPos, occlusion_results[tileOffset])

# Updates individual tile textures
func update_tile(tileRID :RID, tileOffset :Vector2i, playerPos :Vector2i) -> void:
	RenderingServer.canvas_item_clear(tileRID)
	render_tile(tileRID, tileOffset, playerPos)

# Renders tile texture
func render_tile(tileRID :RID, tileOffset :Vector2i, playerPos :Vector2i) -> void:
	var cellPos :Vector2i = tileOffset + playerPos
	
	var tile_y :int
	if tileData.has(cellPos):
		tile_y = tileData[cellPos]["tile_y"]
	else:
		tile_y = get_tile_y(cellPos)
		tileData[cellPos] = {"tile_y": tile_y, "seen": false}
	
	RenderingServer.canvas_item_add_texture_rect_region(
		tileRID,
		Rect2i(tileOffset * TILE_SIZE, Vector2i(TILE_SIZE, TILE_SIZE)),
		Tilesheet,
		Rect2i(1, tile_y, TILE_SIZE, TILE_SIZE)
		)

func get_tile_y(cellPos :Vector2i) -> int:
	var biome :float = BiomeNoise.get_noise_2dv(cellPos)
	return TILE_Y_WALL if biome > 0.3 else TILE_Y_GROUND

# Applies precomputed occlusion result to a tile
func apply_occlusion(tileRID :RID, tileOffset :Vector2i, playerPos :Vector2i, is_occluded :bool):
	var cellPos :Vector2i = tileOffset + playerPos
	
	var color = Color(1, 1, 1, 1)
	if is_occluded: 
		if tileData[cellPos]["seen"] == true:
			color = Color(0.4, 0.4, 0.5, 1)
		else:
			color = Color(0, 0, 0, 1)
	else:
		tileData[cellPos]["seen"] = true
	
	RenderingServer.canvas_item_set_modulate(tileRID, color)
