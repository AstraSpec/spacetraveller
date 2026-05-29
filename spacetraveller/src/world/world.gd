extends GameWorld

@export var Tilesheet :Texture2D
@export var Player :Sprite2D
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

func generate_world(playerPos :Vector2i) -> void:
	var regionChunks = init_region(Vector2i.ZERO)
	spawn_player(playerPos.x, playerPos.y, "human")
	init_world_bubble(playerPos)
	spawn_entity(playerPos.x + 10, playerPos.y, "human")
	update_world_bubble(playerPos)
	generated.emit(regionChunks)
