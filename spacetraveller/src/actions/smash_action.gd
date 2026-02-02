extends PlayerAction
class_name SmashAction

func is_valid(cell_pos: Vector2i) -> bool:
	var tile_id := world.get_tile_at(cell_pos.x, cell_pos.y)
	return TileDb.has_tag(tile_id, "SMASHABLE")

func execute(cell_pos: Vector2i) -> void:
	var broken_tile := world.get_tile_at(cell_pos.x, cell_pos.y)
	world.place_tile(cell_pos.x, cell_pos.y, "dirt")
	if broken_tile == "tree":
		world.drop_item(cell_pos, "stick", 8)

func get_action_name() -> String:
	return "Smash"
