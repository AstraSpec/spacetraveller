extends PlayerAction
class_name ExamineAction

func _init(p_player: PlayerController, p_world: GameWorld):
	super(p_player, p_world)
	auto = true

func is_valid(cell_pos: Vector2i) -> bool:
	return not _metadata_text(world.get_tile_metadata(cell_pos)).is_empty()

func execute(cell_pos: Vector2i) -> void:
	var metadata := world.get_tile_metadata(cell_pos)
	var text := _metadata_text(metadata)
	if text.is_empty():
		return
	EventBus.post("metadata", "The text here says \"%s\"." % text, {"pos": cell_pos, "metadata": metadata})

func get_action_name() -> String:
	return "Examine"

func get_target_prompt() -> String:
	return "Examine where?"

func _metadata_text(metadata: Dictionary) -> String:
	var data = metadata.get("data", {})
	if data is Dictionary:
		return str(data.get("text", ""))
	return ""
