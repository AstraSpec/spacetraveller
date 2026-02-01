extends Node2D

@export var WorldGen :WorldGeneration
@export var Player :Sprite2D

func _ready() -> void:
	RenderingServer.set_default_clear_color(Color.BLACK)
	Player.interact_cell(Vector2(2899, 2899))
	
	TileDb.initialize_data()
	BodyPartDb.initialize_data()
	RaceDb.initialize_data()
	ChunkDb.initialize_data()
	ItemDb.initialize_data()
	RecipeDb.initialize_data()
	StructureDb.initialize_data()
	
	# DEBUG
	var anatomy = Player.get_node("Anatomy")
	var clothing = Player.get_node("Clothing")
	anatomy.initialize_from_race("human")
	
	var inventory = get_node("Inventory")
	if inventory:
		# Initial Debug items
		inventory.add_item("debug_item_1", 5)
		inventory.add_item("debug_item_2", 10)
		inventory.add_item("debug_item_3", 2)
		inventory.add_item("debug_item_4", 5)
		inventory.add_item("debug_item_5", 6)
		inventory.add_item("debug_item_6", 7)
		
		# Female Starter Set
		inventory.add_item("bra_wool", 1)
		inventory.add_item("panties_wool", 1)
		inventory.add_item("linen_shirt", 1)
		inventory.add_item("linen_trousers", 1)
		inventory.add_item("silver_earrings", 2)
		inventory.add_item("gold_ring", 1)
		
		# Equip Starter Set
		var torso_idx = anatomy.find_part_of_type("torso")
		if torso_idx != -1:
			if clothing.equip_item("bra_wool", torso_idx): inventory.remove_item("bra_wool", 1)
			if clothing.equip_item("panties_wool", torso_idx): inventory.remove_item("panties_wool", 1)
			if clothing.equip_item("linen_shirt", torso_idx): inventory.remove_item("linen_shirt", 1)
			
		var leg_idx = anatomy.find_part_of_type("leg")
		if leg_idx != -1:
			if clothing.equip_item("linen_trousers", leg_idx): inventory.remove_item("linen_trousers", 1)
			
		var ear_idx_1 = anatomy.find_part_of_type("ear", 0)
		var ear_idx_2 = anatomy.find_part_of_type("ear", 1)
		if ear_idx_1 != -1 and clothing.equip_item("silver_earrings", ear_idx_1): inventory.remove_item("silver_earrings", 1)
		if ear_idx_2 != -1 and clothing.equip_item("silver_earrings", ear_idx_2): inventory.remove_item("silver_earrings", 1)
		
		var finger_idx = anatomy.find_part_of_type("finger")
		if finger_idx != -1:
			if clothing.equip_item("gold_ring", finger_idx): inventory.remove_item("gold_ring", 1)

	WorldGen.generate_world(Player.cellPos)
	WorldGen.update_world_bubble(Player.cellPos)
