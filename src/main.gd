extends Node2D

@export var World :Node2D
@export var Camera :Camera2D

func _ready() -> void:
	World.init_world_bubble()
	Camera.position = Vector2(
		World.WORLD_BUBBLE_SIZE / 2 * World.TILE_SIZE + World.TILE_SIZE / 2, 
		World.WORLD_BUBBLE_SIZE / 2 * World.TILE_SIZE + World.TILE_SIZE / 2)
