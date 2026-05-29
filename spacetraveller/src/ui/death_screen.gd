extends Control

@export var _GameWorld :GameWorld
@export var PlayAgainButton :Button

func _ready() -> void:
	visible = false
	if _GameWorld:
		_GameWorld.player_died.connect(_on_player_died)
	if PlayAgainButton:
		PlayAgainButton.pressed.connect(_on_play_again_pressed)

func _on_player_died(_cause: String) -> void:
	visible = true
	# Block all game input
	InputManager.reset_stack(InputManager.InputMode.MENU)
	if PlayAgainButton:
		PlayAgainButton.grab_focus()

func _on_play_again_pressed() -> void:
	TimeManager.reset()
	SaveManager.loaded_save_data = {}
	get_tree().reload_current_scene()
