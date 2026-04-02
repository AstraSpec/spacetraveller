extends Node

const GAME_NAME = "SpaceTraveller"
const SAVE_DIR_NAME = "saves"
const SETTINGS_FILE_NAME = "settings.cfg"

var base_path: String = ""
var save_path: String = ""
var settings_path: String = ""

func _ready() -> void:
	_initialize_paths()

func _initialize_paths() -> void:
	var appdata = OS.get_environment("APPDATA")
	if appdata == "":
		appdata = OS.get_config_dir()
	
	base_path = appdata.path_join(GAME_NAME)
	save_path = base_path.path_join(SAVE_DIR_NAME)
	settings_path = base_path.path_join(SETTINGS_FILE_NAME)
	
	_ensure_dir_exists(base_path)
	_ensure_dir_exists(save_path)

func _ensure_dir_exists(path: String) -> void:
	if not DirAccess.dir_exists_absolute(path):
		DirAccess.make_dir_recursive_absolute(path)
		print("Created directory: ", path)
