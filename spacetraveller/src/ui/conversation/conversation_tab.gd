extends BaseListTab

@export var _GameWorld: GameWorld

var target_id: int = -1

func set_params(params: Dictionary) -> void:
	if params.has("target"):
		target_id = params["target"]
	_update_title()
	refresh_view()

func _update_title() -> void:
	if not _GameWorld or target_id < 0:
		return
	var win := _find_window()
	if not win:
		return
	var full_name = _GameWorld.get_entity_name(target_id)
	win.title = full_name if not full_name.is_empty() else "Conversation"

func _find_window() -> Window:
	var node := get_parent()
	while node:
		if node is Window:
			return node
		node = node.get_parent()
	return null

func _get_display_data() -> Array:
	return [
		{ "display_name": "Greeting", "description": "Exchange pleasantries." },
		{ "display_name": "Ask about this place", "description": "Inquire about the surrounding area." },
		{ "display_name": "Farewell", "description": "End the conversation." },
	]
