extends Node
class_name NavAgent

signal path_completed
signal step_completed(pos: Vector2i)

@export var delay: float = 0.0
var path: Array = []
var is_moving: bool = false
var entity: Node2D
var timer: Timer
var tilemap: FastTileMap

func _ready():
	entity = get_parent()
	timer = Timer.new()
	timer.one_shot = true
	add_child(timer)

func navigate_to(target: Vector2i):
	path = Pathfinder.find_path(tilemap, Vector2i(entity.cellPos), target)
	
	if path.is_empty():
		return

	if !is_moving:
		_start_moving()

func _start_moving():
	is_moving = true
	_move_next()

func _move_next():
	if path.is_empty():
		is_moving = false
		path_completed.emit()
		return

	var next_pos = path.pop_front()

	var diff = Vector2(next_pos) - entity.cellPos
	entity.interact_cell(diff)

	step_completed.emit(next_pos)
	
	if path.is_empty():
		is_moving = false
		path_completed.emit()
	else:
		if delay > 0:
			timer.start(delay)
			await timer.timeout
			_move_next()
		else:
			_move_next()

func stop():
	path.clear()
	is_moving = false
	timer.stop()
