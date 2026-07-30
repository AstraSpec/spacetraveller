extends TabContainer

@export var _GameWorld :GameWorld
@export var DayLabel :HBoxContainer
@export var TimeLabel :HBoxContainer
@export var SunLabel :HBoxContainer
@export var StaminaValue :Label
@export var StaminaBar :ProgressBar
@export var ConsciousnessValue :Label
@export var ConsciousnessBar :ProgressBar
@export var PainValue :Label
@export var PainBar :ProgressBar
@export var BloodValue :Label
@export var BloodBar :ProgressBar
@export var BodyGrid :GridContainer
@export var LookController :Node

var _inspection_pos := Vector2i.ZERO
var _look_mode_active := false

const VITALS_TAB := 0
const LOOK_TAB := 1
const COLOR_TEXT := Color(0.88, 0.9, 0.91, 1.0)
const COLOR_MUTED := Color(0.54, 0.58, 0.62, 1.0)
const COLOR_POSITIVE := Color(0.47, 0.78, 0.49, 1.0)
const COLOR_ATTENTION := Color(0.85, 0.72, 0.36, 1.0)
const COLOR_DANGER := Color(0.89, 0.43, 0.43, 1.0)
const COLOR_CONSCIOUSNESS := Color(0.62, 0.64, 0.68, 1.0)
const COLOR_BODY_HEALTHY := Color(0.48, 0.60, 0.50, 1.0)
const COLOR_BODY_WOUNDED := Color(0.77, 0.60, 0.35, 1.0)
const COLOR_BODY_CRITICAL := Color(0.78, 0.36, 0.36, 1.0)
const COLOR_BODY_LABEL := Color(0.70, 0.72, 0.74, 1.0)
const COLOR_WARNING_ORANGE := Color(0.90, 0.55, 0.28, 1.0)
const COLOR_CONDITION_RED := Color(0.78, 0.24, 0.24, 1.0)

@onready var LookTitle: Label = $Look/VBoxContainer/Header/Title
@onready var LookCoordinates: Label = $Look/VBoxContainer/Header/Coordinates
@onready var VisibilityLabel: Label = $Look/VBoxContainer/VisibilityLabel
@onready var TileSection: VBoxContainer = $Look/VBoxContainer/TileSection
@onready var TerrainValue: Label = $Look/VBoxContainer/TileSection/Grid/TerrainValue
@onready var MovementValue: Label = $Look/VBoxContainer/TileSection/Grid/MovementValue
@onready var LightLabel: Label = $Look/VBoxContainer/TileSection/Grid/LightLabel
@onready var LightValue: Label = $Look/VBoxContainer/TileSection/Grid/LightValue
@onready var DarknessNoteMargin: MarginContainer = $Look/VBoxContainer/TileSection/DarknessNoteMargin
@onready var DarknessNote: Label = $Look/VBoxContainer/TileSection/DarknessNoteMargin/DarknessNote
@onready var CreatureSection: VBoxContainer = $Look/VBoxContainer/CreatureSection
@onready var CreatureName: Label = $Look/VBoxContainer/CreatureSection/Grid/NameValue
@onready var AttitudeLabel: Label = $Look/VBoxContainer/CreatureSection/Grid/AttitudeLabel
@onready var AttitudeValue: Label = $Look/VBoxContainer/CreatureSection/Grid/AttitudeValue
@onready var SpeedLabel: Label = $Look/VBoxContainer/CreatureSection/Grid/SpeedLabel
@onready var SpeedValue: Label = $Look/VBoxContainer/CreatureSection/Grid/SpeedValue
@onready var ItemsSection: VBoxContainer = $Look/VBoxContainer/ItemsSection
@onready var ItemsTitle: Label = $Look/VBoxContainer/ItemsSection/SectionTitle
@onready var ItemsList: VBoxContainer = $Look/VBoxContainer/ItemsSection/ItemsList
@onready var TextSection: VBoxContainer = $Look/VBoxContainer/TextSection
@onready var WorldText: Label = $Look/VBoxContainer/TextSection/Text
@onready var _body_cells := {
	"head": BodyGrid.get_node("HeadPart") as Control,
	"torso": BodyGrid.get_node("TorsoPart") as Control,
	"left_arm": BodyGrid.get_node("LeftArmPart") as Control,
	"right_arm": BodyGrid.get_node("RightArmPart") as Control,
	"left_leg": BodyGrid.get_node("LeftLegPart") as Control,
	"right_leg": BodyGrid.get_node("RightLegPart") as Control,
}

