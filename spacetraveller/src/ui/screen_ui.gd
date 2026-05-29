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

func _on_combat_event(_attacker_id: int, _defender_id: int, _damage: float, _result: String) -> void:
	_update_health()

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
