extends Node

const RESOLUTIONS = [
	Vector2i(800, 600),
	Vector2i(1024, 768),
	Vector2i(1280, 720),
	Vector2i(1366, 768),
	Vector2i(1600, 900),
	Vector2i(1920, 1080)
]

var registry = {
	"resolution_index": {
		"section": "Video",
		"default": 2,
		"apply": func(): _apply_video_settings(),
		"ui": {
			"label": "Resolution",
			"type": "cycle",
			"get_options": func(): return RESOLUTIONS.map(func(r): return str(r.x) + "x" + str(r.y))
		}
	},
	"fullscreen": {
		"section": "Video",
		"default": false,
		"apply": func(): _apply_video_settings(),
		"ui": {
			"label": "Fullscreen",
			"type": "toggle"
		}
	}
}

var _config = ConfigFile.new()

func load_settings() -> void:
	var err = _config.load(App.settings_path)
	if err != OK:
		print("No settings file found or error loading, initializing with defaults.")
		_initialize_defaults_in_config()
	
	# Apply all registered settings on load
	var applied_funcs = []
	for key in registry:
		var def = registry[key]
		if not applied_funcs.has(def.apply):
			def.apply.call()
			applied_funcs.append(def.apply)
			
	_apply_keybinds()

func _initialize_defaults_in_config() -> void:
	for key in registry:
		var def = registry[key]
		if not _config.has_section_key(def.section, key):
			_config.set_value(def.section, key, def.default)
	save_settings()

func save_settings() -> void:
	var err = _config.save(App.settings_path)
	if err != OK:
		printerr("Failed to save settings to: ", App.settings_path)
	else:
		print("Settings saved to: ", App.settings_path)

func set_setting(section: String, key: String, value: Variant) -> void:
	_config.set_value(section, key, value)

func get_setting(section: String, key: String, default: Variant) -> Variant:
	return _config.get_value(section, key, default)

# Simplified accessors using the registry
func get_val(key: String) -> Variant:
	if not registry.has(key):
		printerr("Setting not found in registry: ", key)
		return null
	var def = registry[key]
	return get_setting(def.section, key, def.default)

func update_and_apply(key: String, value: Variant, auto_save: bool = true) -> void:
	if not registry.has(key):
		printerr("Setting not found in registry: ", key)
		return
	var def = registry[key]
	set_setting(def.section, key, value)
	def.apply.call()
	
	if auto_save:
		save_settings()

func _apply_video_settings() -> void:
	var is_fs = get_val("fullscreen")
	var res_idx = get_val("resolution_index")
	var res = RESOLUTIONS[res_idx]
	
	if is_fs:
		DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_FULLSCREEN)
	else:
		DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_WINDOWED)
		DisplayServer.window_set_size(res)
		var screen = DisplayServer.get_primary_screen()
		var screen_size = DisplayServer.screen_get_size(screen)
		var screen_pos = DisplayServer.screen_get_position(screen)
		DisplayServer.window_set_position(screen_pos + (screen_size - res) / 2)

func _apply_keybinds() -> void:
	if not _config.has_section("Controls"):
		return
		
	for action in _config.get_section_keys("Controls"):
		var events = _config.get_value("Controls", action)
		if events is Array:
			InputMap.action_erase_events(action)
			for event in events:
				if event is InputEvent:
					InputMap.action_add_event(action, event)

func save_keybind(action: String, events: Array, auto_save: bool = true) -> void:
	set_setting("Controls", action, events)
	if auto_save:
		save_settings()
