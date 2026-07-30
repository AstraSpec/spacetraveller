extends MarginContainer

@export var _GameWorld: GameWorld
@export var container: VBoxContainer

const CAPACITY_ORDER := ["moving", "manipulation", "perception", "speech"]
const COLOR_TEXT := Color(0.88, 0.9, 0.91, 1.0)
const COLOR_MUTED := Color(0.54, 0.58, 0.62, 1.0)
const COLOR_POSITIVE := Color(0.47, 0.78, 0.49, 1.0)
const COLOR_ATTENTION := Color(0.85, 0.72, 0.36, 1.0)
const COLOR_DANGER := Color(0.89, 0.43, 0.43, 1.0)


func _ready() -> void:
	call_deferred("refresh_view")


func refresh_view() -> void:
	if not container or not _GameWorld:
		return

	for child in container.get_children():
		container.remove_child(child)
		child.queue_free()

	var status := _GameWorld.get_entity_condition_status(0)
	if status.is_empty():
		_add_empty_message("Condition information is unavailable.")
		return

	_build_summary(status)
	container.add_child(HSeparator.new())

	var columns := HBoxContainer.new()
	columns.size_flags_vertical = Control.SIZE_EXPAND_FILL
	columns.add_theme_constant_override("separation", 18)
	container.add_child(columns)

	_build_capacities(columns, status.get("capacities", {}))
	_build_injuries(columns, status)


func _build_summary(status: Dictionary) -> void:
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 8)

	_add_summary_value(row, "Body", float(status.get("body_integrity", 0.0)))
	_add_summary_gap(row)
	_add_summary_value(row, "Blood", float(status.get("blood", 0.0)))
	_add_summary_gap(row)
	_add_summary_value(row, "Stamina", float(status.get("stamina", 0.0)))
	container.add_child(row)

	var physiology_row := HBoxContainer.new()
	physiology_row.add_theme_constant_override("separation", 8)
	_add_summary_value(
		physiology_row,
		"Consciousness",
		float(status.get("consciousness", 0.0)))
	_add_summary_gap(physiology_row)
	_add_summary_value(
		physiology_row,
		"Pain",
		float(status.get("pain", 0.0)))
	if bool(status.get("downed", false)):
		_add_summary_gap(physiology_row)
		var downed := Label.new()
		downed.text = "DOWNED"
		downed.add_theme_color_override("font_color", COLOR_DANGER)
		physiology_row.add_child(downed)
	physiology_row.tooltip_text = "Consciousness ceiling: %d%%\nUntreated pain floor: %d%%\nPain accuracy: %+.0f points\nConsciousness accuracy: %+.0f points" % [
		roundi(float(status.get("consciousness_ceiling", 1.0)) * 100.0),
		roundi(float(status.get("pain_floor", 0.0)) * 100.0),
		float(status.get("pain_accuracy_modifier", 0.0)) * 100.0,
		float(status.get("consciousness_accuracy_modifier", 0.0)) * 100.0,
	]
	container.add_child(physiology_row)


func _add_summary_gap(row: HBoxContainer) -> void:
	var gap := Control.new()
	gap.custom_minimum_size.x = 18.0
	row.add_child(gap)


func _add_summary_value(row: HBoxContainer, label_text: String, value: float) -> void:
	var label := Label.new()
	label.text = label_text
	label.add_theme_color_override("font_color", Color.WHITE)
	row.add_child(label)
	var value_label := Label.new()
	value_label.text = "%d%%" % roundi(clampf(value, 0.0, 1.0) * 100.0)
	value_label.add_theme_color_override("font_color", _percentage_color(value))
	row.add_child(value_label)


func _build_capacities(parent: HBoxContainer, capacities: Dictionary) -> void:
	var panel := PanelContainer.new()
	panel.custom_minimum_size.x = 280.0
	panel.size_flags_vertical = Control.SIZE_EXPAND_FILL
	parent.add_child(panel)

	var margin := MarginContainer.new()
	_set_panel_margins(margin)
	panel.add_child(margin)

	var content := VBoxContainer.new()
	content.add_theme_constant_override("separation", 12)
	margin.add_child(content)
	content.add_child(_section_title("CAPACITIES"))

	for capacity_id in CAPACITY_ORDER:
		var capacity: Dictionary = capacities.get(capacity_id, {})
		if capacity.is_empty():
			continue
		content.add_child(_capacity_row(capacity))


func _capacity_row(capacity: Dictionary) -> Control:
	var row := HBoxContainer.new()
	var label := Label.new()
	label.text = str(capacity.get("label", capacity.get("id", "")))
	label.add_theme_color_override("font_color", COLOR_TEXT)
	row.add_child(label)

	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(spacer)

	var value := clampf(float(capacity.get("value", 0.0)), 0.0, 1.0)
	var value_label := Label.new()
	value_label.text = "%d%%" % roundi(value * 100.0)
	if value <= 0.0:
		value_label.text += " - Incapable"
	value_label.add_theme_color_override("font_color", _percentage_color(value))
	row.add_child(value_label)

	var anatomical := float(capacity.get("anatomical", value))
	if not is_equal_approx(anatomical, value):
		row.tooltip_text = "Body: %d%%\nCurrent effective value: %d%%" % [
			roundi(anatomical * 100.0),
			roundi(value * 100.0),
		]
	return row


