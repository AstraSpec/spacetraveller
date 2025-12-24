extends CanvasLayer

@export var MapMenu :Control

func _unhandled_key_input(_event: InputEvent) -> void:
	if Input.is_action_just_pressed("m"):
		MapMenu.visible = !MapMenu.visible
