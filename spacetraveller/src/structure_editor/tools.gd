extends Node
class_name EditorTools

const CURSORS = {
	"pencil": { "tex": preload("res://gfx/mouse/pencil.png"), "hot": "centered" },
	"eyedropper": { "tex": preload("res://gfx/mouse/eyedropper.png"), "hot": "point" },
	"selection": { "tex": preload("res://gfx/mouse/selection.png"), "hot": "centered" }
}

class Tool:
	var editor
	var options = {}
	
	func _init(e): 
		editor = e
		for opt in get_options_config():
			options[opt.name] = opt.default
			
	func on_press(_btn: String, _pos: Vector2i): pass
	func on_release(_btn: String, _pos: Vector2i): pass
	func on_drag(_btn: String, _pos: Vector2i): pass
	func on_hover(_pos: Vector2i): pass
	func on_deactivate(): pass
	
	func get_options_config() -> Array:
		return []
	
	func get_cursor_id() -> String:
		return ""
		
	func get_cursor_config() -> Dictionary:
		var data = CURSORS.get(get_cursor_id(), { "tex": null })
		var tex = data.tex
		if !tex: return { "tex": null, "hot": Vector2.ZERO }
		
		var hot = Vector2.ZERO
		match data.get("hot"):
			"centered":
				hot = tex.get_size() / 2.0
			"point":
				hot = Vector2(0, tex.get_size().y - 1)
			"cursor":
				hot = Vector2.ZERO
		
		return { "tex": tex, "hot": hot }
	
	func is_pos_valid(pos: Vector2i) -> bool:
		if editor.active_selection.size == Vector2i.ZERO:
			return true
		return editor.active_selection.has_point(pos)

class PencilTool extends Tool:
	func get_cursor_id(): return "pencil"

	func on_press(btn: String, pos: Vector2i): 
		editor.save_undo_state()
		_paint(btn, pos)
	func on_drag(btn: String, pos: Vector2i): _paint(btn, pos)
	func on_hover(pos: Vector2i):
		if is_pos_valid(pos):
			editor.Editor.update_preview_tiles([pos], editor.tileID1)
		else:
			editor.Editor.clear_preview_tiles()
	func _paint(btn: String, pos: Vector2i):
		if !is_pos_valid(pos): return
		var id = editor.tileID1 if btn == "left" else editor.tileID2
		editor.place_tile_at(pos, id)

class LineTool extends Tool:
	func get_cursor_id(): return "pencil"

	var is_drawing = false
	var start_pos = Vector2i.ZERO
	var button = ""
	
	func on_press(btn: String, pos: Vector2i):
		if !is_pos_valid(pos): return
		if !is_drawing:
			editor.save_undo_state()
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
			points = points.filter(func(p): return is_pos_valid(p))
			var tid = editor.tileID1 if button == "left" else editor.tileID2
			editor.Editor.update_preview_tiles(points, tid)
		else:
			if is_pos_valid(pos):
				editor.Editor.update_preview_tiles([pos], editor.tileID1)
			else:
				editor.Editor.clear_preview_tiles()

	func _commit(pos: Vector2i):
		var points = editor.get_line_points(start_pos, pos)
		var tid = editor.tileID1 if button == "left" else editor.tileID2
		for p in points:
			if editor.is_inside_bubble(p) and is_pos_valid(p):
				editor.Editor.place_tile(p.x, p.y, tid)
		editor.Editor.update_visuals(Vector2i(0, 0))
		is_drawing = false
		on_hover(pos)

