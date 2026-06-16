extends Node2D

@export var _GameWorld :GameWorld
@export var Player :Sprite2D
@export var Canvas :CanvasLayer

var structure_editor_scene = preload("res://src/structure_editor/structure_editor.tscn")
var structure_editor_instance = null

var DEFAULT_ZOOM_LVL :int = 3

func _ready() -> void:
	RenderingServer.set_default_clear_color(Color.BLACK)

	TileDb.initialize_data()
	BodyPartDb.initialize_data()
	RaceDb.initialize_data()
	ChunkDb.initialize_data()
	FeatureDb.initialize_data()
	DungeonDb.initialize_data()
	ItemDb.initialize_data()
	LootDb.initialize_data()
	RecipeDb.initialize_data()
	AttitudeDb.initialize_data()
	TraversalProfileDb.initialize_data()
	StructureDb.initialize_data()
	StyleDb.initialize_data()
	AbilityDb.initialize_data()
	NameDb.initialize_data()
	JobDb.initialize_data()
	SpawnDb.initialize_data()
	EntityGroupDb.initialize_data()
	QuestDb.initialize_data()

	SaveManager.register_world(_GameWorld)
	_GameWorld.generate_world(Vector2i(2899, 2899))

	if not SaveManager.loaded_save_data.is_empty():
		SaveManager.apply_loaded_data()
		SaveManager.loaded_save_data = {}
	else:
		_initialize_new_game()

	InputManager.reset_stack(InputManager.InputMode.EXPLORATION)
	InputManager.structure_editor_toggled.connect(_on_structure_editor_toggled)
	_initialize_windows()
	QuestService.bind_game_world(_GameWorld)

func _initialize_new_game():
	_GameWorld.add_entity_inventory_item(0, "stick", 5)
	_GameWorld.add_entity_inventory_item(0, "rope", 2)
	_GameWorld.add_entity_inventory_item(0, "flint", 1)
	_GameWorld.add_entity_inventory_item(0, "spider_silk", 4)
	_GameWorld.add_entity_inventory_item(0, "bone", 1)
	_GameWorld.add_entity_inventory_item(0, "iron_shard", 1)
	_GameWorld.add_entity_inventory_item(0, "longsword", 1)
	_GameWorld.add_entity_inventory_item(0, "torch", 2)
	_GameWorld.add_entity_inventory_item(0, "fire_starter", 1)
	_GameWorld.add_entity_inventory_item(0, "bone_needle", 1)

	_GameWorld.add_entity_inventory_item(0, "bra_wool", 1)
	_GameWorld.add_entity_inventory_item(0, "panties_wool", 1)
	_GameWorld.add_entity_inventory_item(0, "linen_shirt", 1)
	_GameWorld.add_entity_inventory_item(0, "linen_trousers", 1)
	_GameWorld.add_entity_inventory_item(0, "silver_earrings", 2)
	_GameWorld.add_entity_inventory_item(0, "gold_ring", 1)

	var anatomy = _GameWorld.get_entity_anatomy(0)
	
	var torso_idx = -1
	var leg_idx = -1
	var ear_idx_1 = -1
	var ear_idx_2 = -1
	var finger_idx = -1

	var parts = anatomy.get("parts", [])
	for i in range(parts.size()):
		var part = parts[i]
		var type_id = part.get("type_id", "")
		#var local_index = part.get("local_index", 0)

		if type_id == "torso" and torso_idx == -1:
			torso_idx = i
		elif type_id == "leg" and leg_idx == -1:
			leg_idx = i
		elif type_id == "ear":
			if ear_idx_1 == -1:
				ear_idx_1 = i
			elif ear_idx_2 == -1:
				ear_idx_2 = i
		elif type_id == "finger" and finger_idx == -1:
			finger_idx = i
	
	if torso_idx != -1:
		if _GameWorld.equip_entity_clothing(0, torso_idx, "bra_wool", "under"):
			_GameWorld.remove_entity_inventory_item(0, "bra_wool", 1)
		if _GameWorld.equip_entity_clothing(0, torso_idx, "panties_wool", "under"):
			_GameWorld.remove_entity_inventory_item(0, "panties_wool", 1)
		if _GameWorld.equip_entity_clothing(0, torso_idx, "linen_shirt", "outer"):
			_GameWorld.remove_entity_inventory_item(0, "linen_shirt", 1)

	if leg_idx != -1:
		if _GameWorld.equip_entity_clothing(0, leg_idx, "linen_trousers", "outer"):
			_GameWorld.remove_entity_inventory_item(0, "linen_trousers", 1)

	if ear_idx_1 != -1:
		if _GameWorld.equip_entity_clothing(0, ear_idx_1, "silver_earrings", "accessory"):
			_GameWorld.remove_entity_inventory_item(0, "silver_earrings", 1)
	if ear_idx_2 != -1:
		if _GameWorld.equip_entity_clothing(0, ear_idx_2, "silver_earrings", "accessory"):
			_GameWorld.remove_entity_inventory_item(0, "silver_earrings", 1)

	if finger_idx != -1:
		if _GameWorld.equip_entity_clothing(0, finger_idx, "gold_ring", "accessory"):
			_GameWorld.remove_entity_inventory_item(0, "gold_ring", 1)


	_GameWorld.update_world_bubble(_GameWorld.get_player_position())

func _initialize_windows():
	for window in Canvas.get_node("Window").get_children():
		if window is BaseWindow:
			window.Player = Player

func _on_structure_editor_toggled(active: bool):
	if active:
		structure_editor_instance = structure_editor_scene.instantiate()
		add_child(structure_editor_instance)
		structure_editor_instance.World = _GameWorld
		structure_editor_instance.FastTilemap = _GameWorld.get_renderer()
		_GameWorld.get_renderer().set_occlusion_enabled(false)
		structure_editor_instance.spacing = 1
		structure_editor_instance.start_editor(_GameWorld.get_player_position())
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

		_GameWorld.get_renderer().set_occlusion_enabled(true)
		_GameWorld.update_world_bubble(_GameWorld.get_player_position())
