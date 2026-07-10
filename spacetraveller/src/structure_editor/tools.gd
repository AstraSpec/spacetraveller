extends Node
class_name EditorTools

const CURSORS = {
	"pencil": { "tex": preload("res://gfx/mouse/pencil.png"), "hot": "centered" },
	"eyedropper": { "tex": preload("res://gfx/mouse/eyedropper.png"), "hot": "point" },
	"selection": { "tex": preload("res://gfx/mouse/selection.png"), "hot": "centered" }
}

class Tool:
	static var clipboard = {
		"contents": {},
		"size": Vector2i.ZERO
	}
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
	func on_key(_key: String): pass
	func on_deactivate(): pass
	
	func get_options_config() -> Array:
		return []
	
	func get_effective_option(index: int) -> bool:
		var config = get_options_config()
		if index >= config.size(): return false
		var opt_name = config[index].name
		var val = options.get(opt_name, false)
		if index == 0 and InputManager.is_shift_pressed: return !val
		if index == 1 and InputManager.is_ctrl_pressed: return !val
		return val

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
		if !editor.is_inside_bubble(pos):
			return false
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
			var entry = editor.get_tool_entry_for_button("left")
			editor.Editor.update_preview_tiles([pos], entry.id, entry.type)
		else:
			editor.Editor.clear_preview_tiles()
	func _paint(btn: String, pos: Vector2i):
		if !is_pos_valid(pos): return
		var entry = editor.get_tool_entry_for_button(btn)
		editor.place_at(pos, entry.id, entry.type)

