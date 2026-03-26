extends BaseListTab

func _get_display_data() -> Array:
	return [
		{ "display_name": "Close Menu", "id": "close" },
		{ "display_name": "New Game", "id": "new" },
		{ "display_name": "Save Game", "id": "save" },
		{ "display_name": "Load Game", "id": "load" },
		{ "display_name": "Options", "id": "options" },
		{ "display_name": "Keybinds", "id": "keybinds" },
		{ "display_name": "Exit game", "id": "exit" }
	]

func _on_item_activated() -> void:
	if _items_cache.is_empty() or selected_index < 0 or selected_index >= _items_cache.size():
		return
	
	var item = _items_cache[selected_index]
	match item.id:
		"close":
			InputManager.set_mode(InputManager.InputMode.EXPLORATION)
		"new":
			TimeManager.reset()
			InputManager.set_mode(InputManager.InputMode.EXPLORATION)
			# Delay reload slightly to ensure menu is closed and input handled
			await get_tree().process_frame
			get_tree().reload_current_scene()
		"save":
			SaveManager.save_game()
			InputManager.set_mode(InputManager.InputMode.EXPLORATION)
		"load":
			InputManager.toggle_menu("load_game")
		"options":
			print("Options - Not yet implemented")
		"keybinds":
			print("Keybinds - Not yet implemented")
		"exit":
			get_tree().quit()
