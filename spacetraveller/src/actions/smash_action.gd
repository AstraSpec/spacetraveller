extends PlayerAction
class_name SmashAction

func is_valid(cell_pos: Vector2i) -> bool:
	var tile_id := world.get_tile_at(cell_pos.x, cell_pos.y)
	return TileDb.has_tag(tile_id, "SMASHABLE")

func execute(cell_pos: Vector2i) -> void:
	world.submit_player_intent(GameWorld.INTENT_SMASH, cell_pos.x, cell_pos.y, "")

func get_action_name() -> String:
	return "Smash"
