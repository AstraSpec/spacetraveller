extends Node2D

@export var WorldGen :GameWorld
@export var Player :Sprite2D
@export var Canvas :CanvasLayer

var structure_editor_scene = preload("res://src/structure_editor/structure_editor.tscn")
var structure_editor_instance = null

var DEFAULT_ZOOM_LVL :int = 3

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
	
	SaveManager.register_world(WorldGen)
	SaveManager.register_player(Player)
	SaveManager.register_inventory(Player._Inventory)
	WorldGen.generate_world(Vector2i(Player.cellPos))
	
	if not SaveManager.loaded_save_data.is_empty():
		SaveManager.apply_loaded_data()
		SaveManager.loaded_save_data = {} # Clear memory
	else:
		_initialize_new_game()

	InputManager.reset_stack(InputManager.InputMode.EXPLORATION)
	InputManager.structure_editor_toggled.connect(_on_structure_editor_toggled)
	_initialize_windows()

func _initialize_new_game():
	# DEBUG
	var _Anatomy = Player._Anatomy
	var _Clothing = Player._Clothing
	_Anatomy.initialize_from_race("human")
	
	var _Inventory = Player._Inventory
	_Inventory.add_item("stick", 5)
	_Inventory.add_item("rope", 2)
	_Inventory.add_item("flint", 1)
	_Inventory.add_item("spider_silk", 4)
	_Inventory.add_item("bone", 1)
	_Inventory.add_item("iron_shard", 1)
	_Inventory.add_item("wooden_sword", 1)
	_Inventory.add_item("torch", 2)
	_Inventory.add_item("fire_starter", 1)
	_Inventory.add_item("bone_needle", 1)
	
	_Inventory.add_item("bra_wool", 1)
	_Inventory.add_item("panties_wool", 1)
	_Inventory.add_item("linen_shirt", 1)
	_Inventory.add_item("linen_trousers", 1)
	_Inventory.add_item("silver_earrings", 2)
	_Inventory.add_item("gold_ring", 1)
		
	var torso_idx = _Anatomy.find_part_of_type("torso")
	if torso_idx != -1:
		if _Clothing.equip_item("bra_wool", torso_idx): _Inventory.remove_item("bra_wool", 1)
		if _Clothing.equip_item("panties_wool", torso_idx): _Inventory.remove_item("panties_wool", 1)
		if _Clothing.equip_item("linen_shirt", torso_idx): _Inventory.remove_item("linen_shirt", 1)
		
	var leg_idx = _Anatomy.find_part_of_type("leg")
	if leg_idx != -1:
		if _Clothing.equip_item("linen_trousers", leg_idx): _Inventory.remove_item("linen_trousers", 1)
		
	var ear_idx_1 = _Anatomy.find_part_of_type("ear", 0)
	var ear_idx_2 = _Anatomy.find_part_of_type("ear", 1)
	if ear_idx_1 != -1 and _Clothing.equip_item("silver_earrings", ear_idx_1): _Inventory.remove_item("silver_earrings", 1)
	if ear_idx_2 != -1 and _Clothing.equip_item("silver_earrings", ear_idx_2): _Inventory.remove_item("silver_earrings", 1)
	
	var finger_idx = _Anatomy.find_part_of_type("finger")
	if finger_idx != -1:
		if _Clothing.equip_item("gold_ring", finger_idx): _Inventory.remove_item("gold_ring", 1)

	WorldGen.update_world_bubble(Player.cellPos)

func _initialize_windows():
	for window in Canvas.get_node("Window").get_children():
		if window is BaseWindow:
			window.Player = Player

func _on_structure_editor_toggled(active: bool):
	if active:
		structure_editor_instance = structure_editor_scene.instantiate()
		add_child(structure_editor_instance)
		structure_editor_instance.World = WorldGen
		structure_editor_instance.FastTilemap = WorldGen.get_renderer()
		WorldGen.get_renderer().set_occlusion_enabled(false)
		structure_editor_instance.spacing = 1
		structure_editor_instance.start_editor(Player.cellPos)
		Player.Camera.locked = false
		Canvas.visible = false
	else:
		structure_editor_instance.queue_free()
		structure_editor_instance = null
		Player.Camera.zoomID = DEFAULT_ZOOM_LVL-1
		Player.Camera._view_zoomed(1)
		Player.Camera._view_centered()
		Player.Camera.locked = true
		Canvas.visible = true
		
		WorldGen.get_renderer().set_occlusion_enabled(true)
		WorldGen.update_world_bubble(Player.cellPos)
