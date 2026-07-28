extends PlayerAction
class_name CloseAction

func _init(p_player: PlayerController, p_world: GameWorld):
	super(p_player, p_world)
	auto = true

func is_valid(cell_pos: Vector2i) -> bool:
	var tile_id := world.get_tile_at(cell_pos.x, cell_pos.y)
	return TileDb.has_tag(tile_id, "CAN_CLOSE")

func execute(cell_pos: Vector2i) -> void:
	world.submit_player_intent(GameWorld.INTENT_CLOSE, cell_pos.x, cell_pos.y, "")

func get_action_name() -> String:
	return "Close"

func get_target_prompt() -> String:
	return "Close what?"
