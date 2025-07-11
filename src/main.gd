extends Node2D

@export var World :Node2D
@export var Player :Sprite2D

func _ready() -> void:
	Player.cellPos = Vector2(
		World.WORLD_BUBBLE_SIZE / 2, 
		World.WORLD_BUBBLE_SIZE / 2)
	
	World.init_world_bubble(Player.cellPos)