func _ready() -> void:
	TimeManager.turn_passed.connect(_update_display)
	InputManager.look_mode_changed.connect(_on_look_mode_changed)
	InputManager.ui_next_tab.connect(_on_next_tab)
	InputManager.ui_prev_tab.connect(_on_prev_tab)
	if LookController and LookController.has_signal("inspection_focus_changed"):
		LookController.connect("inspection_focus_changed", Callable(self, "_on_inspection_focus_changed"))
	if _GameWorld:
		_GameWorld.player_action_resolved.connect(_on_player_action_resolved)
		_GameWorld.entity_moved.connect(_on_entity_moved)
		_GameWorld.combat_event.connect(_on_combat_event)
		_GameWorld.smash_event.connect(_on_smash_event)
		_GameWorld.effect_event.connect(_on_effect_event)
		_GameWorld.interact_event.connect(_on_interact_event)
		_GameWorld.player_movement_mode_changed.connect(_on_player_movement_mode_changed)
	if QuestService.quest_completed.is_connected(_on_quest_completed) == false:
		QuestService.quest_completed.connect(_on_quest_completed)
	if QuestService.quest_failed.is_connected(_on_quest_failed) == false:
		QuestService.quest_failed.connect(_on_quest_failed)
	_update_display()
	call_deferred("_update_vitals")
	call_deferred("_initialize_inspection")

func _on_next_tab() -> void:
	if not _can_switch_sidebar_tabs():
		return
	current_tab = (current_tab + 1) % get_tab_count()

func _on_prev_tab() -> void:
	if not _can_switch_sidebar_tabs():
		return
	current_tab = (current_tab - 1 + get_tab_count()) % get_tab_count()

func _can_switch_sidebar_tabs() -> bool:
	match InputManager.current_mode:
		InputManager.InputMode.EXPLORATION, InputManager.InputMode.LOOK:
			return true
		_:
			return false

func _on_player_action_resolved(_entity_id: int, _cost: float, _next_turn_time: float) -> void:
	_update_vitals()

func _on_combat_event(attacker_id: int, defender_id: int, damage: float, result: String, verb: String, part: String) -> void:
	_update_vitals()
	var attacker_name = _entity_name(attacker_id)
	var defender_name = _entity_name(defender_id)
	var category = "combat_player" if attacker_id == 0 else "combat_enemy"
	var verb_conj = verb if attacker_id == 0 else verb + "s"
	var target = _possessive(defender_id, part)
	var dmg_str = str(int(round(damage)))
	var is_kill = result.ends_with("_kill")
	var is_down = result.ends_with("_down")
	var quality := result.trim_suffix("_kill").trim_suffix("_down")
	var is_crit = quality == "critical"
	var quality_prefix := "[b]%s[/b] " % quality.capitalize()
	var msg = ""

	if result == "exhausted":
		msg = "%s %s too exhausted to attack." % [attacker_name, "are" if attacker_id == 0 else "is"]
		category = "combat_player"
	elif result == "no_limbs":
		msg = "%s %s no functional limbs to strike with." % [attacker_name, "have" if attacker_id == 0 else "has"]
		category = "combat_player"
	elif result == "downed":
		msg = "%s %s too incapacitated to attack." % [attacker_name, "are" if attacker_id == 0 else "is"]
	elif result == "miss":
		msg = "%s %s at %s and misses." % [attacker_name, verb_conj, defender_name]
	elif result == "dodge":
		msg = "%s %s %s attack." % [
			"You" if defender_id == 0 else defender_name,
			"dodge" if defender_id == 0 else "dodges",
			"your" if attacker_id == 0 else "%s's" % attacker_name,
		]
	elif is_kill:
		if defender_id == 0:
			msg = "%s%s %s %s for %s damage and kills you!" % [
				quality_prefix,
				attacker_name,
				verb_conj,
				target,
				dmg_str,
			]
		else:
			msg = "%s%s %s %s for %s damage. %s dies." % [
				quality_prefix,
				attacker_name,
				verb_conj,
				target,
				dmg_str,
				defender_name,
			]
	elif is_down:
		msg = "%s%s %s %s for %s damage and knocks %s down." % [
			quality_prefix,
			attacker_name,
			verb_conj,
			target,
			dmg_str,
			"you" if defender_id == 0 else defender_name,
		]
	else:
		msg = "%s%s %s %s for %s damage." % [
			quality_prefix,
			attacker_name,
			verb_conj,
			target,
			dmg_str,
		]

	EventBus.post(category, msg, {"attacker": attacker_id, "defender": defender_id, "damage": damage, "part": part, "crit": is_crit})

