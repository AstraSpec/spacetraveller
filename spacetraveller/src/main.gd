extends Node2D

@export var WorldGen :WorldGeneration
@export var Player :Sprite2D

func _ready() -> void:
	Player.interact_cell(Vector2(2899, 2899))
	
	TileDb.initialize_data()
	ChunkDb.initialize_data()
	ItemDb.initialize_data()
	StructureDb.initialize_data()
	
	WorldGen.generate_world(Player.cellPos)
	WorldGen.update_world_bubble(Player.cellPos)
	
	InputManager.current_mode = InputManager.InputMode.GAMEPLAY
