extends Node

const DIALOGUE_DIR := "res://data/dialogues"
const FALLBACK_DIALOGUE_ID := "default_stranger"
const TERMINAL_NODE_IDS := {
	"accepted": true,
	"declined": true,
	"complete": true,
	"not_done": true,
}

var _dialogues: Array = []
var _dialogues_by_id: Dictionary = {}

func _ready() -> void:
	load_dialogues()

func load_dialogues() -> void:
	_dialogues.clear()
	_dialogues_by_id.clear()
	var dir := DirAccess.open(DIALOGUE_DIR)
	if not dir:
		push_error("[DialogueService] Missing dialogue directory: %s" % DIALOGUE_DIR)
		return
	dir.list_dir_begin()
	var file_name := dir.get_next()
	while not file_name.is_empty():
		if not dir.current_is_dir() and file_name.get_extension().to_lower() == "json":
			_load_file("%s/%s" % [DIALOGUE_DIR, file_name])
		file_name = dir.get_next()
	dir.list_dir_end()
	_dialogues.sort_custom(func(a, b): return int(a.get("priority", 0)) > int(b.get("priority", 0)))

func has_dialogue_for(world, target_id: int) -> bool:
	return not _pick_dialogue(world, target_id).is_empty()

func start_state(world, target_id: int, saved: Dictionary = {}) -> Dictionary:
	if _is_saved_state_valid(saved):
		return saved.duplicate(true)
	var dialogue := _pick_dialogue(world, target_id)
	if dialogue.is_empty():
		return {}
	return {
		"dialogue_id": str(dialogue.get("id", "")),
		"node_id": str(dialogue.get("start", "start")),
		"local_flags": {},
		"pending_quest_id": _find_pending_quest_id(target_id),
	}

func get_npc_text(state: Dictionary, world, target_id: int) -> String:
	var node := _get_node(state)
	if node.is_empty():
		return "They have nothing to say."
	return _format_text(str(node.get("npc", "")), state, world, target_id)

func get_options(state: Dictionary, world, target_id: int) -> Array:
	var node := _get_node(state)
	if node.is_empty():
		return [_end_option()]
	var out: Array = []
	for option in node.get("options", []):
		if option is Dictionary and _constraints_pass(option.get("constraints", {}), state, world, target_id):
			out.append(option.duplicate(true))
	_append_quest_decision_options(out, node, state, world, target_id)
	out.append(_end_option())
	return out

func apply_option(option: Dictionary, state: Dictionary, world, target_id: int) -> Dictionary:
	var next_state := state.duplicate(true)
	var result := {
		"state": next_state,
		"end_conversation": false,
		"start_fight": false,
		"message": "",
		"open_tab": "",
	}
	var effects: Dictionary = option.get("effects", {})
	_apply_effects(effects, next_state, result, world, target_id)
	result["open_tab"] = str(effects.get("open_tab", ""))

	var next_id := str(option.get("next", ""))
	if not next_id.is_empty():
		next_state["node_id"] = next_id
	if bool(effects.get("end_conversation", false)):
		result["end_conversation"] = true
	if bool(effects.get("start_fight", false)):
		result["start_fight"] = true
		result["end_conversation"] = true
	return result

func should_save_state(state: Dictionary) -> bool:
	if state.is_empty():
		return false
	var node := _get_node(state)
	if node.is_empty():
		return false
	if bool(node.get("resume", false)):
		return true
	var node_id := str(state.get("node_id", ""))
	if TERMINAL_NODE_IDS.has(node_id):
		return false
	var options: Array = node.get("options", [])
	if options.is_empty():
		return false
	for option in options:
		if not option is Dictionary:
			continue
		var effects: Dictionary = option.get("effects", {})
		if not bool(effects.get("end_conversation", false)) and not bool(effects.get("start_fight", false)):
			return true
	return false

func _load_file(path: String) -> void:
	var text := FileAccess.get_file_as_string(path)
	if text.is_empty():
		push_warning("[DialogueService] Empty dialogue file: %s" % path)
		return
	var parsed = JSON.parse_string(text)
	if not parsed is Dictionary:
		push_error("[DialogueService] Invalid JSON: %s" % path)
		return
	if not parsed.has("id") or not parsed.has("nodes"):
		push_error("[DialogueService] Dialogue missing id/nodes: %s" % path)
		return
	_dialogues.append(parsed)
	_dialogues_by_id[str(parsed["id"])] = parsed

