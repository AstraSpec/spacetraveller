extends Node2D

@export var _GameWorld :GameWorld
@export var Player :PlayerController
@export var Canvas :CanvasLayer

var structure_editor_scene = preload("res://src/structure_editor/structure_editor.tscn")
var structure_editor_instance = null

var DEFAULT_ZOOM_LVL :int = 3
var look_focus: Vector2i = Vector2i.ZERO
var _look_mode_active: bool = false

func _ready() -> void:
	RenderingServer.set_default_clear_color(Color.BLACK)

	TileDb.initialize_data()
	TileGroupDb.initialize_data()
	BodyPartDb.initialize_data()
	RaceDb.initialize_data()
	ChunkDb.initialize_data()
	FeatureDb.initialize_data()
	DungeonDb.initialize_data()
	ItemDb.initialize_data()
	LootDb.initialize_data()
	RecipeDb.initialize_data()
	TraversalProfileDb.initialize_data()
	StructureDb.initialize_data()
	StyleDb.initialize_data()
	AbilityDb.initialize_data()
	NameDb.initialize_data()
	FactionDb.initialize_data()
	JobDb.initialize_data()
	SpawnDb.initialize_data()
	EntityGroupDb.initialize_data()
	QuestDb.initialize_data()
	ScenarioDb.initialize_data()

	SaveManager.register_world(_GameWorld)
	var new_game_options := {}
	var new_game_scenario_id := ""
	var new_game_location := {}
	if SaveManager.loaded_save_data.is_empty():
		new_game_options = SaveManager.consume_new_game_options()
		new_game_scenario_id = _resolve_scenario_id(new_game_options)
		new_game_location = ScenarioDb.get_location(new_game_scenario_id)

	_GameWorld.generate_world(Vector2i(2899, 2899), new_game_location)

	if not SaveManager.loaded_save_data.is_empty():
		SaveManager.apply_loaded_data()
		SaveManager.loaded_save_data = {}
	else:
		_initialize_new_game(new_game_scenario_id)

	InputManager.reset_stack(InputManager.InputMode.EXPLORATION)
	InputManager.structure_editor_toggled.connect(_on_structure_editor_toggled)
	InputManager.look_mode_changed.connect(_on_look_mode_changed)
	InputManager.look_directional_input.connect(_on_look_directional_input)
	_initialize_windows()
	QuestService.bind_game_world(_GameWorld)

func _resolve_scenario_id(options: Dictionary = {}) -> String:
	var scenario_id := str(options.get("scenario_id", "")).to_lower()
	if not scenario_id.is_empty() and ScenarioDb.has_scenario(scenario_id):
		return scenario_id
	return ScenarioDb.get_default_scenario_id()

func _initialize_new_game(scenario_id: String):
	if scenario_id.is_empty():
		return
	_apply_scenario_loadout(scenario_id)
	_GameWorld.update_world_bubble(_GameWorld.get_player_position())

func _apply_scenario_loadout(scenario_id: String) -> void:
	var scenario := ScenarioDb.get_scenario(scenario_id)
	for item in scenario.get("items", []):
		_add_scenario_item(item)
	for entry in scenario.get("equipment", []):
		_apply_scenario_equipment(entry)

func _add_scenario_item(item: Variant) -> void:
	if not item is Dictionary:
		return
	var item_id := str(item.get("id", ""))
	var amount := int(item.get("amount", 1))
	if item_id.is_empty() or amount <= 0:
		return
	_GameWorld.add_entity_inventory_item(0, item_id, amount)

func _apply_scenario_equipment(entry: Variant) -> void:
	if not entry is Dictionary:
		return
	var item_id := str(entry.get("id", ""))
	if item_id.is_empty():
		return

	_GameWorld.add_entity_inventory_item(0, item_id, 1)

	var mode := str(entry.get("mode", "wear")).to_lower()
	var equipped := false
	if mode == "wield":
		equipped = _GameWorld.wield_entity_weapon_by_string(0, item_id)
	else:
		equipped = _wear_scenario_item(item_id, entry)

	if equipped:
		_GameWorld.remove_entity_inventory_item(0, item_id, 1)
	else:
		push_warning("Could not equip scenario item '%s'." % item_id)

func _wear_scenario_item(item_id: String, entry: Dictionary) -> bool:
	var part_type := str(entry.get("part", ""))
	var layer := str(entry.get("layer", ""))
	if part_type.is_empty() or layer.is_empty():
		return _GameWorld.equip_entity_clothing_by_string(0, item_id)

	var part_index := _find_anatomy_part_index(part_type, int(entry.get("part_index", 0)))
	if part_index < 0:
		return false
	return _GameWorld.equip_entity_clothing(0, part_index, item_id, layer)

func _find_anatomy_part_index(part_type: String, occurrence: int = 0) -> int:
	var anatomy := _GameWorld.get_entity_anatomy(0)
	var parts: Array = anatomy.get("parts", [])
	var seen := 0
	for i in range(parts.size()):
		if not parts[i] is Dictionary:
			continue
		var part: Dictionary = parts[i]
		if str(part.get("type_id", "")) != part_type:
			continue
		if seen == occurrence:
			return i
		seen += 1
	return -1

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
		_GameWorld.get_renderer().set_show_items(true)
		_GameWorld.get_renderer().set_show_entities(true)
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

		_GameWorld.get_renderer().set_show_items(true)
		_GameWorld.get_renderer().set_show_entities(true)
		_GameWorld.get_renderer().set_occlusion_enabled(true)
		_GameWorld.update_world_bubble(_GameWorld.get_player_position())

func _on_look_mode_changed(active: bool) -> void:
	_look_mode_active = active
	if active:
		if Player and Player.has_method("cancel_navigation"):
			Player.cancel_navigation()
		look_focus = _GameWorld.get_player_position()
		_update_look_view()
	else:
		_GameWorld.update_world_bubble(_GameWorld.get_player_position())

func _on_look_directional_input(direction: Vector2) -> void:
	if not _look_mode_active:
		return
	look_focus += Vector2i(direction)
	_update_look_view()

func _update_look_view() -> void:
	_GameWorld.update_world_view(
		look_focus,
		_GameWorld.get_player_position(),
		false
	)
