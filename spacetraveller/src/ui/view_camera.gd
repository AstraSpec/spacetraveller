extends Camera2D
class_name ViewCamera

@export var ZOOM_LVL : Array[float] = [0.75, 1.0, 1.5, 2.0]
@export var DRAG_SPEED : float = 1.75
@export var limits : Rect2 = Rect2(-1e8, -1e8, 2e8, 2e8)
@export var margins : MarginContainer
@export var viewport : SubViewport
var centerNode

var zoomID : int = 1

func _ready() -> void:
	InputManager.view_panned.connect(_view_panned)
	InputManager.view_zoomed.connect(_view_zoomed)
	InputManager.view_centered.connect(_view_centered)
	
	zoom = Vector2(ZOOM_LVL[zoomID], ZOOM_LVL[zoomID])

func _view_panned(relative: Vector2):
	_update_camera_pos(position - relative * DRAG_SPEED / zoom.x)

func _view_zoomed(z: int):
	var oldZoomID = zoomID
	zoomID = clamp(zoomID + z, 0, ZOOM_LVL.size() - 1)
	
	if oldZoomID != zoomID:
		zoom = Vector2(ZOOM_LVL[zoomID], ZOOM_LVL[zoomID])
		_update_camera_pos(position)

func _view_centered():
	var target = centerNode.position if centerNode else Vector2.ZERO
	_update_camera_pos(target)

func _update_camera_pos(newPos: Vector2):
	if margins:
		var viewSize = margins.size
		if margins is MarginContainer:
			viewSize = Vector2(
				margins.size.x 
				- margins.get_theme_constant("margin_left") 
				- margins.get_theme_constant("margin_right"),
				margins.size.y 
				- margins.get_theme_constant("margin_top") 
				- margins.get_theme_constant("margin_bottom")
			)
			
		var halfSize = (viewSize / zoom.x) / 2.0
		
		var minPos = limits.position + halfSize
		var maxPos = limits.end - halfSize
		
		if maxPos.x < minPos.x:
			newPos.x = limits.position.x + limits.size.x / 2.0
		else:
			newPos.x = clamp(newPos.x, minPos.x, maxPos.x)
			
		if maxPos.y < minPos.y:
			newPos.y = limits.position.y + limits.size.y / 2.0
		else:
			newPos.y = clamp(newPos.y, minPos.y, maxPos.y)
	else:
		newPos.x = clamp(newPos.x, limits.position.x, limits.end.x)
		newPos.y = clamp(newPos.y, limits.position.y, limits.end.y)

	position = newPos.round()
	
	if viewport:
		viewport.render_target_update_mode = SubViewport.UPDATE_ONCE
