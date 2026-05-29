extends Sprite2D

signal moved_cell(cellPos :Vector2)
signal moved_chunk(chunkPos :Vector2)

@export var Camera :Camera2D
@export var World : GameWorld
@export var PathfindingTimer :Timer

const DIR :Array[Vector2] = [Vector2.UP, Vector2.DOWN, Vector2.LEFT, Vector2.RIGHT]

var CHUNK_SIZE = GameWorld.get_chunk_size()

var currentAction: PlayerAction = null
var interactionCells : Array[Vector2i] = []
var pathIndicators : Array[Vector2i] = []

var available_actions: Array[PlayerAction] = []

var current_path: Array[Vector2i] = []

func _ready():
	InputManager.directional_input.connect(_on_movement_triggered)
	InputManager.action_smash_requested.connect(_on_smash_requested)
	InputManager.action_pickup_requested.connect(_on_pickup_requested)
	InputManager.exploration_right_click.connect(_on_right_click)

	available_actions.append(SmashAction.new(self, World))
	available_actions.append(PickupAction.new(self, World))
	available_actions.append(AttackAction.new(self, World))

	World.entity_moved.connect(_on_entity_moved)

func cellPos() -> Vector2:
	return World.get_player_position()

func chunkPos() -> Vector2:
	return World.get_player_chunk()

func _on_entity_moved(entity_id: int, new_pos: Vector2i, new_chunk: Vector2i):
	if entity_id == 0:
		moved_cell.emit(Vector2(new_pos))
		moved_chunk.emit(Vector2(new_chunk))
		World.update_world_bubble(new_pos)

func _on_right_click(_global_pos: Vector2):
	_clear_path()
	var mouse_local = get_local_mouse_position()
	var cell_diff = (mouse_local / World.get_renderer().get_cell_size()).floor()
	var target_cell = Vector2i(cellPos()) + Vector2i(cell_diff)

	var path_array = World.request_player_path(Vector2i(cellPos()), target_cell)
	if path_array.is_empty():
		return

	for p in path_array:
		current_path.append(Vector2i(p))

	if not current_path.is_empty() and current_path[0] == Vector2i(cellPos()):
		current_path.remove_at(0)

	_show_path_indicators()
	World.update_world_bubble(cellPos())

	if not current_path.is_empty():
		PathfindingTimer.start()

func _on_pathfinding_timer_timeout() -> void:
	if current_path.is_empty():
		PathfindingTimer.stop()
		_clear_path()
		return

	var next_pos: Vector2i = current_path[0]
	var displacement := Vector2(next_pos - Vector2i(cellPos()))
	_submit_move(Vector2i(displacement))
	current_path.remove_at(0)

	_refresh_path_indicators()

	if current_path.is_empty():
		PathfindingTimer.stop()
		_clear_path()

func _show_path_indicators():
	for i in range(current_path.size()):
		var waypoint = current_path[i]
		var is_endpoint = (i == current_path.size() - 1)
		var atlas_x = 17 if is_endpoint else 18
		World.add_overlay(waypoint.x, waypoint.y, atlas_x, 0, Color(1.0, 1.0, 1.0, 1.0), -1.0)
		pathIndicators.append(waypoint)
	World.update_world_bubble(cellPos())

func _clear_path():
	current_path.clear()
	_clear_path_overlays()

func _clear_path_overlays():
	for indicator in pathIndicators:
		World.remove_overlay(indicator.x, indicator.y)
	pathIndicators.clear()
	World.update_world_bubble(cellPos())

func _refresh_path_indicators():
	_clear_path_overlays()
	_show_path_indicators()

func _clear_interaction_cells():
	for cell in interactionCells:
		World.remove_overlay(cell.x, cell.y)
	interactionCells.clear()
	World.update_world_bubble(cellPos())

func _try_set_action(action: PlayerAction):
	_clear_interaction_cells()
	_clear_path()
	var found_valid = false

	for dir :Vector2 in DIR:
		var target = Vector2i(cellPos() + dir)
		if action.is_valid(target):
			found_valid = true
			World.add_overlay(target.x, target.y, 20, 0, Color(1.0, 1.0, 1.0, 1.0), -1.0)
			interactionCells.append(target)

	if found_valid:
		World.update_world_bubble(cellPos())
		currentAction = action
	else:
		currentAction = null

func _on_movement_triggered(dir: Vector2):
	_clear_path()
	if currentAction:
		var target_cell = Vector2i(cellPos()) + Vector2i(dir)
		if currentAction.is_valid(target_cell):
			if currentAction is SmashAction:
				World.submit_player_intent(World.INTENT_SMASH, target_cell.x, target_cell.y, "")
			elif currentAction is PickupAction:
				currentAction.execute(target_cell)
			elif currentAction is AttackAction:
				World.submit_player_intent(World.INTENT_ATTACK, target_cell.x, target_cell.y, "")
			World.update_world_bubble(cellPos())

		currentAction = null
		_clear_interaction_cells()
	else:
		if dir == Vector2.ZERO:
			var pos = Vector2i(cellPos())
			World.submit_player_intent(World.INTENT_PICKUP, int(pos.x), int(pos.y), "")
			World.update_world_bubble(cellPos())
		else:
			_submit_move(dir)
			World.update_world_bubble(cellPos())

func _submit_move(dir: Vector2):
	var pos = Vector2i(cellPos())
	var target = pos + Vector2i(dir)
	World.submit_player_intent(World.INTENT_MOVE, target.x, target.y, "")

func _load_vector2(v) -> Vector2:
	if v is Vector2:
		return v
	if v is Dictionary:
		return Vector2(v.get("x", 0), v.get("y", 0))
	if v is String:
		return str_to_var("Vector2" + v) if not v.begins_with("Vector2") else str_to_var(v)
	return Vector2.ZERO

func _on_smash_requested():
	_try_set_action(SmashAction.new(self, World))

func _on_pickup_requested():
	_try_set_action(PickupAction.new(self, World))
