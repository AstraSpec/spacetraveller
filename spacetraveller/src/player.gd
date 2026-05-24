extends Sprite2D

signal moved_cell(cellPos :Vector2)
signal moved_chunk(chunkPos :Vector2)

@onready var InteractionCell :PackedScene = preload("res://src/interaction_cell.tscn")
@export var Camera :Camera2D
@export var World : GameWorld
@export var _Inventory: Inventory
@export var _Anatomy :Anatomy
@export var _Clothing :Clothing
@export var _Equipment: EquipmentComponent

var nav_agent: NavAgent

const DIR :Array[Vector2] = [Vector2.UP, Vector2.DOWN, Vector2.LEFT, Vector2.RIGHT]

var CHUNK_SIZE = GameWorld.get_chunk_size()

var cellPos : Vector2 = Vector2.ZERO
var chunkPos : Vector2 = Vector2.ZERO

var currentAction: PlayerAction = null
var interactionCells : Array[Node2D] = []

var available_actions: Array[PlayerAction] = []

func _ready():
	InputManager.directional_input.connect(_on_movement_triggered)
	InputManager.action_smash_requested.connect(_on_smash_requested)
	InputManager.action_pickup_requested.connect(_on_pickup_requested)
	InputManager.exploration_right_click.connect(_on_right_click)
	
	nav_agent = NavAgent.new()
	nav_agent.delay = 0.05
	nav_agent.show_path = true
	add_child(nav_agent)
	nav_agent.world = World
	nav_agent.step_completed.connect(func(_pos): TimeManager.advance_turn())

	# Register default actions
	available_actions.append(SmashAction.new(self, World))
	available_actions.append(PickupAction.new(self, World))

func _on_right_click(_global_pos: Vector2):
	var mouse_local = get_local_mouse_position()
	var cell_diff = (mouse_local / World.get_renderer().get_cell_size()).floor()
	var target_cell = Vector2i(cellPos) + Vector2i(cell_diff)
	nav_agent.navigate_to(target_cell)
	World.update_world_bubble(cellPos)

func _on_smash_requested():
	_try_set_action(SmashAction.new(self, World))

func _on_pickup_requested():
	_try_set_action(PickupAction.new(self, World))

func _clear_interaction_cells():
	for cell in interactionCells:
		cell.queue_free()
	interactionCells.clear()

func _try_set_action(action: PlayerAction):
	_clear_interaction_cells()
	var tile_size = FastTileMap.get_tile_size()
	var found_valid = false
	
	for dir :Vector2 in DIR:
		var target = Vector2i(cellPos + dir)
		if action.is_valid(target):
			found_valid = true
			var inst = InteractionCell.instantiate()
			inst.position = dir * tile_size
			add_child(inst)
			interactionCells.append(inst)
	
	if found_valid:
		# Entered action mode
		currentAction = action
	else:
		# No valid target found
		currentAction = null

func _on_movement_triggered(dir: Vector2):
	nav_agent.stop()
	if currentAction:
		var target_cell = Vector2i(cellPos) + Vector2i(dir)
		if currentAction.is_valid(target_cell):
			currentAction.execute(target_cell)
			World.update_world_bubble(cellPos)
			TimeManager.advance_turn()
		
		currentAction = null
		_clear_interaction_cells()
		# Returns to move mode
	else:
		interact_cell(dir)
		TimeManager.advance_turn()

func get_save_data() -> Dictionary:
	return {
		"cellPos": {"x": cellPos.x, "y": cellPos.y},
		"chunkPos": {"x": chunkPos.x, "y": chunkPos.y},
		"anatomy": _Anatomy.get_save_data(),
		"clothing": _Clothing.get_save_data()
	}

func load_save_data(data: Dictionary) -> void:
	cellPos = _load_vector2(data.get("cellPos", cellPos))
	chunkPos = _load_vector2(data.get("chunkPos", chunkPos))
	
	if data.has("anatomy"):
		_Anatomy.load_save_data(data["anatomy"])
	if data.has("clothing"):
		_Clothing.load_save_data(data["clothing"])
		
	moved_cell.emit(cellPos)
	moved_chunk.emit(chunkPos)

func _load_vector2(v) -> Vector2:
	if v is Vector2:
		return v
	if v is Dictionary:
		return Vector2(v.get("x", 0), v.get("y", 0))
	if v is String:
		return str_to_var("Vector2" + v) if not v.begins_with("Vector2") else str_to_var(v)
	return Vector2.ZERO

func interact_cell(displacement: Vector2) -> void:
	cellPos += displacement
	moved_cell.emit(cellPos)
	
	var newChunkPos = Vector2(floor(cellPos.x / CHUNK_SIZE), floor(cellPos.y / CHUNK_SIZE))
	
	if chunkPos != newChunkPos:
		chunkPos = newChunkPos
		moved_chunk.emit(chunkPos)
	
	World.update_world_bubble(cellPos)

func equip_item(item_id: String) -> bool:
	return _Equipment.equip_item(item_id)

func unequip_item(item_id: String) -> bool:
	return _Equipment.unequip_item(item_id)
