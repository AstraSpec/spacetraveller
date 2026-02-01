extends TabContainer

@export var DayLabel :HBoxContainer
@export var TimeLabel :HBoxContainer
@export var SunLabel :HBoxContainer

func _ready() -> void:
	TimeManager.turn_passed.connect(_update_display)
	_update_display()

func _update_display() -> void:
	if DayLabel:
		DayLabel.label2_text = TimeManager.get_date_string()
	if TimeLabel:
		TimeLabel.label2_text = TimeManager.get_time_string()
	if SunLabel:
		SunLabel.label2_text = TimeManager.get_sun_state()
