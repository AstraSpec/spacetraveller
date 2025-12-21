extends WorldGeneration

@export var Tilesheet :Texture2D
@onready var BiomeNoise :FastNoiseLite = preload("res://noise/biome_noise.tres")

var seed_ :int = 0

func _ready() -> void:
	biome_noise = BiomeNoise
	tilesheet = Tilesheet
	world_seed = seed_