func _possessive(entity_id: int, part: String) -> String:
	if part.is_empty():
		return _entity_name(entity_id)
	if entity_id == 0:
		return "your %s" % part
	return "their %s" % part

func _on_interact_event(entity_id: int, target_id: int) -> void:
	if InputManager.current_mode == InputManager.InputMode.MENU and InputManager.active_menu_id == "conversation":
		return
	if target_id == entity_id:
		return
	if not _GameWorld or not _GameWorld.can_interact_with_entity(target_id):
		return
	if not DialogueService.has_dialogue_for(_GameWorld, target_id):
		return
	InputManager.toggle_menu("conversation", {"target": target_id})

func _on_quest_completed(quest_id: String) -> void:
	var q: Dictionary = QuestService.get_quest(quest_id)
	var label := str(q.get("label", "quest"))
	var message := "Quest completed: %s" % label
	var next_giver := str(q.get("next_giver", ""))
	if not next_giver.is_empty():
		message += " Speak to %s next." % next_giver
	EventBus.post("quest", message, {"quest_id": quest_id, "status": "completed", "next_giver": next_giver})

func _on_quest_failed(quest_id: String) -> void:
	var q: Dictionary = QuestService.get_quest(quest_id)
	var label := str(q.get("label", "quest"))
	EventBus.post("quest_failed", "Quest failed: %s. The quest giver died." % label, {"quest_id": quest_id, "status": "failed"})

func _npc_real_name(target_id: int) -> String:
	if not _GameWorld:
		return "Someone"
	var n := _GameWorld.get_entity_name(target_id)
	return n if not n.is_empty() else "A stranger"

func _on_effect_event(entity_id: int, effect_type: String, note: String, part: String) -> void:
	_update_vitals()
	var actor = "You" if entity_id == 0 else _entity_name(entity_id)
	var subj_be = "are" if entity_id == 0 else "is"
	var possessive = "your" if entity_id == 0 else "their"
	var msg = ""
	if effect_type == "bleed" and note == "onset":
		if part.is_empty():
			msg = "%s %s bleeding." % [actor, subj_be]
		else:
			msg = "%s %s bleeding from %s %s." % [actor, subj_be, possessive, part]
	elif effect_type == "bleed" and note == "stopped":
		if part.is_empty():
			msg = "%s %s stopped bleeding." % [actor, "have" if entity_id == 0 else "has"]
		else:
			msg = "%s %s %s stopped bleeding." % [possessive.capitalize(), part, "has"]
	elif effect_type == "drop_weapon":
		var item_name := str(ItemDb.get_item_name(note))
		if item_name.is_empty():
			item_name = note
		msg = "%s drop%s %s." % [
			actor,
			"" if entity_id == 0 else "s",
			item_name,
		]
	elif effect_type == "downed":
		msg = "%s collapse%s." % [actor, "" if entity_id == 0 else "s"]
	elif effect_type == "recovered":
		msg = "%s regain%s enough consciousness to act." % [
			actor,
			"" if entity_id == 0 else "s",
		]
	else:
		return
	EventBus.post("effect", msg, {"entity": entity_id, "effect": effect_type, "part": part})

