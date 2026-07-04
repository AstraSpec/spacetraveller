extends GameWorld

@export var Tilesheet :Texture2D
@export var Player :PlayerController
@onready var BiomeNoise :FastNoiseLite = preload("res://noise/biome_noise.tres")

signal generated(regionChunks)

var seed_ :int = randi()

func _ready() -> void:
	setup_renderer()
	biome_noise = BiomeNoise
	get_renderer().tilesheet = Tilesheet
	world_seed = seed_

	InputManager.inventory_item_dropped.connect(_on_inventory_item_dropped)
	player_action_resolved.connect(_on_player_action_resolved)

func _on_inventory_item_dropped(ID: String, amount: int) -> void:
	drop_item(Vector2i(get_player_position()), ID, amount)
	update_world_bubble(get_player_position())

func _on_player_action_resolved(_entity_id: int, cost: float, _next_turn_time: float) -> void:
	TimeManager.advance_turn(max(1, int(cost)))

func generate_world(playerPos: Vector2i, location: Dictionary = {}) -> void:
	var regionChunks = init_region(Vector2i.ZERO)
	var resolvedPlayerPos: Vector2i = _resolve_start_position(playerPos, location, regionChunks)
	spawn_player(resolvedPlayerPos.x, resolvedPlayerPos.y, "human")
	init_world_bubble(resolvedPlayerPos, true)
	update_world_bubble(resolvedPlayerPos)
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