class LineTool extends Tool:
	func get_cursor_id(): return "pencil"
	func get_options_config() -> Array:
		return [
			{ "name": "snapped", "label": "Snapped", "default": false }
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
			var target = pos
			if get_effective_option(0):
				var delta = Vector2(pos - start_pos)
				if delta != Vector2.ZERO:
					var angle = round(delta.angle() / (PI/4.0)) * (PI/4.0)
					var dist = delta.length()
					var snapped_delta = Vector2.from_angle(angle) * dist
					target = start_pos + Vector2i(round(snapped_delta.x), round(snapped_delta.y))

			var points = editor.get_line_points(start_pos, target)
			points = points.filter(func(p): return is_pos_valid(p))
			var entry = editor.get_tool_entry_for_button(button)
			editor.Editor.update_preview_tiles(points, entry.id, entry.type)
		else:
			if is_pos_valid(pos):
				var entry = editor.get_tool_entry_for_button("left")
				editor.Editor.update_preview_tiles([pos], entry.id, entry.type)
			else:
				editor.Editor.clear_preview_tiles()

	func _commit(pos: Vector2i):
		var target = pos
		if get_effective_option(0):
			var delta = Vector2(pos - start_pos)
			if delta != Vector2.ZERO:
				var angle = round(delta.angle() / (PI/4.0)) * (PI/4.0)
				var dist = delta.length()
				var snapped_delta = Vector2.from_angle(angle) * dist
				target = start_pos + Vector2i(round(snapped_delta.x), round(snapped_delta.y))

		var points = editor.get_line_points(start_pos, target)
		var entry = editor.get_tool_entry_for_button(button)
		for p in points:
			if editor.is_inside_bubble(p) and is_pos_valid(p):
				editor.place_entry(Vector2i(p.x + editor.playerOffset.x, p.y + editor.playerOffset.y), entry.id, entry.type)
		editor.update_editor_visuals()
		is_drawing = false
		on_hover(pos)

class ShapeTool extends Tool:
	var shape_type: int
	
	func _init(e, p_shape_type: int):
		super(e)
		shape_type = p_shape_type

	func get_cursor_id(): return "pencil"
	func get_options_config() -> Array:
		return [
			{ "name": "perfect", "label": "Perfect", "default": false },
			{ "name": "filled", "label": "Filled", "default": false }
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
			var entry = editor.get_tool_entry_for_button(button)
			var points = editor.Editor.get_shape_points(shape_type, start_pos, pos, get_effective_option(0), get_effective_option(1))
			points = points.filter(func(p): return editor.is_inside_bubble(p))
			editor.Editor.update_preview_tiles(points, entry.id, entry.type)
		else:
			if is_pos_valid(pos):
				var entry = editor.get_tool_entry_for_button("left")
				editor.Editor.update_preview_tiles([pos], entry.id, entry.type)
			else:
				editor.Editor.clear_preview_tiles()

	func _commit(pos: Vector2i):
		var entry = editor.get_tool_entry_for_button(button)
		editor.commit_shape(shape_type, start_pos + Vector2i(editor.playerOffset), pos + Vector2i(editor.playerOffset), get_effective_option(0), get_effective_option(1), entry.id, entry.type)
		is_drawing = false
		on_hover(pos)

class EyedropperTool extends Tool:
	func get_cursor_id(): return "eyedropper"

	func on_press(btn: String, pos: Vector2i):
		if !editor.is_inside_bubble(pos): return
		var base_type: String = editor._content_type_for_tab(editor.ContentTabs.current_tab) if editor.ContentTabs else "tile"
		editor.select_editor_content_at(pos, btn != "right", base_type)
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
		var entry = editor.get_tool_entry_for_button(btn)
		if entry.type != "tile" or str(entry.id).is_empty(): return
		editor.save_undo_state()
		
		if editor.active_selection.size != Vector2i.ZERO:
			editor.World.fill_tiles(pos.x + editor.playerOffset.x, pos.y + editor.playerOffset.y, entry.id, editor.playerOffset, editor.active_selection, false, get_effective_option(0))
		else:
			editor.World.fill_tiles(pos.x + editor.playerOffset.x, pos.y + editor.playerOffset.y, entry.id, editor.playerOffset, Rect2i(editor.editor_area_origin, editor.editor_area_size), false, get_effective_option(0))
			
		editor.update_editor_visuals()
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
	var captured_contents = {}

	func on_press(btn: String, pos: Vector2i):
		if btn != "left": return
		if !editor.is_inside_bubble(pos): return
		
		var rect = selection_rect
		if is_moving: rect.position += move_offset
		
		if rect.size != Vector2i.ZERO and rect.has_point(pos):
			# Start moving
			is_moving = true
			is_selecting = false
			drag_start_pos = pos
			move_offset = Vector2i.ZERO
			if !is_floating:
				_capture_and_cut_contents()
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
			var bounds := Rect2i(editor.editor_area_origin, editor.editor_area_size)
			var clamped_pos := Vector2i(clampi(pos.x, bounds.position.x, bounds.end.x - 1), clampi(pos.y, bounds.position.y, bounds.end.y - 1))
			var min_pos = Vector2i(min(drag_start_pos.x, clamped_pos.x), min(drag_start_pos.y, clamped_pos.y))
			var max_pos = Vector2i(max(drag_start_pos.x, clamped_pos.x), max(drag_start_pos.y, clamped_pos.y))
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
			if selection_rect.size == Vector2i.ONE and editor.get_editor_contents_at(selection_rect.position).is_empty():
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

	func on_key(key: String):
		match key:
			"delete":
				_delete_selection()
			"copy":
				_copy_to_clipboard()
			"cut":
				_cut_to_clipboard()
			"paste":
				_paste_from_clipboard()
			"undo", "redo":
				if is_floating:
					captured_contents.clear()
					is_floating = false
					_update_visuals()
					editor.Editor.clear_preview_tiles()

	func on_deactivate():
		if is_floating:
			_commit_move()
		editor.Editor.clear_preview_tiles()

	func _copy_to_clipboard():
		if selection_rect.size == Vector2i.ZERO: return
		Tool.clipboard.contents = editor.capture_editor_contents(selection_rect)
		Tool.clipboard.size = selection_rect.size

	func _cut_to_clipboard():
		if selection_rect.size == Vector2i.ZERO: return
		editor.save_undo_state()
		Tool.clipboard.contents = editor.capture_editor_contents(selection_rect, true)
		Tool.clipboard.size = selection_rect.size
		editor.update_editor_visuals()

	func _paste_from_clipboard():
		if Tool.clipboard.contents.is_empty(): return
		
		if is_floating:
			_commit_move()
			
		editor.save_undo_state()
		captured_contents = Tool.clipboard.contents.duplicate(true)
		selection_rect = Rect2i(editor.mousePos, Tool.clipboard.size)
		move_offset = Vector2i.ZERO
		is_floating = true
		is_moving = false
		is_selecting = false
		_update_visuals()
		_preview_tiles()

	func _delete_selection():
		if is_floating:
			captured_contents.clear()
			is_floating = false
			_update_visuals()
			editor.Editor.clear_preview_tiles()
		elif selection_rect.size != Vector2i.ZERO:
			editor.save_undo_state()
			editor.capture_editor_contents(selection_rect, true)
			editor.update_editor_visuals()

	func _capture_and_cut_contents():
		editor.save_undo_state()
		captured_contents = editor.capture_editor_contents(selection_rect, true)
		
		if captured_contents.is_empty():
			is_floating = false
			return

		editor.update_editor_visuals()
		is_floating = true

	func _preview_tiles():
		var preview_data = {}
		var current_pos = selection_rect.position + move_offset
		for rel_p_variant in captured_contents.keys():
			if !(rel_p_variant is Vector2i):
				continue
			var contents_variant: Variant = captured_contents[rel_p_variant]
			if !(contents_variant is Dictionary):
				continue
			var rel_p: Vector2i = rel_p_variant
			var contents: Dictionary = contents_variant
			if contents.has("tile") and editor.is_inside_bubble(current_pos + rel_p):
				preview_data[current_pos + rel_p] = contents["tile"]
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
			
		var cell_size = editor.FastTilemap.get_cell_size()
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

		var bounded_contents := {}
		for rel_p_variant in captured_contents.keys():
			if rel_p_variant is Vector2i and editor.is_inside_bubble(selection_rect.position + rel_p_variant):
				bounded_contents[rel_p_variant] = captured_contents[rel_p_variant]
		editor.place_editor_contents(selection_rect.position, bounded_contents)
		
		editor.update_editor_visuals()
		captured_contents.clear()
		is_floating = false
		_update_visuals()