func _on_smash_event(entity_id: int, tile_id: String, result: String) -> void:
	_update_vitals()
	var actor = "You" if entity_id == 0 else _entity_name(entity_id)
	var tile_name = TileDb.get_tile_name(tile_id)
	if result == "exhausted":
		var verb_be = "are" if entity_id == 0 else "is"
		EventBus.post("smash", "%s %s too exhausted to smash." % [actor, verb_be], {})
		return
	if result == "failed":
		var verb_fail = "fail" if entity_id == 0 else "fails"
		EventBus.post("smash", "%s %s to smash the %s." % [actor, verb_fail, tile_name], {})
		return
	var sounds = ["*SMASH!*", "*CRUNCH!*", "*CRACK!*"]
	var sfx = sounds[randi() % sounds.size()]
	var verb = "smash" if entity_id == 0 else "smashes"
	var msg = "[b]%s[/b] %s %s the %s." % [sfx, actor, verb, tile_name]
	EventBus.post("smash", msg, {"entity": entity_id, "tile": tile_id})

func _entity_name(entity_id: int) -> String:
	if entity_id == 0:
		return "You"
	if not _GameWorld:
		return "Entity"
	var anatomy = _GameWorld.get_entity_anatomy(entity_id)
	var race = anatomy.get("race_id", "creature") if not anatomy.is_empty() else "creature"
	return race.capitalize()

func _update_display() -> void:
	if DayLabel:
		DayLabel.label2_text = TimeManager.get_date_string()
	if TimeLabel:
		TimeLabel.label2_text = TimeManager.get_time_string()
	if SunLabel:
		var sun_state := TimeManager.get_sun_state()
		SunLabel.label2_text = sun_state
		_set_value_color(SunLabel, _sun_color(sun_state))
	_update_vitals()
	_refresh_inspection()

func _update_vitals() -> void:
	var status: Dictionary = _GameWorld.get_entity_condition_status(0)
	_update_consciousness(status)
	_update_stamina(status)
	_update_body(status)
	_update_pain(status)
	_update_blood(status)

func _update_stamina(status: Dictionary) -> void:
	var current := int(round(float(status.get("current_stamina", 0.0))))
	var maximum := int(round(float(status.get("max_stamina", 0.0))))
	if maximum <= 0:
		StaminaValue.text = "--"
		StaminaValue.add_theme_color_override("font_color", COLOR_MUTED)
		StaminaBar.hide()
		return
	StaminaValue.text = "%d / %d" % [current, maximum]
	StaminaValue.add_theme_color_override("font_color", COLOR_TEXT)
	_update_resource_bar(StaminaBar, current, maximum)
	StaminaBar.tooltip_text = "Stamina: %d of %d" % [current, maximum]

func _update_consciousness(status: Dictionary) -> void:
	var current := float(status.get("current_consciousness", 0.0))
	var maximum := maxf(1.0, float(status.get("max_consciousness", 100.0)))
	var percent := clampf(float(status.get("consciousness", 0.0)), 0.0, 1.0)
	var ceiling := clampf(float(status.get("consciousness_ceiling", 1.0)), 0.0, 1.0)
	var downed := bool(status.get("downed", false))
	if downed:
		ConsciousnessValue.text = "DOWNED %d%%" % roundi(percent * 100.0)
	else:
		ConsciousnessValue.text = "%d%%" % roundi(percent * 100.0)
	var color := COLOR_DANGER if percent <= 0.20 else (COLOR_ATTENTION if percent < 0.35 else COLOR_TEXT)
	ConsciousnessValue.add_theme_color_override("font_color", color)
	_update_resource_bar(ConsciousnessBar, roundi(current), roundi(maximum))
	_set_bar_fill(ConsciousnessBar, COLOR_CONSCIOUSNESS)
	ConsciousnessBar.tooltip_text = "Consciousness: %.1f of %.1f\nCurrent ceiling: %d%%\nAccuracy: %+.0f points\nAction speed: %d%%" % [
		current,
		maximum,
		roundi(ceiling * 100.0),
		float(status.get("consciousness_accuracy_modifier", 0.0)) * 100.0,
		roundi(float(status.get("consciousness_speed_multiplier", 1.0)) * 100.0),
	]

