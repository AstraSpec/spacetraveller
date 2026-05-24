extends Node
class_name NavAgent

signal path_completed
signal step_completed(pos: Vector2i)

@export var delay: float = 0.0
@export var show_path: bool = false
var path: Array = []
var is_moving: bool = false
var entity: Node2D
var timer: Timer
var world: GameWorld

func _ready():
	entity = get_parent()
	timer = Timer.new()
	timer.one_shot = true
	add_child(timer)

func navigate_to(target: Vector2i):
	path = Pathfinder.find_path(world, Vector2i(entity.cellPos), target)
	
	if path.is_empty():
		if show_path:
			_clear_visual_path()
		return

	if show_path:
		_draw_visual_path()

	if !is_moving:
		_start_moving()

func _draw_visual_path():
	_clear_visual_path()
	for i in range(path.size()):
		var p = path[i]
		var id = "path_end" if i == path.size() - 1 else "path_node"
		world.place_tile(p.x, p.y, id, GameWorld.LAYER_INDICATOR)

func _clear_visual_path():
	world.clear_cache(GameWorld.LAYER_INDICATOR)

func _start_moving():
	is_moving = true
	_move_next()

func _move_next():
	if path.is_empty():
		is_moving = false
		path_completed.emit()
		return

	var next_pos = path.pop_front()
	
	if show_path:
		world.place_tile(next_pos.x, next_pos.y, "void", GameWorld.LAYER_INDICATOR)
	
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