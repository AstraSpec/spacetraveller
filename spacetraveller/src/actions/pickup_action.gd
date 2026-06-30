extends PlayerAction
class_name PickupAction

func _init(p_player: PlayerController, p_world: GameWorld):
	super(p_player, p_world)
	auto = true

func is_valid(cell_pos: Vector2i) -> bool:
	return world.has_item(cell_pos)

func execute(cell_pos: Vector2i) -> void:
	InputManager.toggle_menu("nearby", {"filter_pos": cell_pos})

func get_action_name() -> String:
	return "Pickup"