func _update_pain(status: Dictionary) -> void:
	var pain := clampf(float(status.get("pain", 0.0)), 0.0, 1.0)
	PainValue.text = "Pain %d%%" % roundi(pain * 100.0)
	var color := COLOR_BODY_LABEL
	if pain > 0.80:
		color = COLOR_DANGER
	elif pain > 0.50:
		color = COLOR_WARNING_ORANGE
	elif pain > 0.20:
		color = COLOR_ATTENTION
	PainValue.add_theme_color_override("font_color", color)
	PainBar.max_value = 100.0
	PainBar.value = pain * 100.0
	_set_bar_fill(PainBar, _pain_bar_color(pain))
	PainBar.show()
	var tooltip := "Pain: %d%%\nUntreated floor: %d%%\nAccuracy: %+.0f points\nAction speed: %d%%" % [
		roundi(pain * 100.0),
		roundi(float(status.get("pain_floor", 0.0)) * 100.0),
		float(status.get("pain_accuracy_modifier", 0.0)) * 100.0,
		roundi(float(status.get("pain_speed_multiplier", 1.0)) * 100.0),
	]
	PainValue.tooltip_text = tooltip
	PainBar.tooltip_text = tooltip
	var pain_cell := PainValue.get_parent() as Control
	pain_cell.tooltip_text = tooltip
	pain_cell.show()

func _update_body(status: Dictionary) -> void:
	for cell in _body_cells.values():
		cell.hide()
	for part_value in status.get("hud_anatomy", []):
		var part: Dictionary = part_value
		var cell: Control = _body_cells[str(part.get("id", ""))]
		var bar := cell.get_node("Bar") as ProgressBar
		var integrity := clampf(float(part.get("integrity", 0.0)), 0.0, 1.0)
		bar.max_value = 100.0
		bar.value = integrity * 100.0
		_set_bar_fill(bar, _body_bar_color(integrity))
		var label := cell.get_node("Label") as Label
		label.add_theme_color_override(
			"font_color",
			_body_label_color(integrity))
		var current := float(part.get("current_integrity", 0.0))
		var maximum := float(part.get("max_integrity", 0.0))
		cell.tooltip_text = "%s: %d%%\nIntegrity: %.1f of %.1f" % [
			str(part.get("label", "Body part")),
			roundi(integrity * 100.0),
			current,
			maximum,
		]
		cell.show()

func _update_blood(status: Dictionary) -> void:
	var blood := clampf(float(status.get("blood", 0.0)), 0.0, 1.0)
	BloodValue.text = "Blood %d%%" % roundi(blood * 100.0)
	var color := COLOR_BODY_LABEL
	if blood < 0.10:
		color = COLOR_DANGER
	elif blood < 0.25:
		color = COLOR_DANGER
	elif blood < 0.50:
		color = COLOR_ATTENTION
	BloodValue.add_theme_color_override("font_color", color)
	BloodValue.add_theme_font_size_override("font_size", 10)
	BloodBar.max_value = 100.0
	BloodBar.value = blood * 100.0
	_set_bar_fill(BloodBar, _blood_bar_color(blood))
	BloodBar.show()
	var tooltip := "Blood: %.1f of %.1f" % [
		float(status.get("current_blood", 0.0)),
		float(status.get("max_blood", 0.0)),
	]
	BloodValue.tooltip_text = tooltip
	BloodBar.tooltip_text = tooltip
	var blood_cell := BloodValue.get_parent() as Control
	blood_cell.tooltip_text = tooltip
	blood_cell.show()

func _body_bar_color(integrity: float) -> Color:
	if integrity > 0.70:
		return COLOR_BODY_HEALTHY
	if integrity >= 0.40:
		return COLOR_BODY_WOUNDED
	return COLOR_BODY_CRITICAL

func _body_label_color(integrity: float) -> Color:
	if integrity > 0.70:
		return COLOR_BODY_LABEL
	if integrity >= 0.40:
		return COLOR_ATTENTION
	return COLOR_DANGER

func _pain_bar_color(pain: float) -> Color:
	if pain <= 0.20:
		return COLOR_CONSCIOUSNESS
	if pain >= 0.80:
		return COLOR_CONDITION_RED
	var danger := (pain - 0.20) / 0.60
	return COLOR_CONSCIOUSNESS.lerp(COLOR_CONDITION_RED, danger)

