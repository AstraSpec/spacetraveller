extends Label

func update_text(pos: Vector2i, selection_size: Vector2i = Vector2i.ZERO):
	var text_val = "Coords: (%d, %d)" % [pos.x, pos.y]
	if selection_size != Vector2i.ZERO:
		text_val += " Selection: %dx%d" % [selection_size.x, selection_size.y]
	text = text_val
