extends Node

var _GameWorld: GameWorld
var loaded_save_data: Dictionary = {}

func register_world(w: GameWorld) -> void:
	_GameWorld = w

func save_game(slot_name: String = "") -> void:
	if not _GameWorld:
		printerr("Cannot save: GameWorld not registered in SaveManager")
		return
		
	if slot_name == "":
		slot_name = _generate_save_name()
		
	var full_path = App.save_path.path_join(slot_name + ".json")
	
	var data = {
		"version": 1,
		"time": TimeManager.total_turns,
		"world": _GameWorld.get_save_data(),
	}
	
	var file = FileAccess.open(full_path, FileAccess.WRITE)
	if file:
		file.store_string(JSON.stringify(data, "\t"))
		file.close()
		print("Game saved to: ", full_path)
	else:
		printerr("Failed to save game to: ", full_path)

func load_save_to_memory(slot_name: String) -> bool:
	if slot_name == "":
		printerr("Cannot load to memory: no slot name provided")
		return false
		
	var full_path = App.save_path.path_join(slot_name + ".json")
	
	if not FileAccess.file_exists(full_path):
		printerr("Save file does not exist: ", full_path)
		return false
		
	var file = FileAccess.open(full_path, FileAccess.READ)
	if not file:
		printerr("Failed to open save file: ", full_path)
		return false
		
	var json_string = file.get_as_text()
	file.close()
	
	var json = JSON.new()
	var error = json.parse(json_string)
	if error != OK:
		printerr("JSON Parse Error: ", json.get_error_message(), " at line ", json.get_error_line())
		return false
		
	loaded_save_data = json.data
	return true

func apply_loaded_data() -> void:
	if loaded_save_data.is_empty():
		return
		
	if loaded_save_data.has("time"):
		TimeManager.total_turns = loaded_save_data["time"]
		
	if loaded_save_data.has("world") and _GameWorld:
		_GameWorld.load_save_data(loaded_save_data["world"])
		# Entity 0 (player) position, anatomy, clothing, health, equipment
		# are all restored inside _GameWorld.load_save_data()
		_GameWorld.update_world_bubble(_GameWorld.get_player_position())

func load_game(slot_name: String) -> void:
	if not _GameWorld:
		printerr("Cannot load: GameWorld not registered in SaveManager")
		return
		
	if load_save_to_memory(slot_name):
		apply_loaded_data()
		loaded_save_data = {}
		print("Game loaded from: ", slot_name)

func get_save_list() -> Array:
	var saves = []
	if not DirAccess.dir_exists_absolute(App.save_path):
		return saves
		
	var dir = DirAccess.open(App.save_path)
	if dir:
		dir.list_dir_begin()
		var file_name = dir.get_next()
		while file_name != "":
			if not dir.current_is_dir() and file_name.ends_with(".json"):
				saves.append(file_name.get_basename())
			file_name = dir.get_next()
	saves.sort()
	saves.reverse()
	return saves

func delete_save(slot_name: String) -> void:
	var full_path = App.save_path.path_join(slot_name + ".json")
	if FileAccess.file_exists(full_path):
		DirAccess.remove_absolute(full_path)
		print("Deleted save: ", full_path)

func _generate_save_name() -> String:
	var datetime = Time.get_datetime_dict_from_system()
	return "save_%04d-%02d-%02d_%02d-%02d-%02d" % [
		datetime.year, datetime.month, datetime.day,
		datetime.hour, datetime.minute, datetime.second
	]