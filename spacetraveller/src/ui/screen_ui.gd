extends TabContainer

@export var _GameWorld :GameWorld
@export var DayLabel :HBoxContainer
@export var TimeLabel :HBoxContainer
@export var SunLabel :HBoxContainer
@export var HealthLabel :HBoxContainer

func _ready() -> void:
	TimeManager.turn_passed.connect(_update_display)
	if _GameWorld:
		_GameWorld.player_action_resolved.connect(_on_player_action_resolved)
		_GameWorld.combat_event.connect(_on_combat_event)
	_update_display()
	call_deferred("_update_health")

func _on_player_action_resolved(_entity_id: int, _cost: float, _next_turn_time: float) -> void:
	_update_health()

func _on_combat_event(attacker_id: int, defender_id: int, damage: float, result: String) -> void:
	_update_health()
	var attacker_name = _entity_name(attacker_id)
	var defender_name = _entity_name(defender_id)
	var dmg_str = str(int(round(damage)))
	var msg = ""
	var category = "combat_player" if attacker_id == 0 else "combat_enemy"
	if result == "kill":
		if defender_id == 0:
			msg = "[b]%s killed %s. (%s dmg)[/b]" % [attacker_name, defender_name, dmg_str]
		else:
			msg = "%s killed %s. (%s dmg)" % [attacker_name, defender_name, dmg_str]
	else:
		msg = "%s hit %s for %s damage." % [attacker_name, defender_name, dmg_str]
	EventBus.post(category, msg, {"attacker": attacker_id, "defender": defender_id, "damage": damage})

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
	_update_health()

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
