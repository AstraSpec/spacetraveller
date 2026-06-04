extends Node

signal quest_offered(quest_id: String)
signal quest_started(quest_id: String)
signal objective_progressed(quest_id: String, progress: int, target: int)
signal quest_completed(quest_id: String)
signal quest_declined(quest_id: String)

const STATUS_OFFERED := "offered"
const STATUS_ACTIVE := "active"
const STATUS_COMPLETED := "completed"
const STATUS_DECLINED := "declined"

var _game_world: Node = null

func bind_game_world(world: Node) -> void:
	if _game_world == world:
		return
	if _game_world:
		_disconnect_game_world()
	_game_world = world
	_connect_game_world()

func _connect_game_world() -> void:
	if not _game_world:
		return
	if not _game_world.has_signal("quest_updated"):
		return
	if _game_world.quest_updated.is_connected(_on_quest_updated):
		return
	_game_world.quest_updated.connect(_on_quest_updated)

func _disconnect_game_world() -> void:
	if not _game_world:
		return
	if _game_world.has_signal("quest_updated") and _game_world.quest_updated.is_connected(_on_quest_updated):
		_game_world.quest_updated.disconnect(_on_quest_updated)

func _on_quest_updated(quest_id: String) -> void:
	if not _game_world:
		return
	var q: Dictionary = _game_world.get_quest(quest_id)
	if q.is_empty():
		return
	var status: String = str(q.get("status", ""))
	var prev_progress: int = int(q.get("__last_emitted_progress", -1))
	var progress: int = int(q.get("progress", 0))
	var target: int = int(q.get("target", 0))

	match status:
		STATUS_OFFERED:
			quest_offered.emit(quest_id)
		STATUS_ACTIVE:
			if prev_progress < 0:
				quest_started.emit(quest_id)
			objective_progressed.emit(quest_id, progress, target)
		STATUS_COMPLETED:
			objective_progressed.emit(quest_id, target, target)
			quest_completed.emit(quest_id)
		STATUS_DECLINED:
			quest_declined.emit(quest_id)

	q["__last_emitted_progress"] = progress

func get_offers_for(giver_entity_id: int) -> Array:
	if not _game_world:
		return []
	return _game_world.get_quest_offers_for_giver(giver_entity_id)

func get_active() -> Array:
	if not _game_world:
		return []
	return _game_world.get_active_quests()

func get_completed() -> Array:
	if not _game_world:
		return []
	return _game_world.get_completed_quests()

func get_all_offers() -> Array:
	if not _game_world:
		return []
	return _game_world.get_offered_quests()

func offer(giver_entity_id: int) -> String:
	if not _game_world:
		return ""
	var offers: Array = _game_world.generate_quest_offers(giver_entity_id, 1)
	if offers.is_empty():
		return ""
	return str(offers[0].get("quest_id", ""))

func accept(quest_id: String) -> bool:
	return _game_world != null and _game_world.accept_quest(quest_id)

func decline(quest_id: String) -> bool:
	return _game_world != null and _game_world.decline_quest(quest_id)

func can_complete(quest_id: String) -> bool:
	return _game_world != null and _game_world.can_complete_quest(quest_id)

func complete(quest_id: String) -> bool:
	return _game_world != null and _game_world.complete_quest(quest_id)

func is_active(quest_id: String) -> bool:
	return _game_world != null and _game_world.is_quest_active(quest_id)

func is_completed(quest_id: String) -> bool:
	return _game_world != null and _game_world.is_quest_completed(quest_id)

func get_quest(quest_id: String) -> Dictionary:
	if not _game_world:
		return {}
	return _game_world.get_quest(quest_id)
