extends Node2D

@export var World :WorldGeneration
@export var Player :Sprite2D

func _ready() -> void:
	Player.cellPos = Vector2(
		World.WORLD_BUBBLE_RADIUS, 
		World.WORLD_BUBBLE_RADIUS)
	
	World.init_world_bubble(Player.cellPos)
