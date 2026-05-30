extends TabContainer

@export var _GameWorld :GameWorld
@export var DayLabel :HBoxContainer
@export var TimeLabel :HBoxContainer
@export var SunLabel :HBoxContainer
@export var HealthLabel :HBoxContainer
@export var StaminaLabel :HBoxContainer

func _ready() -> void:
	TimeManager.turn_passed.connect(_update_display)
	if _GameWorld:
		_GameWorld.player_action_resolved.connect(_on_player_action_resolved)
		_GameWorld.combat_event.connect(_on_combat_event)
		_GameWorld.smash_event.connect(_on_smash_event)
	_update_display()
	call_deferred("_update_vitals")

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
	var is_crit = result == "crit" or result == "crit_kill"
	var is_kill = result == "kill" or result == "crit_kill"
	var msg = ""

	if result == "exhausted":
		msg = "%s %s too exhausted to attack." % [attacker_name, "are" if attacker_id == 0 else "is"]
	elif result == "miss":
		msg = "%s %s at %s and misses." % [attacker_name, verb_conj, defender_name]
	elif is_kill:
		if defender_id == 0:
			msg = "[b]%s %s your %s for %s and kills you![/b]" % [attacker_name, verb_conj, part, dmg_str]
		else:
			msg = "%s %s %s for %s. %s dies." % [attacker_name, verb_conj, target, dmg_str, defender_name]
	else:
		msg = "%s %s %s for %s damage." % [attacker_name, verb_conj, target, dmg_str]

	if is_crit:
		msg = "[b]CRITICAL HIT! %s[/b]" % msg

	EventBus.post(category, msg, {"attacker": attacker_id, "defender": defender_id, "damage": damage, "part": part, "crit": is_crit})

func _possessive(entity_id: int, part: String) -> String:
	if part.is_empty():
		return _entity_name(entity_id)
	if entity_id == 0:
		return "your %s" % part
	return "the %s's %s" % [_entity_name(entity_id), part]

func _on_smash_event(entity_id: int, tile_id: String, result: String) -> void:
	_update_vitals()
	var actor = "You" if entity_id == 0 else _entity_name(entity_id)
	var tile_name = tile_id.replace("_", " ")
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
		SunLabel.label2_text = TimeManager.get_sun_state()
	_update_vitals()

func _update_vitals() -> void:
	_update_health()
	_update_stamina()

func _update_health() -> void:
	if not HealthLabel or not _GameWorld:
		return
	var hp = _GameWorld.get_player_health()
	if hp.is_empty():
		HealthLabel.label2_text = "--"
		return
	var current = int(round(hp.get("current_hp", 0)))
	var maximum = int(round(hp.get("max_hp", 0)))
	HealthLabel.label2_text = "%d / %d" % [current, maximum]

func _update_stamina() -> void:
	if not StaminaLabel or not _GameWorld:
		return
	var st = _GameWorld.get_player_stamina()
	if st.is_empty():
		StaminaLabel.label2_text = "--"
		return
	var current = int(round(st.get("current_stamina", 0)))
	var maximum = int(round(st.get("max_stamina", 0)))
	StaminaLabel.label2_text = "%d / %d" % [current, maximum]