class RectangleTool extends Tool:
	func get_cursor_id(): return "pencil"
	func get_options_config() -> Array:
		return [
			{ "name": "filled", "label": "Filled", "default": false },
			{ "name": "perfect", "label": "Perfect", "default": false }
		]

	var is_drawing = false
	var start_pos = Vector2i.ZERO
	var button = ""

	func on_press(btn: String, pos: Vector2i):
		if !is_pos_valid(pos): return
		if !is_drawing:
			editor.save_undo_state()
			is_drawing = true
			start_pos = pos
			button = btn
			on_hover(pos)

	func on_release(btn: String, pos: Vector2i):
		if is_drawing and btn == button:
			_commit(pos)

	func on_hover(pos: Vector2i):
		if is_drawing:
			var tid = editor.tileID1 if button == "left" else editor.tileID2
			editor.Editor.update_preview_shape(StructureEditor.SHAPE_RECTANGLE, start_pos, pos, options.filled, options.perfect, tid)
		else:
			if is_pos_valid(pos):
				editor.Editor.update_preview_tiles([pos], editor.tileID1)
			else:
				editor.Editor.clear_preview_tiles()

	func _commit(pos: Vector2i):
		var tid = editor.tileID1 if button == "left" else editor.tileID2
		editor.Editor.commit_shape(StructureEditor.SHAPE_RECTANGLE, start_pos, pos, options.filled, options.perfect, tid)
		is_drawing = false
		on_hover(pos)

class EllipsisTool extends Tool:
	func get_cursor_id(): return "pencil"
	func get_options_config() -> Array:
		return [
			{ "name": "filled", "label": "Filled", "default": false },
			{ "name": "perfect", "label": "Perfect", "default": false }
		]

	var is_drawing = false
	var start_pos = Vector2i.ZERO
	var button = ""

	func on_press(btn: String, pos: Vector2i):
		if !is_pos_valid(pos): return
		if !is_drawing:
			editor.save_undo_state()
			is_drawing = true
			start_pos = pos
			button = btn
			on_hover(pos)

	func on_release(btn: String, pos: Vector2i):
		if is_drawing and btn == button:
			_commit(pos)

	func on_hover(pos: Vector2i):
		if is_drawing:
			var tid = editor.tileID1 if button == "left" else editor.tileID2
			editor.Editor.update_preview_shape(StructureEditor.SHAPE_ELLIPSIS, start_pos, pos, options.filled, options.perfect, tid)
		else:
			if is_pos_valid(pos):
				editor.Editor.update_preview_tiles([pos], editor.tileID1)
			else:
				editor.Editor.clear_preview_tiles()

	func _commit(pos: Vector2i):
		var tid = editor.tileID1 if button == "left" else editor.tileID2
		editor.Editor.commit_shape(StructureEditor.SHAPE_ELLIPSIS, start_pos, pos, options.filled, options.perfect, tid)
		is_drawing = false
		on_hover(pos)

class EyedropperTool extends Tool:
	func get_cursor_id(): return "eyedropper"

	func on_press(btn: String, pos: Vector2i):
		if !editor.is_inside_bubble(pos): return
		var id = editor.Editor.get_tile_at(pos.x, pos.y)
		editor.select_tile(id, btn == "left")
	func on_hover(_pos: Vector2i):
		editor.Editor.clear_preview_tiles()

class FillTool extends Tool:
	func get_cursor_id(): return "pencil"

	func get_options_config() -> Array:
		return [
			{ "name": "contiguous", "label": "Contiguous", "default": true }
		]
		
	func on_press(btn: String, pos: Vector2i):
		if !editor.is_inside_bubble(pos): return
		editor.save_undo_state()
		var tid = editor.tileID1 if btn == "left" else editor.tileID2
		
		if editor.active_selection.size != Vector2i.ZERO:
			var inside = editor.active_selection.has_point(pos)
			editor.Editor.fill_tiles(pos.x, pos.y, tid, editor.active_selection, !inside, options.contiguous)
		else:
			editor.Editor.fill_tiles(pos.x, pos.y, tid, Rect2i(), false, options.contiguous)
			
		editor.Editor.update_visuals(Vector2i(0, 0))
	func on_hover(_pos: Vector2i):
		editor.Editor.clear_preview_tiles()

