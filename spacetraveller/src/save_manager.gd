extends Node

const GAME_NAME = "SpaceTraveller"
const SAVE_DIR_NAME = "saves"

var save_path: String = ""

var WorldGen: WorldGeneration
var Player: Node
var _Inventory: Inventory

func register_world(w: WorldGeneration) -> void: WorldGen = w
func register_player(p: Node) -> void: Player = p
func register_inventory(i: Inventory) -> void: _Inventory = i

func _ready() -> void:
	initialize()

func initialize() -> void:
	var appdata = OS.get_environment("APPDATA")
	if appdata == "":
		# Fallback for non-windows
		appdata = OS.get_config_dir()
	
	save_path = appdata.path_join(GAME_NAME).path_join(SAVE_DIR_NAME)
	
	if not DirAccess.dir_exists_absolute(save_path):
		DirAccess.make_dir_recursive_absolute(save_path)
		print("Created save directory: ", save_path)

func save_game(slot_name: String = "") -> void:
	if not WorldGen or not Player or not _Inventory:
		printerr("Cannot save: nodes not registered in SaveManager")
		return
		
	if slot_name == "":
		slot_name = _generate_save_name()
		
	var full_path = save_path.path_join(slot_name + ".json")
	
	var data = {
		"version": 1,
		"time": TimeManager.total_turns,
		"player": Player.get_save_data(),
		"inventory": _Inventory.get_save_data(),
		"world": WorldGen.get_save_data()
	}
	
	var file = FileAccess.open(full_path, FileAccess.WRITE)
	if file:
		file.store_string(JSON.stringify(data, "\t"))
		file.close()
		print("Game saved to: ", full_path)
	else:
		printerr("Failed to save game to: ", full_path)

func load_game(slot_name: String) -> void:
	if not WorldGen or not Player or not _Inventory:
		printerr("Cannot load: nodes not registered in SaveManager")
		return
		
	if slot_name == "":
		printerr("Cannot load: no slot name provided")
		return
		
	var full_path = save_path.path_join(slot_name + ".json")
	
	if not FileAccess.file_exists(full_path):
		printerr("Save file does not exist: ", full_path)
		return
		
	var file = FileAccess.open(full_path, FileAccess.READ)
	if not file:
		printerr("Failed to open save file: ", full_path)
		return
		
	var json_string = file.get_as_text()
	file.close()
	
	var json = JSON.new()
	var error = json.parse(json_string)
	if error != OK:
		printerr("JSON Parse Error: ", json.get_error_message(), " at line ", json.get_error_line())
		return
		
	var data = json.data
	
	# Order matters for loading
	if data.has("time"):
		TimeManager.total_turns = data["time"]
		
	if data.has("world"):
		WorldGen.load_save_data(data["world"])
		
	if data.has("player"):
		Player.load_save_data(data["player"])
		
	if data.has("inventory"):
		_Inventory.load_save_data(data["inventory"])
		
	print("Game loaded from: ", full_path)
	
	# Refresh UI/Visuals
	WorldGen.update_world_bubble(Player.cellPos)
	_Inventory.inventory_changed.emit()

func get_save_list() -> Array:
	var saves = []
	if not DirAccess.dir_exists_absolute(save_path):
		return saves
		
	var dir = DirAccess.open(save_path)
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
	var full_path = save_path.path_join(slot_name + ".json")
	if FileAccess.file_exists(full_path):
		DirAccess.remove_absolute(full_path)
		print("Deleted save: ", full_path)

func _generate_save_name() -> String:
	var datetime = Time.get_datetime_dict_from_system()
	return "save_%04d-%02d-%02d_%02d-%02d-%02d" % [
		datetime.year, datetime.month, datetime.day,
		datetime.hour, datetime.minute, datetime.second
	]
