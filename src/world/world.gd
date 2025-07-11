extends Node2D

@export var Tilesheet :Texture2D

@onready var BiomeNoise :FastNoiseLite = preload("res://noise/biome_noise.tres")

var seed_ :int = 0

const TILE_SIZE :int = 12
const WORLD_BUBBLE_SIZE :int = 64 # DIAMETER
const WORLD_BUBBLE_RADIUS: int = WORLD_BUBBLE_SIZE / 2
const TILE_Y_GRASS :int = 27
const TILE_Y_STONE :int = 53
const TILE_Y_OCCLUDED :int = 66

var tile_RIDs :Dictionary

func _ready() -> void:
	BiomeNoise.seed = seed_

# Check if a tile is occluded from player's perspective
func is_occluded(playerPos :Vector2i, cellPos :Vector2i) -> bool:
	if cellPos == playerPos:
		return false
	
	var dx :int = abs(cellPos.x - playerPos.x)
	var dy :int = abs(cellPos.y - playerPos.y)
	var sx :int = 1 if playerPos.x < cellPos.x else -1
	var sy :int = 1 if playerPos.y < cellPos.y else -1
	var err :int = dx - dy
	
	var currentPos :Vector2i = playerPos
	
	while currentPos != cellPos:
		var biome :float = BiomeNoise.get_noise_2dv(currentPos)
		if biome > 0.5:  # Stone tile (wall)
			return true
		
		var e2 :int = 2 * err
		if e2 > -dy:
			err -= dy
			currentPos.x += sx
		if e2 < dx:
			err += dx
			currentPos.y += sy
	
	return false

# Initializes world bubble of tiles around player
func init_world_bubble(playerPos :Vector2i) -> void:
	var centrePos :Vector2i = Vector2i(WORLD_BUBBLE_RADIUS, WORLD_BUBBLE_RADIUS)

	# Iterate through each position in the tile_RIDs array
	for i in WORLD_BUBBLE_SIZE * WORLD_BUBBLE_SIZE:
		var tileOffset :Vector2i = Vector2i(i / WORLD_BUBBLE_SIZE as int, i % WORLD_BUBBLE_SIZE) - centrePos
		
		# Calculate the distance from the center the current tile
		if tileOffset.length() < WORLD_BUBBLE_RADIUS:
			init_tile(tileOffset, playerPos)

# Initializes individual tile textures
func init_tile(tileOffset :Vector2i, playerPos :Vector2i) -> void:
	var tileRID :RID = RenderingServer.canvas_item_create()
	RenderingServer.canvas_item_set_parent(tileRID, get_canvas_item())
	render_tile(tileRID, tileOffset, playerPos)
	
	tile_RIDs[tileOffset] = tileRID

# Renders tile texture
func render_tile(tileRID :RID, tileOffset :Vector2i, playerPos :Vector2i):
	RenderingServer.canvas_item_add_texture_rect_region(
		tileRID,
		Rect2i(tileOffset * TILE_SIZE, Vector2i(TILE_SIZE, TILE_SIZE)),
		Tilesheet,
		Rect2i(1, get_tile_y(tileOffset, playerPos), TILE_SIZE, TILE_SIZE))

# Updates world bubble of tiles around player
func update_world_bubble(playerPos :Vector2i) -> void:
	for tileOffset in tile_RIDs:
		update_tile(tile_RIDs[tileOffset], tileOffset, playerPos)

# Updates individual tile textures
func update_tile(tileRID :RID, tileOffset :Vector2i, playerPos :Vector2i) -> void:
	RenderingServer.canvas_item_clear(tileRID)
	render_tile(tileRID, tileOffset, playerPos)

func get_tile_y(tileOffset :Vector2i, playerPos :Vector2i) -> int:
	var cellPos :Vector2i = playerPos + tileOffset
	
	var biome :float = BiomeNoise.get_noise_2dv(cellPos)
	var base_tile_y :int = TILE_Y_STONE if biome > 0.5 else TILE_Y_GRASS
	
	# If it's a grass tile and occluded from player's perspective, show as occluded
	if base_tile_y == TILE_Y_GRASS and is_occluded(playerPos, cellPos):
		return TILE_Y_OCCLUDED
	
	return base_tile_y
