extends WorldGeneration

@export var Tilesheet :Texture2D
@onready var BiomeNoise :FastNoiseLite = preload("res://noise/biome_noise.tres")

signal generated(regionChunks)

var seed_ :int = 0

func _ready() -> void:
	biome_noise = BiomeNoise
	tilesheet = Tilesheet
	world_seed = seed_
	
func generate_world(playerPos :Vector2i) -> void:
	var regionChunks = init_region(Vector2i.ZERO)
	init_world_bubble(playerPos)
	generated.emit(regionChunks)
