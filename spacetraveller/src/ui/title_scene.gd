extends Control

@export var menu_container: ButtonListContainer
@export var load_game_ui: BaseWindow
@export var keybind_ui: BaseWindow
@export var credits_ui: BaseWindow

func _ready() -> void:
	RenderingServer.set_default_clear_color(Color.BLACK)
	
	if menu_container:
		menu_container.item_selected.connect(_on_menu_item_selected)
		menu_container.item_activated.connect(_on_menu_item_activated)
		
		var menu_data = [
			{ "display_name": "New Game", "id": "new" },
			{ "display_name": "Load Game", "id": "load" },
			{ "display_name": "Options", "id": "options" },
			{ "display_name": "Credits", "id": "credits" },
			{ "display_name": "Exit", "id": "exit" }
		]
		menu_container.set_data(menu_data)
	
	InputManager.reset_stack(InputManager.InputMode.MENU)
	InputManager.ui_directional_input.connect(_on_ui_directional_input)
	InputManager.ui_accept.connect(_on_ui_accept)

func _on_ui_directional_input(direction: Vector2) -> void:
	if load_game_ui and load_game_ui.visible: return
	if keybind_ui and keybind_ui.visible: return
	if credits_ui and credits_ui.visible: return
	if menu_container:
		menu_container.handle_directional_input(direction)

func _on_ui_accept() -> void:
	if load_game_ui and load_game_ui.visible: return
	if keybind_ui and keybind_ui.visible: return
	if credits_ui and credits_ui.visible: return
	var item = menu_container._get_data_for_button_index(menu_container.selected_index)
	_on_menu_item_activated(menu_container.selected_index, item)

func _on_menu_item_selected(_index: int, _data: Variant) -> void:
	pass

func _on_menu_item_activated(_index: int, data: Variant) -> void:
	if not data: return
	
	match data.id:
		"new":
			SaveManager.loaded_save_data = {}
			get_tree().change_scene_to_file("res://main.tscn")
		"load":
			InputManager.toggle_menu("load_game")
		"options":
			print("Options not implemented")
		"credits":
			InputManager.toggle_menu("credits")
		"exit":
			get_tree().quit()
