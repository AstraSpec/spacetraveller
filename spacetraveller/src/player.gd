extends Sprite2D

@export var World :WorldGeneration

const MOVE_TIMER :float = 0.3
const HOLD_MOVE_TIMER :float = 0.05

var timeSinceMove :float = 0.0
var keyPressed :Vector2 = Vector2.ZERO
var keyHeld :bool = false
var shiftMode :bool = false

var cellPos :Vector2 = Vector2.ZERO

# Track previous frame's key states to detect new key presses
var prevUp :bool = false
var prevDown :bool = false
var prevLeft :bool = false
var prevRight :bool = false

func _process(delta) -> void:
	# Computes time since moved
	timeSinceMove += delta
	
	shiftMode = Input.is_action_pressed("shift")
	
	# Get current key states
	var up = Input.is_action_pressed("w")
	var down = Input.is_action_pressed("s")
	var left = Input.is_action_pressed("a")
	var right = Input.is_action_pressed("d")
	
	# Check if any NEW key was pressed this frame (not held from before)
	var newKeyPressed = (up and !prevUp) or (down and !prevDown) or (left and !prevLeft) or (right and !prevRight)
	
	# Calculate direction based on ONLY the just-pressed keys
	var newDirection = Vector2.ZERO
	if up and !prevUp: newDirection.y -= 1
	if down and !prevDown: newDirection.y += 1
	if left and !prevLeft: newDirection.x -= 1
	if right and !prevRight: newDirection.x += 1
	
	# Update previous state for next frame
	prevUp = up
	prevDown = down
	prevLeft = left
	prevRight = right
	
	# Gets movement vector
	var displacement :Vector2 = Vector2(Input.get_axis("a", "d"), Input.get_axis("w", "s"))
	
	# No keys held
	if displacement == Vector2.ZERO:
		keyPressed = Vector2.ZERO
		keyHeld = false
		timeSinceMove = MOVE_TIMER if shiftMode else 0.0
		
	# New key pressed this frame - move instantly in NEW direction only
	elif displacement != keyPressed and shiftMode == false and newKeyPressed:
		interact_cell(newDirection)
		keyPressed = displacement
		keyHeld = false
	
	# Same direction held
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
	
	