func _is_saved_state_valid(saved: Dictionary) -> bool:
	var did := str(saved.get("dialogue_id", ""))
	var nid := str(saved.get("node_id", ""))
	if did.is_empty() or nid.is_empty() or not _dialogues_by_id.has(did):
		return false
	var nodes: Dictionary = _dialogues_by_id[did].get("nodes", {})
	return nodes.has(nid)

func _pick_dialogue(world: Node, target_id: int) -> Dictionary:
	var ctx := _build_context(world, target_id)
	var dialogue_id := str(ctx.get("dialogue_id", "")).strip_edges()
	if not dialogue_id.is_empty() and _dialogues_by_id.has(dialogue_id):
		return _dialogues_by_id[dialogue_id]

	var job := str(ctx.get("job", "")).strip_edges()
	if not job.is_empty():
		for job_dialogue_id in JobDb.get_dialogues(job):
			dialogue_id = str(job_dialogue_id).strip_edges()
			if not dialogue_id.is_empty() and _dialogues_by_id.has(dialogue_id):
				return _dialogues_by_id[dialogue_id]
	return _dialogues_by_id.get(FALLBACK_DIALOGUE_ID, {})

func _constraints_pass(constraints: Dictionary, state: Dictionary, world, target_id: int) -> bool:
	if constraints.is_empty():
		return true
	var ctx := _build_context(world, target_id)
	ctx["pending_quest_id"] = str(state.get("pending_quest_id", ""))
	return _dict_constraints_pass(constraints, ctx)

func _dict_constraints_pass(c: Dictionary, ctx: Dictionary) -> bool:
	if c.has("race") and str(c["race"]) != str(ctx.get("race", "")):
		return false
	if c.has("job") and str(c["job"]) != str(ctx.get("job", "")):
		return false
	if c.has("trait") and not Array(ctx.get("traits", [])).has(str(c["trait"])):
		return false
	if c.has("min_friendship") and int(ctx.get("friendship", 0)) < int(c["min_friendship"]):
		return false
	if c.has("max_friendship") and int(ctx.get("friendship", 0)) > int(c["max_friendship"]):
		return false
	if c.has("min_romance") and int(ctx.get("romance", 0)) < int(c["min_romance"]):
		return false
	if c.has("min_player_stamina") and float(ctx.get("player_stamina", 0.0)) < float(c["min_player_stamina"]):
		return false
	if c.has("quest_status") and str(c["quest_status"]) != str(ctx.get("quest_status", "none")):
		return false
	if c.has("can_complete_quest") and bool(c["can_complete_quest"]) != bool(ctx.get("can_complete_quest", false)):
		return false
	if c.has("has_pending_quest") and bool(c["has_pending_quest"]) != (not str(ctx.get("pending_quest_id", ctx.get("quest_id", ""))).is_empty()):
		return false
	return true

func _build_context(world, target_id: int) -> Dictionary:
	var anatomy: Dictionary = world.get_entity_anatomy(target_id) if world else {}
	var profile: Dictionary = world.get_entity_social_profile(target_id) if world and world.has_method("get_entity_social_profile") else {}
	var stamina: Dictionary = world.get_player_stamina() if world else {}
	var qid := _find_pending_quest_id(target_id)
	var qstatus := "none"
	var can_complete := false
	if not qid.is_empty():
		var q: Dictionary = QuestService.get_quest(qid)
		qstatus = str(q.get("status", "none"))
		can_complete = bool(q.get("can_complete", false)) or QuestService.can_complete(qid)
	return {
		"race": str(anatomy.get("race_id", "")),
		"job": str(profile.get("job", "drifter")),
		"dialogue_id": str(profile.get("dialogue_id", "")),
		"traits": profile.get("traits", []),
		"context_tags": profile.get("context_tags", []),
		"friendship": int(world.get_entity_friendship(target_id)) if world else 0,
		"romance": int(world.get_entity_romance(target_id)) if world else 0,
		"player_stamina": float(stamina.get("current_stamina", 0.0)),
		"quest_id": qid,
		"quest_status": qstatus,
		"can_complete_quest": can_complete,
	}

func _find_pending_quest_id(target_id: int) -> String:
	for q in QuestService.get_active():
		if int(q.get("giver_entity_id", -1)) == target_id:
			return str(q.get("quest_id", ""))
	var offers: Array = QuestService.get_offers_for(target_id)
	if not offers.is_empty():
		return str(offers[0].get("quest_id", ""))
	return ""

