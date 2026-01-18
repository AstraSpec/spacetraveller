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
	
	# Test Inventory UI
	var inventory = get_node("Inventory")
	if inventory:
		inventory.add_item("debug_item_1", 5)
		inventory.add_item("debug_item_2", 10)
		inventory.add_item("debug_item_3", 2)
		inventory.add_item("debug_item_4", 5)
		inventory.add_item("debug_item_5", 6)
		inventory.add_item("debug_item_6", 7)
