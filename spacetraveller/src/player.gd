extends Sprite2D

signal moved_cell(cellPos :Vector2)
signal moved_chunk(chunkPos :Vector2)

@onready var InteractionCell :PackedScene = preload("res://src/interaction_cell.tscn")
@export var Camera :Camera2D
@export var World : GameWorld
@export var PathfindingTimer :Timer
@export var _Inventory: Inventory
@export var _Anatomy :Anatomy
@export var _Clothing :Clothing
@export var _Equipment: EquipmentComponent

const DIR :Array[Vector2] = [Vector2.UP, Vector2.DOWN, Vector2.LEFT, Vector2.RIGHT]

var CHUNK_SIZE = GameWorld.get_chunk_size()

var cellPos : Vector2 = Vector2.ZERO
var chunkPos : Vector2 = Vector2.ZERO

var currentAction: PlayerAction = null
var interactionCells : Array[Node2D] = []
var pathIndicators : Array[Node2D] = []

var available_actions: Array[PlayerAction] = []

var current_path: Array[Vector2i] = []

func _ready():
	InputManager.directional_input.connect(_on_movement_triggered)
	InputManager.action_smash_requested.connect(_on_smash_requested)
	InputManager.action_pickup_requested.connect(_on_pickup_requested)
	InputManager.exploration_right_click.connect(_on_right_click)

	# Register default actions
	available_actions.append(SmashAction.new(self, World))
	available_actions.append(PickupAction.new(self, World))

func _on_right_click(_global_pos: Vector2):
	_clear_path()
	var mouse_local = get_local_mouse_position()
	var cell_diff = (mouse_local / World.get_renderer().get_cell_size()).floor()
	var target_cell = Vector2i(cellPos) + Vector2i(cell_diff)
	
	var path_array = World.request_player_path(Vector2i(cellPos), target_cell)
	if path_array.is_empty():
		return
	
	for p in path_array:
		current_path.append(Vector2i(p))
	
	if not current_path.is_empty() and current_path[0] == Vector2i(cellPos):
		current_path.remove_at(0)
	
	_show_path_indicators()
	World.update_world_bubble(cellPos)
	
	if not current_path.is_empty():
		PathfindingTimer.start()

func _on_pathfinding_timer_timeout() -> void:
	if current_path.is_empty():
		PathfindingTimer.stop()
		_clear_path()
		return
	
	var next_pos: Vector2i = current_path[0]
	var displacement := Vector2(next_pos - Vector2i(cellPos))
	interact_cell(displacement)
	current_path.remove_at(0)
	
	_refresh_path_indicators()
	
	if current_path.is_empty():
		PathfindingTimer.stop()
		_clear_path()

func _show_path_indicators():
	var tile_size = World.get_renderer().get_cell_size()
	for waypoint in current_path:
		var inst = InteractionCell.instantiate()
		inst.position = Vector2(waypoint - Vector2i(cellPos)) * tile_size
		inst.modulate = Color(0.3, 0.6, 1.0, 1.0)
		add_child(inst)
		pathIndicators.append(inst)

func _clear_path():
	current_path.clear()
	for indicator in pathIndicators:
		indicator.queue_free()
	pathIndicators.clear()

func _refresh_path_indicators():
	for indicator in pathIndicators:
		indicator.queue_free()
	pathIndicators.clear()
	_show_path_indicators()

func _clear_interaction_cells():
	for cell in interactionCells:
		cell.queue_free()
	interactionCells.clear()

func _try_set_action(action: PlayerAction):
	_clear_interaction_cells()
	_clear_path()  # Cancel movement when selecting an action
	var tile_size = FastTileMap.get_tile_size()
	var found_valid = false
	
	for dir :Vector2 in DIR:
		var target = Vector2i(cellPos + dir)
		if action.is_valid(target):
			found_valid = true
			var inst = InteractionCell.instantiate()
			inst.position = dir * tile_size
			add_child(inst)
			interactionCells.append(inst)
	
	if found_valid:
		currentAction = action
	else:
		currentAction = null

func _on_movement_triggered(dir: Vector2):
	_clear_path()
	if currentAction:
		var target_cell = Vector2i(cellPos) + Vector2i(dir)
		if currentAction.is_valid(target_cell):
			currentAction.execute(target_cell)
			World.update_world_bubble(cellPos)
			TimeManager.advance_turn()
			World.process_npcs(TimeManager.total_turns, cellPos.x, cellPos.y)
		
		currentAction = null
		_clear_interaction_cells()
	else:
		interact_cell(dir)
		TimeManager.advance_turn()
		World.process_npcs(TimeManager.total_turns, cellPos.x, cellPos.y)

func get_save_data() -> Dictionary:
	return {
		"cellPos": {"x": cellPos.x, "y": cellPos.y},
		"chunkPos": {"x": chunkPos.x, "y": chunkPos.y},
		"anatomy": _Anatomy.get_save_data(),
		"clothing": _Clothing.get_save_data()
	}

func load_save_data(data: Dictionary) -> void:
	cellPos = _load_vector2(data.get("cellPos", cellPos))
	chunkPos = _load_vector2(data.get("chunkPos", chunkPos))
	
	if data.has("anatomy"):
		_Anatomy.load_save_data(data["anatomy"])
	if data.has("clothing"):
		_Clothing.load_save_data(data["clothing"])
		
	moved_cell.emit(cellPos)
	moved_chunk.emit(chunkPos)

func _load_vector2(v) -> Vector2:
	if v is Vector2:
		return v
	if v is Dictionary:
		return Vector2(v.get("x", 0), v.get("y", 0))
	if v is String:
		return str_to_var("Vector2" + v) if not v.begins_with("Vector2") else str_to_var(v)
	return Vector2.ZERO

func interact_cell(displacement: Vector2) -> void:
	cellPos += displacement
	moved_cell.emit(cellPos)
	
	var newChunkPos = Vector2(floor(cellPos.x / CHUNK_SIZE), floor(cellPos.y / CHUNK_SIZE))
	
	if chunkPos != newChunkPos:
		chunkPos = newChunkPos
		moved_chunk.emit(chunkPos)
	
	World.update_world_bubble(cellPos)

func equip_item(item_id: String) -> bool:
	return _Equipment.equip_item(item_id)

func unequip_item(item_id: String) -> bool:
	return _Equipment.unequip_item(item_id)

func follow_path_step() -> bool:
	if current_path.is_empty():
		return false
	
	var next_pos: Vector2i = current_path[0]
	var displacement := Vector2(next_pos - Vector2i(cellPos))
	interact_cell(displacement)
	World.update_world_bubble(cellPos)
	current_path.remove_at(0)
	
	# Refresh indicators so they stay relative to the player
	_refresh_path_indicators()
	
	return not current_path.is_empty()

func _on_smash_requested():
	_try_set_action(SmashAction.new(self, World))

func _on_pickup_requested():
	_try_set_action(PickupAction.new(self, World))