func _build_injuries(parent: HBoxContainer, status: Dictionary) -> void:
	var panel := PanelContainer.new()
	panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	panel.size_flags_vertical = Control.SIZE_EXPAND_FILL
	parent.add_child(panel)

	var margin := MarginContainer.new()
	_set_panel_margins(margin)
	panel.add_child(margin)

	var content := VBoxContainer.new()
	content.size_flags_vertical = Control.SIZE_EXPAND_FILL
	content.add_theme_constant_override("separation", 8)
	margin.add_child(content)
	content.add_child(_section_title("INJURIES AND CONDITIONS"))

	var relevant_parts: Array[Dictionary] = []
	for value in status.get("parts", []):
		if not value is Dictionary:
			continue
		var part: Dictionary = value
		var integrity := float(part.get("integrity", 1.0))
		var effects: Array = part.get("effects", [])
		if integrity < 0.999 or not effects.is_empty():
			relevant_parts.append(part)

	var body_effects: Array = status.get("body_effects", [])
	if relevant_parts.is_empty() and body_effects.is_empty():
		var healthy := Label.new()
		healthy.text = "No injuries or conditions."
		healthy.add_theme_color_override("font_color", COLOR_MUTED)
		content.add_child(healthy)
		return

	for part in relevant_parts:
		var integrity := clampf(float(part.get("integrity", 1.0)), 0.0, 1.0)
		var part_row := HBoxContainer.new()
		var tooltip := _part_tooltip(part)
		part_row.tooltip_text = tooltip

		var part_label := Label.new()
		part_label.text = str(part.get("display_name", part.get("type_id", "Body part")))
		var effects: Array = part.get("effects", [])
		part_label.add_theme_color_override(
			"font_color",
			COLOR_DANGER if integrity >= 0.999 and not effects.is_empty() else _percentage_color(integrity)
		)
		part_label.mouse_filter = Control.MOUSE_FILTER_STOP
		part_label.tooltip_text = tooltip
		part_row.add_child(part_label)

		var spacer := Control.new()
		spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		part_row.add_child(spacer)

		var integrity_label := Label.new()
		integrity_label.text = "%d%%" % roundi(integrity * 100.0)
		integrity_label.add_theme_color_override("font_color", _percentage_color(integrity))
		integrity_label.mouse_filter = Control.MOUSE_FILTER_STOP
		integrity_label.tooltip_text = tooltip
		part_row.add_child(integrity_label)
		content.add_child(part_row)

	for effect_value in body_effects:
		if not effect_value is Dictionary:
			continue
		var effect_label := Label.new()
		effect_label.text = str(effect_value.get("type", "Condition")).capitalize()
		effect_label.add_theme_color_override("font_color", COLOR_DANGER)
		effect_label.mouse_filter = Control.MOUSE_FILTER_STOP
		effect_label.tooltip_text = _effect_text(effect_value)
		content.add_child(effect_label)


func _part_tooltip(part: Dictionary) -> String:
	var integrity := clampf(float(part.get("integrity", 1.0)), 0.0, 1.0)
	var max_integrity := maxf(0.0, float(part.get("max_integrity", 0.0)))
	var current_integrity := integrity * max_integrity
	var lines: Array[String] = [
		"Hit points: %s/%s" % [
			_format_hit_points(current_integrity),
			_format_hit_points(max_integrity),
		]
	]
	for effect_value in part.get("effects", []):
		if effect_value is Dictionary:
			lines.append(_effect_text(effect_value))
	return "\n".join(lines)


func _format_hit_points(value: float) -> String:
	if is_equal_approx(value, roundf(value)):
		return str(roundi(value))
	return "%.1f" % value


func _effect_text(effect: Dictionary) -> String:
	var effect_type := str(effect.get("type", "condition"))
	var display_name := "Bleeding" if effect_type == "bleed" else effect_type.capitalize()
	var magnitude := clampf(float(effect.get("magnitude", 0.0)), 0.0, 1.0)
	return "%s  %d%%" % [display_name, roundi(magnitude * 100.0)]


func _section_title(text: String) -> Label:
	var label := Label.new()
	label.text = text
	label.add_theme_color_override("font_color", COLOR_TEXT)
	return label


func _set_panel_margins(margin: MarginContainer) -> void:
	margin.add_theme_constant_override("margin_left", 14)
	margin.add_theme_constant_override("margin_top", 12)
	margin.add_theme_constant_override("margin_right", 14)
	margin.add_theme_constant_override("margin_bottom", 12)


func _percentage_color(value: float) -> Color:
	var normalized := clampf(value, 0.0, 1.0)
	if normalized >= 0.5:
		return COLOR_ATTENTION.lerp(COLOR_POSITIVE, (normalized - 0.5) * 2.0)
	return COLOR_DANGER.lerp(COLOR_ATTENTION, normalized * 2.0)


func _add_empty_message(text: String) -> void:
	var label := Label.new()
	label.text = text
	label.add_theme_color_override("font_color", COLOR_MUTED)
	container.add_child(label)
