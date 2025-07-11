extends Sprite2D

@export var World :Node2D

const MOVE_TIMER :float = 0.35
const HOLD_MOVE_TIMER :float = 0.05

var timeSinceMove :float = 0.0
var keyPressed :Vector2 = Vector2.ZERO
var keyHeld :bool = false
var shiftMode :bool = false

var cellPos :Vector2 = Vector2.ZERO

func _process(delta) -> void:
	# Computes time since moved
	timeSinceMove += delta
	
	shiftMode = Input.is_action_pressed("shift")
	
	# Gets movement vector
	var displacement :Vector2 = Vector2(Input.get_axis("a", "d"), Input.get_axis("w", "s"))
	
	# No keys held
	if displacement == Vector2.ZERO:
		keyPressed = Vector2.ZERO
		keyHeld = false
		
	# Different key pressed from previous key
	elif displacement != keyPressed and shiftMode == false:
		interact_cell(displacement)
		keyHeld = false
	
	# Same key pressed from previous key
	else:
		# Different wait timers for held/not held
		if !keyHeld:
			if timeSinceMove > MOVE_TIMER:
				keyHeld = true
				interact_cell(displacement)
		else:
			if timeSinceMove > HOLD_MOVE_TIMER:
				interact_cell(displacement)

func interact_cell(displacement :Vector2) -> void:
	timeSinceMove = 0.0
	keyPressed = displacement
	
	cellPos += displacement
	
	World.update_world_bubble(cellPos)
	
	
