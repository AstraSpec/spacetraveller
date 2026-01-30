extends PlayerAction
class_name PickupAction

func is_valid(cell_pos: Vector2i) -> bool:
	return world.has_item(cell_pos)

func execute(cell_pos: Vector2i) -> void:
	InputManager.toggle_menu("inventory", {"tab": "nearby", "filter_pos": cell_pos})

func get_action_name() -> String:
	return "Pickup"
