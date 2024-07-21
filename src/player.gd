extends Sprite2D

const MOVE_TIMER :float = 0.35
const HOLD_MOVE_TIMER :float = 0.05

var time_since_move :float = 0.0
var key_pressed :Vector2 = Vector2.ZERO
var key_held :bool = false
var shift_mode :bool = false

func _process(delta) -> void:
	# Computes time since moved
	time_since_move += delta
	
	# Gets movement vector
	var displacement :Vector2 = Vector2(Input.get_axis("a", "d"), Input.get_axis("w", "s"))
	
	# No keys held
	if displacement == Vector2.ZERO:
		key_pressed = Vector2.ZERO
		key_held = false
		
	# Different key pressed from previous key
	elif displacement != key_pressed and shift_mode == false:
		interact_cell(displacement)
		key_held = false
	
	# Same key pressed from previous key
	else:
		# Different wait timers for held/not held
		if !key_held:
			if time_since_move > MOVE_TIMER:
				key_held = true
				interact_cell(displacement)
		else:
			if time_since_move > HOLD_MOVE_TIMER:
				interact_cell(displacement)

func interact_cell(displacement :Vector2) -> void:
	print(displacement)
