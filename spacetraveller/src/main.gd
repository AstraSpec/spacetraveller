extends Node2D

@export var WorldGen :WorldGeneration
@export var Player :Sprite2D

func _ready() -> void:
	var bubble_radius = WorldGen.get_world_bubble_radius()
	Player.interact_cell(Vector2(3900, 3900))
	
	TileDb.initialize_data()
	ChunkDb.initialize_data()
	StructureDb.initialize_data()
	
	WorldGen.generate_world(Player.cellPos)
	WorldGen.update_world_bubble(Player.cellPos)
