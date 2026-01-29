extends BaseListTab

@export var player: Sprite2D

func _get_display_data() -> Array:
	if not player: return []
	
	var data = []
	for action in player.available_actions:
		data.append({
			"display_name": action.get_action_name(),
			"action_ref": action
		})
	return data

func _on_item_activated() -> void:
	if _items_cache.is_empty() or selected_index < 0 or selected_index >= _items_cache.size():
		return
	
	var action_data = _items_cache[selected_index]
	var action = action_data["action_ref"]
	
	if player and player.has_method("_try_set_action"):
		player._try_set_action(action)
		InputManager.set_mode(InputManager.InputMode.EXPLORATION)
