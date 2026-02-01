extends Camera2D

# Camera for the game world

@export var side_panel: Control

func _ready() -> void:
	get_viewport().size_changed.connect(_update_offset)
	_update_offset.call_deferred()

func _process(_delta: float) -> void:
	_update_offset()

func _update_offset() -> void:
	if not side_panel: return
	var panel_width = side_panel.size.x
	
	offset.x = (panel_width / zoom.x) / 2.0
