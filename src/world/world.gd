extends Node2D

@export var Tilesheet :Texture2D

@onready var BiomeNoise :FastNoiseLite = preload("res://noise/biome_noise.tres")
@onready var OcclusionOverlay :TextureRect = $OcclusionOverlay
@onready var ShaderMat :ShaderMaterial = OcclusionOverlay.material as ShaderMaterial
var tile_info_img :Image = Image.create(WORLD_BUBBLE_SIZE, WORLD_BUBBLE_SIZE, false, Image.FORMAT_RF)
var tile_info_tex :ImageTexture = ImageTexture.create_from_image(tile_info_img)

var seed_ :int = 0

const TILE_SIZE :int = 12
const WORLD_BUBBLE_SIZE :int = 64 # DIAMETER
const WORLD_BUBBLE_RADIUS: int = WORLD_BUBBLE_SIZE / 2
const TILE_Y_GRASS :int = 27
const TILE_Y_STONE :int = 53
const TILE_Y_OCCLUDED :int = 66

var tile_RIDs :Dictionary
var tile_data :Dictionary
var seen_cells :Dictionary

func _ready() -> void:
	BiomeNoise.seed = seed_
	
	init_visibility_rect()

func init_visibility_rect() -> void:
	ShaderMat.set_shader_parameter("tile_info", tile_info_tex)
	ShaderMat.set_shader_parameter("map_size", Vector2i(WORLD_BUBBLE_SIZE, WORLD_BUBBLE_SIZE))
	ShaderMat.set_shader_parameter("tile_size", Vector2i(TILE_SIZE, TILE_SIZE))
	
	OcclusionOverlay.texture = tile_info_tex
	OcclusionOverlay.scale = Vector2i(TILE_SIZE, TILE_SIZE)
	
	OcclusionOverlay.position = -(Vector2(WORLD_BUBBLE_SIZE, WORLD_BUBBLE_SIZE) * TILE_SIZE) / 2

func update_occlusion(playerPos: Vector2i):
	for i in WORLD_BUBBLE_SIZE * WORLD_BUBBLE_SIZE:
		var tileOffset = Vector2i(i / WORLD_BUBBLE_SIZE, i % WORLD_BUBBLE_SIZE) - Vector2i(WORLD_BUBBLE_RADIUS, WORLD_BUBBLE_RADIUS)
		var cellPos = playerPos + tileOffset
		var world_index = tileOffset + Vector2i(WORLD_BUBBLE_RADIUS, WORLD_BUBBLE_RADIUS)

		var tile_y = tile_data.get(cellPos, TILE_Y_GRASS)
		var value :float = 1.0 if tile_y == TILE_Y_GRASS else 0.0
		tile_info_img.set_pixelv(world_index, Color(value, 0, 0))
		seen_cells[cellPos] = value > 0.0
	
	tile_info_tex.update(tile_info_img)

	ShaderMat.set_shader_parameter("player_tile_pos", Vector2(WORLD_BUBBLE_RADIUS, WORLD_BUBBLE_RADIUS))

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
	
	tile_RIDs[tileOffset] = tileRID

# Renders tile texture
func render_tile(tileRID :RID, tileOffset :Vector2i, playerPos :Vector2i):
	var cellPos :Vector2i = tileOffset + playerPos
	var tile_y = get_tile_y(cellPos)
	tile_data[cellPos] = tile_y
	
	RenderingServer.canvas_item_add_texture_rect_region(
		tileRID,
		Rect2i(tileOffset * TILE_SIZE, Vector2i(TILE_SIZE, TILE_SIZE)),
		Tilesheet,
		Rect2i(1, tile_y, TILE_SIZE, TILE_SIZE))

# Updates world bubble of tiles around player
func update_world_bubble(playerPos :Vector2i) -> void:
	for tileOffset in tile_RIDs:
		update_tile(tile_RIDs[tileOffset], tileOffset, playerPos)
	
	update_occlusion(playerPos)

# Updates individual tile textures
func update_tile(tileRID :RID, tileOffset :Vector2i, playerPos :Vector2i) -> void:
	RenderingServer.canvas_item_clear(tileRID)
	render_tile(tileRID, tileOffset, playerPos)

func get_tile_y(cellPos :Vector2i) -> int:
	var biome :float = BiomeNoise.get_noise_2dv(cellPos)
	return TILE_Y_STONE if biome > 0.3 else TILE_Y_GRASS
