extends Node2D

@export var World :WorldGeneration
@export var Player :Sprite2D

func _ready() -> void:
	var bubble_radius = WorldGeneration.get_bubble_radius()
	Player.cellPos = Vector2(bubble_radius, bubble_radius)
	
	World.init_world_bubble(Vector2i(Player.cellPos))
