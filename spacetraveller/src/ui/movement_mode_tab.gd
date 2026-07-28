extends BaseListTab

func _get_display_data() -> Array:
	var data: Array = []
	for option_value in Player.World.get_player_movement_mode_options():
		if not option_value is Dictionary:
			continue
		var option: Dictionary = option_value
		data.append({
			"id": str(option.get("id", "")),
			"display_name": str(option.get("name", "")),
			"left": str(option.get("name", "")),
			"right": "Active" if bool(option.get("active", false)) else "",
			"description": str(option.get("description", "")),
			"disabled": not bool(option.get("available", true)),
			"active": bool(option.get("active", false)),
		})
	return data

func _on_refresh() -> void:
	for i in range(_items_cache.size()):
		if bool(_items_cache[i].get("active", false)):
			selected_index = i
			if stripContainer:
				stripContainer._update_selection_visuals()
			break

func _on_item_activated() -> void:
	if selected_index < 0 or selected_index >= _items_cache.size():
		return
	var option: Dictionary = _items_cache[selected_index]
	if bool(option.get("disabled", false)):
		return
	if Player.set_movement_mode(str(option.get("id", ""))):
		InputManager.pop_mode()
