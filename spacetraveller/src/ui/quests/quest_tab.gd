extends ListDetailTab

@onready var _progress_bar: ProgressBar = $HBoxContainer/PanelContainer2/MarginContainer/VBoxContainer/ProgressBar
@onready var _progress_text: Label = $HBoxContainer/PanelContainer2/MarginContainer/VBoxContainer/ProgressText

var _quests: Array = []
var _quest_id_to_index: Dictionary = {}

func _ready() -> void:
	super._ready()
	if QuestService.quest_started.is_connected(_on_quest_started) == false:
		QuestService.quest_started.connect(_on_quest_started)
	if QuestService.quest_completed.is_connected(_on_quest_completed) == false:
		QuestService.quest_completed.connect(_on_quest_completed)
	if QuestService.quest_declined.is_connected(_on_quest_declined) == false:
		QuestService.quest_declined.connect(_on_quest_declined)
	if QuestService.quest_failed.is_connected(_on_quest_failed) == false:
		QuestService.quest_failed.connect(_on_quest_failed)
	if QuestService.objective_progressed.is_connected(_on_objective_progressed) == false:
		QuestService.objective_progressed.connect(_on_objective_progressed)

func _get_display_data() -> Array:
	_quests = QuestService.get_active()
	_quest_id_to_index.clear()
	var formatted: Array = []
	for q in _quests:
		var qid: String = str(q.get("quest_id", ""))
		if qid.is_empty():
			continue
		_quest_id_to_index[qid] = formatted.size()
		formatted.append({
			"quest_id": qid,
			"kind": str(q.get("kind", "")),
			"display_name": str(q.get("label", "Unnamed quest")),
			"description": str(q.get("description", "")),
			"target": int(q.get("target", 0)),
			"progress": int(q.get("progress", 0)),
			"time_remaining_turns": int(q.get("time_remaining_turns", 0)),
			"rewards": q.get("rewards", {}),
			"next_giver": str(q.get("next_giver", "")),
		})
	return formatted

func _update_details_ui(item_data: Dictionary) -> void:
	_clear_detail_rows()

	var target: int = int(item_data.get("target", 0))
	var progress: int = int(item_data.get("progress", 0))
	_progress_bar.max_value = max(1, target)
	_progress_bar.value = clamp(progress, 0, _progress_bar.max_value)
	_progress_text.text = "%d / %d" % [progress, target]

	var time_remaining: int = int(item_data.get("time_remaining_turns", 0))
	if time_remaining > 0:
		_add_detail("Time", "%d:%02d remaining" % [time_remaining / 60, time_remaining % 60])

	var rewards: Dictionary = item_data.get("rewards", {})
	if rewards.is_empty():
		_add_detail("None", "")
		return

	var items: Array = rewards.get("items", [])
	for entry in items:
		var id: String = str(entry.get("id", ""))
		var amount: int = int(entry.get("amount", 0))
		if id.is_empty() or amount <= 0:
			continue
		var _name: String = ItemDb.get_item_name(id) if id != "" else id
		_add_detail("Item", "%d × %s" % [amount, _name])

	var f_delta: int = int(rewards.get("friendship_delta", 0))
	if f_delta != 0:
		_add_detail("Friendship", ("+" if f_delta > 0 else "") + str(f_delta))

	var r_delta: int = int(rewards.get("romance_delta", 0))
	if r_delta != 0:
		_add_detail("Romance", ("+" if r_delta > 0 else "") + str(r_delta))

	var next_giver := str(item_data.get("next_giver", ""))
	if not next_giver.is_empty():
		_add_detail("Next", "Speak to %s" % next_giver)

	if detailsContainer.get_child_count() == 0:
		_add_detail("None", "")

func _on_refresh() -> void:
	if _quests.is_empty():
		_clear_details()

func _clear_details() -> void:
	super._clear_details()
	_progress_bar.max_value = 1.0
	_progress_bar.value = 0.0
	_progress_text.text = ""

func _on_quest_started(_quest_id: String) -> void:
	refresh_view()

func _on_quest_completed(_quest_id: String) -> void:
	refresh_view()

func _on_quest_declined(_quest_id: String) -> void:
	refresh_view()

func _on_quest_failed(_quest_id: String) -> void:
	refresh_view()

func _on_objective_progressed(quest_id: String, _progress: int, _target: int) -> void:
	if not _quest_id_to_index.has(quest_id):
		return
	var idx: int = int(_quest_id_to_index[quest_id])
	if idx != selected_index:
		return
	var q: Dictionary = QuestService.get_quest(quest_id)
	if q.is_empty():
		return
	var data: Dictionary = {
		"target": int(q.get("target", 0)),
		"progress": int(q.get("progress", 0)),
		"time_remaining_turns": int(q.get("time_remaining_turns", 0)),
		"rewards": q.get("rewards", {}),
		"next_giver": str(q.get("next_giver", "")),
	}
	_update_details_ui(data)
