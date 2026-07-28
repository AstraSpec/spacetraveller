extends PlayerAction
class_name AttackAction

func is_valid(cell_pos: Vector2i) -> bool:
	return world.has_entity_at_cell(cell_pos.x, cell_pos.y)

func get_action_name() -> String:
	return "Attack"

func get_target_prompt() -> String:
	return "Attack whom?"
