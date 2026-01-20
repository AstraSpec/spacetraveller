extends PlayerAction
class_name PickupAction

func is_valid(cell_pos: Vector2i) -> bool:
	return world.has_item(cell_pos)

func execute(cell_pos: Vector2i) -> void:
	world.pickup_item(cell_pos, player.inventory)

func get_action_name() -> String:
	return "Pickup"
