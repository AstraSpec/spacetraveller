extends Sprite2D

signal movedCell(cellPos :Vector2)
signal movedChunk(chunkPos :Vector2)

@onready var InteractionCell :PackedScene = preload("res://src/interaction_cell.tscn")
@export var World : WorldGeneration
@export var inventory :Inventory

const DIR :Array[Vector2] = [Vector2.UP, Vector2.DOWN, Vector2.LEFT, Vector2.RIGHT]

var CHUNK_SIZE = WorldGeneration.get_chunk_size()

var cellPos : Vector2 = Vector2.ZERO
var chunkPos : Vector2 = Vector2.ZERO

var currentAction: PlayerAction = null
var interactionCells : Array[Node2D] = []

func _ready():
	InputManager.directional_input.connect(_on_movement_triggered)
	InputManager.action_smash_requested.connect(_on_smash_requested)
	InputManager.action_pickup_requested.connect(_on_pickup_requested)

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
	if currentAction:
		var target_cell = Vector2i(cellPos) + Vector2i(dir)
		if currentAction.is_valid(target_cell):
			currentAction.execute(target_cell)
			World.update_world_bubble(cellPos)
		
		currentAction = null
		_clear_interaction_cells()
		# Returns to move mode
	else:
		interact_cell(dir)

func interact_cell(displacement: Vector2) -> void:
	cellPos += displacement
	movedCell.emit(cellPos)
	
	var newChunkPos = Vector2(floor(cellPos.x / CHUNK_SIZE), floor(cellPos.y / CHUNK_SIZE))
	
	if chunkPos != newChunkPos:
		chunkPos = newChunkPos
		movedChunk.emit(chunkPos)
	
	World.update_world_bubble(cellPos)
