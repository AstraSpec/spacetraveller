extends PlayerAction
class_name SmashAction

func is_valid(cell_pos: Vector2i) -> bool:
	return world.get_tile_at(cell_pos.x, cell_pos.y) == "tree"

func execute(cell_pos: Vector2i) -> void:
	world.place_tile(cell_pos.x, cell_pos.y, "dirt")

func get_action_name() -> String:
	return "Smash"
