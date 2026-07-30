extends GameWorld

const ACTIVITY_INFO_OWNER := "activity"

@export var Tilesheet :Texture2D
@export var Player :PlayerController
@export var ActivityConfirmation :ConfirmationPopup
@onready var BiomeNoise :FastNoiseLite = preload("res://noise/biome_noise.tres")

signal generated(regionChunks)

var seed_ :int = randi()

func _ready() -> void:
	setup_renderer()
	biome_noise = BiomeNoise
	get_renderer().tilesheet = Tilesheet
	world_seed = seed_

	InputManager.activity_cancel_requested.connect(_on_activity_cancel_requested)
	player_action_resolved.connect(_on_player_action_resolved)
	player_action_failed.connect(_on_player_action_failed)
	player_activity_started.connect(_on_player_activity_started)
	player_activity_checkpoint.connect(_on_player_activity_checkpoint)
	player_activity_interrupted.connect(_on_player_activity_interrupted)
	player_activity_completed.connect(_on_player_activity_completed)
	player_activity_cancelled.connect(_on_player_activity_cancelled)
	player_movement_mode_changed.connect(_on_player_movement_mode_changed)
	TimeManager.turn_passed.connect(_on_time_turn_passed)

func _process(_delta: float) -> void:
	if InputManager.current_mode == InputManager.InputMode.ACTIVITY and has_player_activity():
		var activity := get_player_activity()
		if str(activity.get("state", "")) == "running":
			process_player_activity_batch()

func _on_player_action_resolved(_entity_id: int, cost: float, _next_turn_time: float) -> void:
	TimeManager.advance_time(cost)
	update_world_bubble(get_player_position())

func _on_player_action_failed(failure_id: String) -> void:
	if failure_id == "no_locomotion":
		EventBus.post("movement", "Your legs cannot support you.")
	elif failure_id == "downed":
		EventBus.post("movement", "You are too badly dazed to do that.")

func _on_player_movement_mode_changed(mode_id: String, reason: String) -> void:
	if reason == "downed":
		return
	var message := ""
	if reason == "exhausted":
		message = "You are too exhausted to keep running."
	elif mode_id == "run":
		message = "You start running."
	elif mode_id == "prone":
		message = "You lie down."
	else:
		message = "You start walking."
	EventBus.post("movement", message, {"mode": mode_id, "reason": reason})

func _on_player_activity_started(activity: Dictionary) -> void:
	_enter_activity_mode()
	InformationPanel.show_info(_activity_information_text(activity), ACTIVITY_INFO_OWNER)
	if str(activity.get("type", "")) == "crafting" and not bool(activity.get("restored", false)):
		var recipe_name := String(RecipeDb.get_recipe_name(str(activity.get("subject_id", ""))))
		EventBus.post("inventory", "You begin crafting %s." % recipe_name, activity)

func _on_player_activity_checkpoint(activity: Dictionary) -> void:
	_refresh_after_activity_time(activity)

func _on_player_activity_interrupted(activity: Dictionary) -> void:
	_refresh_after_activity_time(activity)
	var interruption_id := str(activity.get("pending_interruption", ""))
	if interruption_id == "attacked":
		_show_attack_interruption(activity)
	elif interruption_id == "manual_cancel":
		_show_cancel_interruption(activity)
	else:
		_show_generic_interruption(activity)

func _on_player_activity_completed(activity: Dictionary) -> void:
	InformationPanel.hide_info(ACTIVITY_INFO_OWNER)
	_refresh_after_activity_time(activity)
	var recipe_id := str(activity.get("subject_id", ""))
	var labels: Array[String] = []
	for result in activity.get("results", []):
		var item_id := str(result.get("item_id", ""))
		var amount := int(result.get("amount", 0))
		var item_name := String(ItemDb.get_item_name(item_id))
		var label := item_name if not item_name.is_empty() else item_id
		labels.append(label if amount <= 1 else "%s x%d" % [label, amount])
	EventBus.post("inventory", "You craft %s." % ", ".join(labels), {"recipe_id": recipe_id})
	_finish_activity_ui(activity)

func _on_player_activity_cancelled(activity: Dictionary) -> void:
	InformationPanel.hide_info(ACTIVITY_INFO_OWNER)
	_refresh_after_activity_time(activity)
	var reason := str(activity.get("reason", ""))
	if reason == "requirements_missing":
		EventBus.post("inventory_warning", "Crafting stopped because the required ingredients are no longer available.", activity)
	elif reason != "death":
		EventBus.post("inventory", "You stop crafting.", activity)
	_finish_activity_ui(activity)