class SelectionTool extends Tool:
	func get_cursor_id(): return "selection"

	var selection_rect : Rect2i = Rect2i()
	var is_selecting = false
	var is_moving = false
	var is_floating = false
	var drag_start_pos = Vector2i.ZERO
	var move_offset = Vector2i.ZERO
	var captured_tiles = {}

	func on_press(btn: String, pos: Vector2i):
		if btn != "left": return
		
		var rect = selection_rect
		if is_moving: rect.position += move_offset
		
		if rect.size != Vector2i.ZERO and rect.has_point(pos):
			# Start moving
			is_moving = true
			is_selecting = false
			drag_start_pos = pos
			move_offset = Vector2i.ZERO
			if !is_floating:
				_capture_and_cut_tiles()
		else:
			# Start new selection
			if is_floating:
				_commit_move()
				
			is_selecting = true
			is_moving = false
			is_floating = false
			drag_start_pos = pos
			selection_rect = Rect2i(pos, Vector2i.ZERO)
			_update_visuals()

	func on_drag(btn: String, pos: Vector2i):
		if btn != "left": return
		
		if is_selecting:
			var min_pos = Vector2i(min(drag_start_pos.x, pos.x), min(drag_start_pos.y, pos.y))
			var max_pos = Vector2i(max(drag_start_pos.x, pos.x), max(drag_start_pos.y, pos.y))
			selection_rect = Rect2i(min_pos, max_pos - min_pos + Vector2i.ONE)
			_update_visuals()
		elif is_moving:
			move_offset = pos - drag_start_pos
			_update_visuals()
			_preview_tiles()

	func on_release(btn: String, _pos: Vector2i):
		if btn != "left": return
		
		if is_selecting:
			is_selecting = false
			if selection_rect.size == Vector2i.ONE:
				selection_rect = Rect2i()
			_update_visuals()
		elif is_moving:
			selection_rect.position += move_offset
			move_offset = Vector2i.ZERO
			is_moving = false
			is_floating = true
			_update_visuals()
			_preview_tiles()

	func on_hover(_pos: Vector2i):
		if is_floating:
			_preview_tiles()
		else:
			editor.Editor.clear_preview_tiles()

	func on_deactivate():
		if is_floating:
			_commit_move()
		editor.Editor.clear_preview_tiles()

	func _capture_and_cut_tiles():
		editor.save_undo_state()
		captured_tiles.clear()
		for x in range(selection_rect.position.x, selection_rect.end.x):
			for y in range(selection_rect.position.y, selection_rect.end.y):
				var p = Vector2i(x, y)
				var tid = editor.Editor.get_tile_at(x, y)
				if tid != "void":
					captured_tiles[p - selection_rect.position] = tid
					editor.Editor.place_tile(x, y, "void")
		
		if captured_tiles.is_empty():
			is_floating = false
			return

		editor.Editor.update_visuals(Vector2i(0, 0))
		is_floating = true

	func _preview_tiles():
		var preview_data = {}
		var current_pos = selection_rect.position + move_offset
		for rel_p in captured_tiles.keys():
			preview_data[current_pos + rel_p] = captured_tiles[rel_p]
		editor.Editor.update_preview_tiles_with_data(preview_data)

	func _update_visuals():
		var visual = editor.SelectionVisual
		if !visual: return
		
		if selection_rect.size == Vector2i.ZERO:
			visual.visible = false
			editor.active_selection = Rect2i()
			return
			
		visual.visible = true
		var rect = selection_rect
		rect.position += move_offset
		editor.active_selection = rect
			
		var cell_size = editor.Editor.get_cell_size()
		var p1 = Vector2(rect.position) * cell_size
		var p2 = Vector2(rect.end) * cell_size
		
		var points = PackedVector2Array([
			Vector2(p1.x, p1.y),
			Vector2(p2.x, p1.y),
			Vector2(p2.x, p2.y),
			Vector2(p1.x, p2.y)
		])
		visual.points = points

	func _commit_move():
		if !is_floating: return
		
		for rel_p in captured_tiles.keys():
			var p = selection_rect.position + rel_p
			if editor.is_inside_bubble(p):
				editor.Editor.place_tile(p.x, p.y, captured_tiles[rel_p])
		
		editor.Editor.update_visuals(Vector2i(0, 0))
		captured_tiles.clear()
		is_floating = false
		_update_visuals()
