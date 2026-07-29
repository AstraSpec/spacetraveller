extends Node2D
class_name PlayerController

signal moved_cell(cellPos :Vector2)
signal moved_chunk(chunkPos :Vector2)

const TARGETING_INFO_OWNER := "targeting"

@export var Camera :Camera2D
@export var World : GameWorld
@export var PathfindingTimer :Timer
@export var _ConfirmationPopup :Window

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
	InputManager.action_close_requested.connect(_on_close_requested)
	InputManager.action_examine_requested.connect(_on_examine_requested)
	InputManager.action_ascend_requested.connect(_on_ascend_requested)
	InputManager.action_descend_requested.connect(_on_descend_requested)
	InputManager.toggle_run_requested.connect(_on_toggle_run_requested)
	InputManager.exploration_right_click.connect(_on_right_click)
	InputManager.menu_toggled.connect(_on_menu_toggled)

	available_actions.append(SmashAction.new(self, World))
	available_actions.append(PickupAction.new(self, World))
	available_actions.append(CloseAction.new(self, World))
	available_actions.append(ExamineAction.new(self, World))
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
		_check_tile_metadata(new_pos)
		_check_ground_items(new_pos)

func _on_right_click(_global_pos: Vector2):
	var overlays_changed := _clear_path(false)
	overlays_changed = _clear_targeting(false) or overlays_changed
	var mouse_local = get_local_mouse_position()
	var cell_diff = (mouse_local / World.get_renderer().get_cell_size()).floor()
	var target_cell = Vector2i(cellPos()) + Vector2i(cell_diff)

	var path_array = World.request_player_path(Vector2i(cellPos()), target_cell)
	if path_array.is_empty():
		if overlays_changed:
			World.update_world_bubble(cellPos())
		return

	for p in path_array:
		current_path.append(Vector2i(p))

	if not current_path.is_empty() and current_path[0] == Vector2i(cellPos()):
		current_path.remove_at(0)

	overlays_changed = _show_path_indicators(false) or overlays_changed
	if overlays_changed:
		World.update_world_bubble(cellPos())

	if not current_path.is_empty():
		PathfindingTimer.start()

func cancel_navigation() -> void:
	if PathfindingTimer:
		PathfindingTimer.stop()
	var overlays_changed := _clear_path(false)
	overlays_changed = _clear_targeting(false) or overlays_changed
	if overlays_changed:
		World.update_world_bubble(cellPos())

func _on_pathfinding_timer_timeout() -> void:
	if current_path.is_empty():
		PathfindingTimer.stop()
		_clear_path()
		return

	var next_pos: Vector2i = current_path[0]
	var displacement := Vector2(next_pos - Vector2i(cellPos()))
	if not _submit_move(Vector2i(displacement)):
		return
	current_path.remove_at(0)

	_refresh_path_indicators()

	if current_path.is_empty():
		PathfindingTimer.stop()
		_clear_path()

func _show_path_indicators(refresh_visuals: bool = true) -> bool:
	var changed := false
	for i in range(current_path.size()):
		var waypoint = current_path[i]
		var is_endpoint = (i == current_path.size() - 1)
		var atlas_x = 17 if is_endpoint else 18
		World.add_overlay(waypoint.x, waypoint.y, atlas_x, 0, Color(1.0, 1.0, 1.0, 1.0), -1.0)
		pathIndicators.append(waypoint)
		changed = true
	if changed and refresh_visuals:
		World.update_world_bubble(cellPos())
	return changed

func _clear_path(refresh_visuals: bool = true) -> bool:
	current_path.clear()
	return _clear_path_overlays(refresh_visuals)

func _clear_path_overlays(refresh_visuals: bool = true) -> bool:
	if pathIndicators.is_empty():
		return false
	for indicator in pathIndicators:
		World.remove_overlay(indicator.x, indicator.y)
	pathIndicators.clear()
	if refresh_visuals:
		World.update_world_bubble(cellPos())
	return true

func _refresh_path_indicators():
	var changed := _clear_path_overlays(false)
	changed = _show_path_indicators(false) or changed
	if changed:
		World.update_world_bubble(cellPos())

func _clear_interaction_cells(refresh_visuals: bool = true) -> bool:
	if interactionCells.is_empty():
		return false
	for cell in interactionCells:
		World.remove_overlay(cell.x, cell.y)
	interactionCells.clear()
	if refresh_visuals:
		World.update_world_bubble(cellPos())
	return true

func _clear_targeting(refresh_visuals: bool = true) -> bool:
	currentAction = null
	InformationPanel.hide_info(TARGETING_INFO_OWNER)
	return _clear_interaction_cells(refresh_visuals)