func _get_node(state: Dictionary) -> Dictionary:
	var did := str(state.get("dialogue_id", ""))
	var nid := str(state.get("node_id", ""))
	var dialogue: Dictionary = _dialogues_by_id.get(did, {})
	var nodes: Dictionary = dialogue.get("nodes", {})
	return nodes.get(nid, {})

func _append_quest_decision_options(out: Array, node: Dictionary, state: Dictionary, world, target_id: int) -> void:
	var npc_text := str(node.get("npc", ""))
	if not npc_text.contains("{quest_description}") and not npc_text.contains("{quest_label}"):
		return
	var ctx := _build_context(world, target_id)
	if str(ctx.get("quest_status", "none")) != QuestService.STATUS_OFFERED:
		return
	var pending := str(state.get("pending_quest_id", ""))
	if pending.is_empty():
		pending = str(ctx.get("quest_id", ""))
	if pending.is_empty():
		return
	if _has_effect_option(out, "quest_accept") or _has_effect_option(out, "quest_decline"):
		return
	out.append({
		"text": "Accept quest",
		"next": "accepted",
		"effects": { "quest_accept": true, "friendship_delta": 5 },
		"ui_color": "quest_accept"
	})
	out.append({
		"text": "Decline quest",
		"next": "declined",
		"effects": { "quest_decline": true, "friendship_delta": -3 },
		"ui_color": "quest_decline"
	})

func _has_effect_option(options: Array, effect_name: String) -> bool:
	for option in options:
		if option is Dictionary:
			var effects: Dictionary = option.get("effects", {})
			if bool(effects.get(effect_name, false)):
				return true
	return false

func _end_option() -> Dictionary:
	return {
		"text": "End conversation",
		"effects": { "end_conversation": true },
		"ui_color": "end"
	}

func _apply_effects(effects: Dictionary, state: Dictionary, result: Dictionary, world, target_id: int) -> void:
	if world:
		var df := int(effects.get("friendship_delta", 0))
		var dr := int(effects.get("romance_delta", 0))
		if df != 0:
			world.set_entity_friendship(target_id, int(world.get_entity_friendship(target_id)) + df)
		if dr != 0:
			world.set_entity_romance(target_id, int(world.get_entity_romance(target_id)) + dr)

	if effects.has("set_flag"):
		var flags: Dictionary = state.get("local_flags", {})
		flags[str(effects["set_flag"])] = true
		state["local_flags"] = flags

	var quest_offer = effects.get("quest_offer", false)
	var has_quest_offer: bool = (typeof(quest_offer) == TYPE_STRING and not str(quest_offer).is_empty()) or (typeof(quest_offer) == TYPE_BOOL and bool(quest_offer))
	if has_quest_offer:
		var qid := str(state.get("pending_quest_id", ""))
		if qid.is_empty():
			qid = _find_pending_quest_id(target_id)
		if qid.is_empty():
			var quest_kind: String = str(quest_offer) if typeof(quest_offer) == TYPE_STRING else ""
			qid = QuestService.offer(target_id, quest_kind)
		state["pending_quest_id"] = qid

	var pending := str(state.get("pending_quest_id", ""))
	if bool(effects.get("quest_accept", false)) and not pending.is_empty():
		QuestService.accept(pending)
	if bool(effects.get("quest_decline", false)) and not pending.is_empty():
		QuestService.decline(pending)
		state["pending_quest_id"] = ""
	if bool(effects.get("quest_complete", false)) and not pending.is_empty():
		if QuestService.complete(pending):
			result["message"] = "Quest completed."
			state["pending_quest_id"] = ""
		else:
			result["message"] = "You do not have everything needed yet."

func _format_text(template: String, state: Dictionary, world, target_id: int) -> String:
	var out := template
	var _name: String = str(world.get_entity_name(target_id)) if world else ""
	if _name.is_empty():
		_name = "stranger"
	out = out.replace("{npc_name}", _name)
	out = out.replace("{job}", str(_build_context(world, target_id).get("job", "drifter")))
	var qid := str(state.get("pending_quest_id", ""))
	if qid.is_empty():
		qid = _find_pending_quest_id(target_id)
	var q: Dictionary = QuestService.get_quest(qid) if not qid.is_empty() else {}
	out = out.replace("{quest_label}", str(q.get("label", "that work")))
	out = out.replace("{quest_description}", _green_text(str(q.get("description", ""))))
	return out

func _green_text(text: String) -> String:
	if text.is_empty():
		return ""
	return "[color=#7CFF7A]%s[/color]" % text.replace("[", "\\[").replace("]", "\\]")
