extends Label

func _process(_delta: float) -> void:
	text = str(Engine.get_frames_per_second())
	
	if Input.is_action_just_pressed("f3"):
		visible = !visible
