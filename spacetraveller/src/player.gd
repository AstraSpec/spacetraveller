extends Sprite2D

@export var World : WorldGeneration

signal movedCell(cellPos :Vector2)
signal movedChunk(chunkPos :Vector2)

var CHUNK_SIZE = WorldGeneration.get_chunk_size()

var cellPos : Vector2 = Vector2.ZERO
var chunkPos : Vector2 = Vector2.ZERO

func _ready():
	InputManager.directional_input.connect(_on_movement_triggered)

func _on_movement_triggered(dir: Vector2):
	interact_cell(dir)

func interact_cell(displacement: Vector2) -> void:
	cellPos += displacement
	movedCell.emit(cellPos)
	
	var newChunkPos = Vector2(floor(cellPos.x / CHUNK_SIZE), floor(cellPos.y / CHUNK_SIZE))
	
	if chunkPos != newChunkPos:
		chunkPos = newChunkPos
		movedChunk.emit(chunkPos)
	
	World.update_world_bubble(cellPos)
