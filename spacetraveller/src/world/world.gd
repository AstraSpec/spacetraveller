extends WorldGeneration

@export var Tilesheet :Texture2D
@export var Player :Sprite2D
@onready var BiomeNoise :FastNoiseLite = preload("res://noise/biome_noise.tres")

signal generated(regionChunks)

var seed_ :int = randi()

func _ready() -> void:
	biome_noise = BiomeNoise
	tilesheet = Tilesheet
	world_seed = seed_
	
	InputManager.inventory_item_dropped.connect(_on_inventory_item_dropped)

func _on_inventory_item_dropped(ID: String, amount: int) -> void:
	drop_item(Vector2i(Player.cellPos), ID, amount)
	update_world_bubble(Player.cellPos)

func generate_world(playerPos :Vector2i) -> void:
	var regionChunks = init_region(Vector2i.ZERO)
	init_world_bubble(playerPos)
	generated.emit(regionChunks)