func _blood_bar_color(blood: float) -> Color:
	if blood >= 0.50:
		return COLOR_CONSCIOUSNESS
	if blood <= 0.10:
		return COLOR_CONDITION_RED
	var danger := (0.50 - blood) / 0.40
	return COLOR_CONSCIOUSNESS.lerp(COLOR_CONDITION_RED, danger)

func _set_bar_fill(bar: ProgressBar, color: Color) -> void:
	var style := StyleBoxFlat.new()
	style.bg_color = color
	style.corner_radius_top_left = 2
	style.corner_radius_top_right = 2
	style.corner_radius_bottom_left = 2
	style.corner_radius_bottom_right = 2
	bar.add_theme_stylebox_override("fill", style)

func _update_resource_bar(bar: ProgressBar, current: int, maximum: int) -> void:
	bar.max_value = maximum
	bar.value = clampi(current, 0, maximum)
	bar.show()

func _set_value_color(row: HBoxContainer, color: Color) -> void:
	var value_label := row.get_node_or_null("Label2") as Label
	if value_label:
		value_label.add_theme_color_override("font_color", color)

func _sun_color(sun_state: String) -> Color:
	match sun_state:
		"Noon", "Sunrise", "Sunset":
			return COLOR_ATTENTION
		"Night", "Unknown":
			return COLOR_MUTED
		_:
			return COLOR_TEXT

func _initialize_inspection() -> void:
	if not _GameWorld:
		return
	_inspection_pos = _GameWorld.get_player_position()
	_refresh_inspection()

func _on_look_mode_changed(active: bool) -> void:
	_look_mode_active = active
	if active:
		current_tab = LOOK_TAB
	else:
		current_tab = VITALS_TAB
	if not active and _GameWorld:
		_inspection_pos = _GameWorld.get_player_position()
		_refresh_inspection()

func _on_inspection_focus_changed(cell_pos: Vector2i) -> void:
	_look_mode_active = InputManager.current_mode == InputManager.InputMode.LOOK
	_inspection_pos = cell_pos
	_refresh_inspection()

func _on_entity_moved(_entity_id: int, _new_pos: Vector2i, _new_chunk: Vector2i) -> void:
	if not _look_mode_active:
		_inspection_pos = _GameWorld.get_player_position()
	_refresh_inspection()

func _refresh_inspection() -> void:
	if not _GameWorld:
		return
	_look_mode_active = InputManager.current_mode == InputManager.InputMode.LOOK
	if not _look_mode_active:
		_inspection_pos = _GameWorld.get_player_position()
	var inspection: Dictionary = _GameWorld.inspect_cell(_inspection_pos)
	_render_inspection(inspection)