func _refresh_after_activity_time(activity: Dictionary) -> void:
	TimeManager.advance_time(float(activity.get("elapsed", 0.0)))
	update_world_bubble(get_player_position())

func _enter_activity_mode() -> void:
	if InputManager.current_mode == InputManager.InputMode.MENU:
		InputManager.pop_mode()
	if InputManager.current_mode != InputManager.InputMode.ACTIVITY:
		InputManager.push_mode(InputManager.InputMode.ACTIVITY)

func _activity_information_text(activity: Dictionary) -> String:
	var activity_type := str(activity.get("type", ""))
	if activity_type == "crafting":
		var recipe_name := String(RecipeDb.get_recipe_name(str(activity.get("subject_id", ""))))
		if not recipe_name.is_empty():
			return "Crafting %s…" % recipe_name

	var activity_name := activity_type.replace("_", " ").strip_edges().capitalize()
	if activity_name.is_empty():
		activity_name = "Working"
	return "%s…" % activity_name

func _finish_activity_ui(activity: Dictionary) -> void:
	if InputManager.current_mode == InputManager.InputMode.ACTIVITY:
		InputManager.pop_mode()
	if bool(activity.get("restore_menu", true)):
		InputManager.toggle_menu(
			str(activity.get("return_menu", "inventory")),
			{"tab": str(activity.get("return_tab", "crafting"))}
		)

func _on_activity_cancel_requested() -> void:
	request_player_activity_cancel()

func _interruption_prompt(activity: Dictionary) -> String:
	var definition = activity.get("interruption", {})
	if definition is Dictionary:
		return str(definition.get("prompt", "Activity interrupted."))
	return "Activity interrupted."

func _show_cancel_interruption(activity: Dictionary) -> void:
	if not ActivityConfirmation:
		resolve_player_activity_interruption("continue")
		return
	ActivityConfirmation.show_confirm(
		_interruption_prompt(activity),
		[
			{"label": "Cancel craft", "callback": resolve_player_activity_interruption.bind("stop")},
			{"label": "Keep crafting", "callback": resolve_player_activity_interruption.bind("continue")},
		]
	)

func _show_attack_interruption(activity: Dictionary) -> void:
	if not ActivityConfirmation:
		resolve_player_activity_interruption("continue")
		return
	ActivityConfirmation.show_confirm(
		_interruption_prompt(activity),
		[
			{"label": "Stop crafting", "callback": resolve_player_activity_interruption.bind("stop")},
			{"label": "Continue", "callback": resolve_player_activity_interruption.bind("continue")},
			{"label": "Continue and ignore attacks", "callback": resolve_player_activity_interruption.bind("ignore")},
		]
	)

func _show_generic_interruption(activity: Dictionary) -> void:
	if not ActivityConfirmation:
		resolve_player_activity_interruption("continue")
		return
	var actions: Array = [
		{"label": "Stop", "callback": resolve_player_activity_interruption.bind("stop")},
		{"label": "Continue", "callback": resolve_player_activity_interruption.bind("continue")},
	]
	var definition = activity.get("interruption", {})
	if definition is Dictionary and bool(definition.get("allow_ignore", false)):
		actions.append({"label": "Continue and ignore", "callback": resolve_player_activity_interruption.bind("ignore")})
	ActivityConfirmation.show_confirm(
		_interruption_prompt(activity),
		actions
	)

func _on_time_turn_passed() -> void:
	update_city_population(TimeManager.total_turns, TimeManager.is_daytime())

func generate_world(playerPos: Vector2i, location: Dictionary = {}) -> void:
	var regionChunks = init_region(Vector2i.ZERO)
	var resolvedPlayerPos: Vector2i = _resolve_start_position(playerPos, location, regionChunks)
	spawn_player(resolvedPlayerPos.x, resolvedPlayerPos.y, "human")
	init_world_bubble(resolvedPlayerPos, true)
	update_world_bubble(resolvedPlayerPos)
	update_city_population(TimeManager.total_turns, TimeManager.is_daytime())
	generated.emit(regionChunks)

