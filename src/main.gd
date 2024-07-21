extends Node2D

@export var World :Node2D
@export var Player :Sprite2D

func _ready() -> void:
	World.init_world_bubble()
	Player.position = Vector2(
		World.WORLD_BUBBLE_SIZE / 2 * World.TILE_SIZE, 
		World.WORLD_BUBBLE_SIZE / 2 * World.TILE_SIZE)