func _render_inspection(inspection: Dictionary) -> void:
	LookTitle.text = "LOOK — %s" % _relative_position_text(_inspection_pos)
	LookCoordinates.text = "%d, %d · Z %d" % [
		_inspection_pos.x,
		_inspection_pos.y,
		int(inspection.get("z", _GameWorld.get_player_z()))
	]

	VisibilityLabel.visible = false
	TileSection.visible = false
	CreatureSection.visible = false
	ItemsSection.visible = false
	TextSection.visible = false

	var visibility := str(inspection.get("visibility", "unseen"))
	if visibility == "unseen":
		VisibilityLabel.text = "Unseen area"
		VisibilityLabel.visible = true
		return
	if visibility == "remembered":
		VisibilityLabel.text = "Remembered terrain."
		VisibilityLabel.visible = true

	var terrain: Dictionary = inspection.get("terrain", {})
	if not terrain.is_empty():
		TileSection.visible = true
		TerrainValue.text = str(terrain.get("name", "Unknown"))
		var properties: Array = terrain.get("properties", [])
		var movement := str(properties[0]) if not properties.is_empty() else "Unknown"
		MovementValue.text = movement
		MovementValue.add_theme_color_override("font_color", COLOR_MUTED if movement == "Blocked" else COLOR_TEXT)

		var has_light := visibility == "visible"
		LightLabel.visible = has_light
		LightValue.visible = has_light
		DarknessNoteMargin.visible = false
		if has_light:
			var light := str(inspection.get("light", "Unknown"))
			LightValue.text = light
			LightValue.add_theme_color_override("font_color", _light_color(light))
			if light == "Dim" or light == "Dark":
				DarknessNote.text = "Too dark to see creatures or items."
				DarknessNoteMargin.visible = true

	var entity: Dictionary = inspection.get("entity", {})
	if not entity.is_empty():
		CreatureSection.visible = true
		var relation := str(entity.get("relation", ""))
		var relation_color := _relation_color(relation)
		CreatureName.text = str(entity.get("name", "Creature"))
		CreatureName.add_theme_color_override("font_color", relation_color)
		var show_attitude := not relation.is_empty() and relation != "you"
		AttitudeLabel.visible = show_attitude
		AttitudeValue.visible = show_attitude
		AttitudeValue.text = relation.capitalize()
		AttitudeValue.add_theme_color_override("font_color", relation_color)
		var relative_speed := str(entity.get("relative_speed", ""))
		SpeedLabel.visible = not relative_speed.is_empty()
		SpeedValue.visible = not relative_speed.is_empty()
		SpeedValue.text = relative_speed

	var items: Array = inspection.get("items", [])
	_render_items(items)

	var metadata: Dictionary = inspection.get("metadata", {})
	var metadata_text := _inspection_metadata_text(metadata)
	if not metadata_text.is_empty():
		TextSection.visible = true
		WorldText.text = '“%s”' % metadata_text

func _on_player_movement_mode_changed(_mode_id: String, _reason: String) -> void:
	_refresh_inspection()

func _render_items(items: Array) -> void:
	for child in ItemsList.get_children():
		ItemsList.remove_child(child)
		child.queue_free()
	ItemsSection.visible = not items.is_empty()
	if items.is_empty():
		return
	ItemsTitle.text = "ITEMS (%d)" % items.size()
	var shown := mini(items.size(), 4)
	for i in range(shown):
		var item: Dictionary = items[i]
		var row := HBoxContainer.new()
		var name_label := Label.new()
		name_label.text = str(item.get("name", item.get("id", "Item")))
		name_label.add_theme_color_override("font_color", COLOR_TEXT)
		name_label.add_theme_font_size_override("font_size", 12)
		name_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		row.add_child(name_label)
		var amount := int(item.get("amount", 1))
		if amount > 1:
			var amount_label := Label.new()
			amount_label.text = "×%d" % amount
			amount_label.add_theme_color_override("font_color", COLOR_MUTED)
			amount_label.add_theme_font_size_override("font_size", 12)
			row.add_child(amount_label)
		ItemsList.add_child(row)
	if items.size() > shown:
		var more_label := Label.new()
		more_label.text = "…and %d more" % (items.size() - shown)
		more_label.add_theme_color_override("font_color", COLOR_MUTED)
		more_label.add_theme_font_size_override("font_size", 12)
		ItemsList.add_child(more_label)

func _light_color(light: String) -> Color:
	match light:
		"Bright":
			return COLOR_ATTENTION
		"Lit":
			return COLOR_TEXT
		_:
			return COLOR_MUTED

func _relation_color(relation: String) -> Color:
	match relation.to_lower():
		"hostile", "enemy":
			return COLOR_DANGER
		"friendly", "ally":
			return COLOR_POSITIVE
		"neutral":
			return COLOR_ATTENTION
		_:
			return COLOR_TEXT

func _relative_position_text(cell_pos: Vector2i) -> String:
	var offset := cell_pos - _GameWorld.get_player_position()
	var distance := maxi(abs(offset.x), abs(offset.y))
	if distance == 0:
		return "Here"
	return "%d tile%s away" % [distance, "" if distance == 1 else "s"]

func _inspection_metadata_text(metadata: Dictionary) -> String:
	var data = metadata.get("data", {})
	if data is Dictionary:
		var nested_text := str(data.get("text", ""))
		if not nested_text.is_empty():
			return nested_text
	return str(metadata.get("text", ""))
