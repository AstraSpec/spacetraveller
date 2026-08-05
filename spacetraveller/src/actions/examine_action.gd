extends PlayerAction
class_name ExamineAction

func _init(p_player: PlayerController, p_world: GameWorld):
	super(p_player, p_world)
	auto = true

func is_valid(cell_pos: Vector2i) -> bool:
	var tile_id := world.get_tile_at(cell_pos.x, cell_pos.y)
	return not _metadata_text(world.get_tile_metadata(cell_pos)).is_empty() or not TileDb.get_tool_data(tile_id).is_empty()

func execute(cell_pos: Vector2i) -> void:
	var metadata := world.get_tile_metadata(cell_pos)
	var text := _metadata_text(metadata)
	if not text.is_empty():
		EventBus.post("metadata", "The text here says \"%s\"." % text, {"pos": cell_pos, "metadata": metadata})

	var tile_id := world.get_tile_at(cell_pos.x, cell_pos.y)
	if not TileDb.get_tool_data(tile_id).is_empty():
		InputManager.toggle_menu("inventory", {
			"tab": "crafting",
			"station_pos": cell_pos,
			"station_id": tile_id,
		})

func get_action_name() -> String:
	return "Examine"

func get_target_prompt() -> String:
	return "Examine where?"

func _metadata_text(metadata: Dictionary) -> String:
	var data = metadata.get("data", {})
	if data is Dictionary:
		return str(data.get("text", ""))
	return ""
