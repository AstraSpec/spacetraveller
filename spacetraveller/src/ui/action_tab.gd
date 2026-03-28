extends BaseListTab

func _get_display_data() -> Array:
	var data = []
	for action in Player.available_actions:
		data.append({
			"display_name": action.get_action_name(),
			"description": "Trigger " + action.get_action_name() + " action.",
			"action_ref": action
		})
	return data

func _on_item_activated() -> void:
	if _items_cache.is_empty() or selected_index < 0:
		return
	
	var action_data = _items_cache[selected_index]
	var action = action_data["action_ref"]
	
	Player._try_set_action(action)
	# Close the menu after selecting an action
	InputManager.set_mode(InputManager.InputMode.EXPLORATION)