func _resolve_start_position(fallback: Vector2i, location: Dictionary, regionChunks: Dictionary) -> Vector2i:
	var chunkIds: Array = _get_location_chunk_ids(location)
	if chunkIds.is_empty():
		return fallback

	var chunkMatch: Dictionary = _find_region_chunk_by_id(regionChunks, chunkIds, _world_pos_to_chunk_pos(fallback))
	if not bool(chunkMatch.get("found", false)):
		var displayName: String = str(location.get("display_name", "scenario location"))
		push_warning("Could not find %s chunk for scenario start; using fallback start." % displayName)
		return fallback

	return _get_chunk_center(chunkMatch.get("position", Vector2i.ZERO))

func _get_location_chunk_ids(location: Dictionary) -> Array:
	var chunkIds: Array = []
	var rawChunkIds = location.get("chunk_ids", [])
	if rawChunkIds is Array:
		for chunkId in rawChunkIds:
			chunkIds.append(str(chunkId))
	return chunkIds

func _find_region_chunk_by_id(regionChunks: Dictionary, chunkIds: Array, originChunk: Vector2i) -> Dictionary:
	if chunkIds.is_empty():
		return { "found": false }

	var chunkMatch: Dictionary = _get_matching_chunk_at(regionChunks, chunkIds, originChunk)
	if bool(chunkMatch.get("found", false)):
		return chunkMatch

	var maxRadius: int = _get_max_chunk_search_radius(regionChunks, originChunk)
	for radius in range(1, maxRadius + 1):
		chunkMatch = _find_matching_chunk_in_ring(regionChunks, chunkIds, originChunk, radius)
		if bool(chunkMatch.get("found", false)):
			return chunkMatch
	return { "found": false }

func _find_matching_chunk_in_ring(regionChunks: Dictionary, chunkIds: Array, originChunk: Vector2i, radius: int) -> Dictionary:
	var chunkMatch: Dictionary = {}
	var eastX: int = originChunk.x + radius
	for dy in range(1 - radius, radius + 1):
		chunkMatch = _get_matching_chunk_at(regionChunks, chunkIds, Vector2i(eastX, originChunk.y + dy))
		if bool(chunkMatch.get("found", false)):
			return chunkMatch

	var southY: int = originChunk.y + radius
	for dx in range(radius - 1, -radius - 1, -1):
		chunkMatch = _get_matching_chunk_at(regionChunks, chunkIds, Vector2i(originChunk.x + dx, southY))
		if bool(chunkMatch.get("found", false)):
			return chunkMatch

	var westX: int = originChunk.x - radius
	for dy in range(radius - 1, -radius - 1, -1):
		chunkMatch = _get_matching_chunk_at(regionChunks, chunkIds, Vector2i(westX, originChunk.y + dy))
		if bool(chunkMatch.get("found", false)):
			return chunkMatch

	var northY: int = originChunk.y - radius
	for dx in range(1 - radius, radius + 1):
		chunkMatch = _get_matching_chunk_at(regionChunks, chunkIds, Vector2i(originChunk.x + dx, northY))
		if bool(chunkMatch.get("found", false)):
			return chunkMatch
	return { "found": false }

func _get_matching_chunk_at(regionChunks: Dictionary, chunkIds: Array, chunkPos: Vector2i) -> Dictionary:
	var key: int = GameWorld.pack_coords(chunkPos.x, chunkPos.y)
	if not regionChunks.has(key):
		return { "found": false }
	if not chunkIds.has(str(regionChunks[key])):
		return { "found": false }
	return {
		"found": true,
		"position": chunkPos,
	}

func _get_max_chunk_search_radius(regionChunks: Dictionary, originChunk: Vector2i) -> int:
	var maxRadius: int = 0
	var keys: Array = regionChunks.keys()
	for key in keys:
		var chunkPos: Vector2i = GameWorld.unpack_coords(int(key))
		var radius: int = maxi(absi(chunkPos.x - originChunk.x), absi(chunkPos.y - originChunk.y))
		maxRadius = maxi(maxRadius, radius)
	return maxRadius

func _world_pos_to_chunk_pos(worldPos: Vector2i) -> Vector2i:
	var chunkSize: float = float(GameWorld.get_chunk_size())
	return Vector2i(
		int(floor(float(worldPos.x) / chunkSize)),
		int(floor(float(worldPos.y) / chunkSize))
	)

func _get_chunk_center(chunkPos: Vector2i) -> Vector2i:
	var chunkSize: int = int(GameWorld.get_chunk_size())
	var halfChunkSize: int = int(chunkSize / 2)
	return Vector2i(
		chunkPos.x * chunkSize + halfChunkSize,
		chunkPos.y * chunkSize + halfChunkSize
	)
