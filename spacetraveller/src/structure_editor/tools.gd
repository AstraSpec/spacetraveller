extends Node
class_name EditorTools

class Tool:
	var editor
	func _init(e): editor = e
	func on_press(_btn: String, _pos: Vector2i): pass
	func on_release(_btn: String, _pos: Vector2i): pass
	func on_drag(_btn: String, _pos: Vector2i): pass
	func on_hover(_pos: Vector2i): pass

class PencilTool extends Tool:
	func on_press(btn: String, pos: Vector2i): _paint(btn, pos)
	func on_drag(btn: String, pos: Vector2i): _paint(btn, pos)
	func on_hover(pos: Vector2i):
		editor.Editor.update_preview_tiles([pos], editor.tileID1)
	func _paint(btn: String, pos: Vector2i):
		var id = editor.tileID1 if btn == "left" else editor.tileID2
		editor.place_tile_at(pos, id)

class LineTool extends Tool:
	var is_drawing = false
	var start_pos = Vector2i.ZERO
	var button = ""
	
	func on_press(btn: String, pos: Vector2i):
		if !is_drawing:
			is_drawing = true
			start_pos = pos
			button = btn
			on_hover(pos)
			
	func on_release(btn: String, pos: Vector2i):
		if is_drawing and btn == button:
			_commit(pos)
			
	func on_hover(pos: Vector2i):
		if is_drawing:
			var points = editor.get_line_points(start_pos, pos)
			var tid = editor.tileID1 if button == "left" else editor.tileID2
			editor.Editor.update_preview_tiles(points, tid)
		else:
			editor.Editor.update_preview_tiles([pos], editor.tileID1)

	func _commit(pos: Vector2i):
		var points = editor.get_line_points(start_pos, pos)
		var tid = editor.tileID1 if button == "left" else editor.tileID2
		for p in points:
			if editor.is_inside_bubble(p):
				editor.Editor.place_tile(p.x, p.y, tid)
		editor.Editor.update_visuals(Vector2i(0, 0))
		is_drawing = false
		on_hover(pos)

class EyedropperTool extends Tool:
	func on_press(btn: String, pos: Vector2i):
		if !editor.is_inside_bubble(pos): return
		var id = editor.Editor.get_tile_at(pos.x, pos.y)
		editor.select_tile(id, btn == "left")
	func on_hover(_pos: Vector2i):
		editor.Editor.clear_preview_tiles()

class FillTool extends Tool:
	func on_press(btn: String, pos: Vector2i):
		if !editor.is_inside_bubble(pos): return
		var tid = editor.tileID1 if btn == "left" else editor.tileID2
		editor.Editor.fill_tiles(pos.x, pos.y, tid)
		editor.Editor.update_visuals(Vector2i(0, 0))
	func on_hover(_pos: Vector2i):
		editor.Editor.clear_preview_tiles()
