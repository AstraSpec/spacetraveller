extends Node2D

@export var World :WorldGeneration
@export var Player :Sprite2D

func _ready() -> void:
	var bubble_radius = WorldGeneration.get_bubble_radius()
	Player.cellPos = Vector2(bubble_radius, bubble_radius)
	
	TileDb.initialize_data()
	ChunkDb.initialize_data()
	
	World.generate_world(Player.cellPos)
