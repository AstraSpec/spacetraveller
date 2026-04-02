extends BaseListTab

func _get_display_data() -> Array:
	return [
		{ "display_name": "Close Menu", "id": "close" },
		{ "display_name": "New Game", "id": "new" },
		{ "display_name": "Save Game", "id": "save" },
		{ "display_name": "Load Game", "id": "load" },
		{ "display_name": "Options", "id": "options" },
		{ "display_name": "Keybinds", "id": "keybinds" },
		{ "display_name": "Exit Game", "id": "title" }
	]

func _on_item_activated() -> void:
	if _items_cache.is_empty() or selected_index < 0 or selected_index >= _items_cache.size():
		return
	
	var item = _items_cache[selected_index]
	match item.id:
		"close":
			InputManager.pop_mode()
		"new":
			TimeManager.reset()
			InputManager.pop_mode()
			# Delay reload slightly to ensure menu is closed and input handled
			await get_tree().process_frame
			get_tree().reload_current_scene()
		"save":
			SaveManager.save_game()
			InputManager.pop_mode()
		"load":
			InputManager.pop_mode()
			InputManager.toggle_menu("load_game")
		"options":
			InputManager.pop_mode()
			InputManager.toggle_menu("options")
		"keybinds":
			InputManager.pop_mode()
			InputManager.toggle_menu("keybinds")
		"title":
			SaveManager.loaded_save_data = {}
			get_tree().change_scene_to_file("res://src/ui/title_scene.tscn")
