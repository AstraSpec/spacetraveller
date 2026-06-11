extends RefCounted
class_name PlayerAction

var player: Sprite2D
var world: GameWorld
var auto: bool = false

func _init(p_player: Sprite2D, p_world: GameWorld):
	player = p_player
	world = p_world

func is_valid(_cell_pos: Vector2i) -> bool:
	return false

func execute(_cell_pos: Vector2i) -> void:
	pass

func get_target_offsets() -> Array[Vector2i]:
	return [
		Vector2i.UP,
		Vector2i.DOWN,
		Vector2i.LEFT,
		Vector2i.RIGHT
	]

func get_action_name() -> String:
	return "Action"
