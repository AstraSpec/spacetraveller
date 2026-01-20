extends RefCounted
class_name PlayerAction

var player: Sprite2D
var world: WorldGeneration

func _init(p_player: Sprite2D, p_world: WorldGeneration):
	player = p_player
	world = p_world

func is_valid(_cell_pos: Vector2i) -> bool:
	return false

func execute(_cell_pos: Vector2i) -> void:
	pass

func get_action_name() -> String:
	return "Action"
