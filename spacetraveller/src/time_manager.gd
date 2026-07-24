extends Node

signal turn_passed

enum Season { SPRING, SUMMER, AUTUMN, WINTER }

# Time Constants
const TURNS_PER_SECOND = 1
const SECONDS_PER_MINUTE = 60
const MINUTES_PER_HOUR = 60
const HOURS_PER_DAY = 24
const DAYS_PER_SEASON = 91

const TURNS_PER_MINUTE = TURNS_PER_SECOND * SECONDS_PER_MINUTE
const TURNS_PER_HOUR = TURNS_PER_MINUTE * MINUTES_PER_HOUR
const TURNS_PER_DAY = TURNS_PER_HOUR * HOURS_PER_DAY
const TURNS_PER_YEAR = TURNS_PER_DAY * DAYS_PER_SEASON * 4

# Seasonal Constants
const BASE_DAY_FRACTION = 0.5 # 12 hours
const DAY_VARIANCE = 0.166 # ±4 hours (4/24)
const BASE_INTENSITY = 0.8
const INTENSITY_VARIANCE = 0.2

var total_turns: int = 28800 # Start at 08:00
var solar_input: float = 0.0

func advance_turn(count: int = 1) -> void:
	total_turns += count
	_update_environment()
	turn_passed.emit()

func reset() -> void:
	total_turns = 28800
	_update_environment()

func is_daytime() -> bool:
	return bool(_get_environment_context().get("is_day", false))

func _update_environment() -> void:
	var ctx = _get_environment_context()
	
	# 1. Update Solar Input
	if ctx.is_day:
		var sun_angle = (ctx.day_fraction - ctx.sunrise) / ctx.day_length * PI
		solar_input = sin(sun_angle) * ctx.max_intensity
	else:
		solar_input = 0.0

# --- CORE ---

func _get_environment_context() -> Dictionary:
	var yearly_fraction = float(total_turns) / float(TURNS_PER_YEAR)
	var day_seconds = total_turns % TURNS_PER_DAY
	var day_fraction = float(day_seconds) / float(TURNS_PER_DAY)
	
	# seasonal_factor: 0 (Spring), 1 (Summer), 0 (Autumn), -1 (Winter)
	var seasonal_factor = sin(2.0 * PI * yearly_fraction)
	
	var day_length = BASE_DAY_FRACTION + (DAY_VARIANCE * seasonal_factor)
	var half_day = day_length / 2.0
	var sunrise = 0.5 - half_day
	var sunset = 0.5 + half_day
	
	return {
		"day_fraction": day_fraction,
		"seasonal_factor": seasonal_factor,
		"day_length": day_length,
		"sunrise": sunrise,
		"sunset": sunset,
		"is_day": day_fraction >= sunrise and day_fraction <= sunset,
		"max_intensity": BASE_INTENSITY + (INTENSITY_VARIANCE * seasonal_factor)
	}

func get_sun_state() -> String:
	var ctx = _get_environment_context()
	var f = ctx.day_fraction
	var hour = 1.0 / 24.0
	
	var states = [
		{"name": "Noon",    "active": abs(f - 0.5) < hour * 0.5},
		{"name": "Sunrise", "active": abs(f - ctx.sunrise) < hour},
		{"name": "Sunset",  "active": abs(f - ctx.sunset) < hour},
		{"name": "Day",     "active": ctx.is_day},
		{"name": "Night",   "active": true}
	]
	
	for state in states:
		if state.active:
			return state.name
			
	return "Unknown"

# --- UTILITIES ---

func get_time_string() -> String:
	var total_seconds = total_turns / TURNS_PER_SECOND
	var hour = (total_seconds / (SECONDS_PER_MINUTE * MINUTES_PER_HOUR)) % HOURS_PER_DAY
	var minute = (total_seconds / SECONDS_PER_MINUTE) % MINUTES_PER_HOUR
	var second = total_seconds % SECONDS_PER_MINUTE
	return "%02d:%02d:%02d" % [hour, minute, second]

func get_date_string() -> String:
	var total_days = total_turns / TURNS_PER_DAY
	var season_idx = (total_days / DAYS_PER_SEASON) % 4
	var day_in_season = (total_days % DAYS_PER_SEASON) + 1
	var season_name = Season.keys()[season_idx].capitalize()
	return "%d, %s" % [day_in_season, season_name]

func get_day_of_year() -> int:
	return (total_turns / TURNS_PER_DAY) + 1