func _try_set_action(action: PlayerAction):
	_clear_targeting()
	_clear_path()
	var valid_cells: Array[Vector2i] = []

	for _offset: Vector2i in action.get_target_offsets():
		var target = Vector2i(cellPos()) + _offset
		if action.is_valid(target):
			valid_cells.append(target)

	if action.auto and valid_cells.size() == 1:
		action.execute(valid_cells[0])
		currentAction = null
	elif not valid_cells.is_empty():
		for target in valid_cells:
			World.add_overlay(target.x, target.y, 20, 0, Color(1.0, 1.0, 1.0, 1.0), -1.0)
			interactionCells.append(target)
		World.update_world_bubble(cellPos())
		currentAction = action
		InformationPanel.show_info(action.get_target_prompt(), TARGETING_INFO_OWNER)
	else:
		currentAction = null

func _on_movement_triggered(dir: Vector2):
	_clear_path()
	if currentAction:
		var target_cell = Vector2i(cellPos()) + Vector2i(dir)
		if currentAction.is_valid(target_cell):
			if currentAction is AttackAction:
				World.submit_player_intent(World.INTENT_ATTACK, target_cell.x, target_cell.y, "")
			else:
				currentAction.execute(target_cell)

		_clear_targeting()
	else:
		if dir == Vector2.ZERO:
			World.submit_player_intent(World.INTENT_NONE, 0, 0, "")
		else:
			_submit_move(dir)

func _submit_move(dir: Vector2) -> bool:
	var pos = Vector2i(cellPos())
	var target = pos + Vector2i(dir)
	if World.would_player_move_fall(target.x, target.y):
		PathfindingTimer.stop()
		_clear_path()
		_ConfirmationPopup.show_confirm(
			"Jump off this ledge?",
			[
				{"label": "No"},
				{"label": "Yes", "callback": Callable(self, "_confirm_ledge_jump").bind(target)},
			]
		)
		return false
	return World.submit_player_intent(World.INTENT_MOVE, target.x, target.y, "") > 0.0

func _confirm_ledge_jump(target: Vector2i) -> void:
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

func _on_close_requested():
	_try_set_action(CloseAction.new(self, World))

func _on_examine_requested():
	_try_set_action(ExamineAction.new(self, World))

func _on_ascend_requested():
	_try_change_z(1)

func _on_descend_requested():
	_try_change_z(-1)

func set_movement_mode(mode_id: String) -> bool:
	var changed := World.set_player_movement_mode(mode_id)
	if not changed:
		EventBus.post("movement_warning", "You are too exhausted to run.", {"mode": mode_id})
	return changed

func _on_toggle_run_requested() -> void:
	if not World.toggle_player_run():
		EventBus.post("movement_warning", "You are too exhausted to run.", {"mode": "run"})

func _try_change_z(delta: int) -> void:
	_clear_targeting()
	_clear_path()
	var old_z := World.get_player_z()
	var cost := World.submit_player_change_z(delta)
	if cost > 0.0:
		var pos := Vector2i(cellPos())
		EventBus.post("movement", "You go up." if delta > 0 else "You go down.", {"z": World.get_player_z(), "old_z": old_z})
		_check_tile_metadata(pos)
		_check_ground_items(pos)
		return

	if delta > 0:
		EventBus.post("movement", "You cannot go up here.", {"z": old_z})
	else:
		EventBus.post("movement", "You cannot go down here.", {"z": old_z})

func _on_menu_toggled(_id: String, is_open: bool, _params: Dictionary) -> void:
	if is_open:
		_clear_targeting()

func _check_ground_items(pos: Vector2i) -> void:
	var items = World.get_items_at(pos)
	if items.is_empty():
		return
	var top_id = str(items[0].get("id", ""))
	var top_name = ItemDb.get_item_name(top_id) if not top_id.is_empty() else "something"
	var msg = "You see %s here." % top_name
	if items.size() > 1:
		msg += " And others."
	EventBus.post("ground", msg, {"pos": pos, "items": items})

func _check_tile_metadata(pos: Vector2i) -> void:
	var metadata: Dictionary = World.get_tile_metadata(pos)
	if metadata.is_empty():
		return

	var text := _metadata_text(metadata)
	if text.is_empty():
		return

	EventBus.post("metadata", "The text here says \"%s\"." % text, {"pos": pos, "metadata": metadata})

func _metadata_text(metadata: Dictionary) -> String:
	var data = metadata.get("data", {})
	if data is Dictionary:
		var nested_text := str(data.get("text", ""))
		if not nested_text.is_empty():
			return nested_text

	return ""
