extends BaseListTab

@export var _GameWorld: GameWorld
@export var npcMessage: RichTextLabel
@export var friendshipBar: ProgressBar
@export var romanceBar: ProgressBar

const FRIENDSHIP_STEP := 5
const ROMANCE_STEP := 2
const TERMINAL_TEXT := "End conversation."
# Ordinal tone contract: index 0 = positive, 1 = neutral, 2 = negative.
const TONE_ORDER := ["positive", "neutral", "negative"]

# Tags identifying which of the two model calls a response belongs to.
const TAG_NPC := "npc"
const TAG_OPTIONS := "options"

const HOOK_TURN := 3
const QUEST_ACCEPT_TEXT := "I'll do it. Tell me more."
const QUEST_DECLINE_TEXT := "Sorry, I can't help with that."
const QUEST_COMPLETE_TEXT := "I've brought what you asked for."
const QUEST_TONE_ACCEPT := "quest_accept"
const QUEST_TONE_DECLINE := "quest_decline"
const QUEST_TONE_COMPLETE := "quest_complete"
# Soft close: after this many NPC turns without a quest, the conversation is
# force-ended (the NPC delivers a closing line and the window closes).
const FORCE_END_TURN := 5

# Conversation phases — replaces scattered booleans with a state machine.
enum Phase {
	GREETING,     # turns 1-2, no quest mention allowed
	SMALL_TALK,   # turns 3+ without quest, normal 3-tone options
	QUEST_OFFER,  # turn 3 with quest pending, accept/decline/end row
	ACCEPTED,     # quest accepted — NPC closing line, then auto-end
	DECLINED,     # quest declined — NPC closing line, then auto-end
	TURN_IN,      # player has an active quest from this NPC (turn-in flow)
	FORCE_CLOSE,  # turn limit reached without quest, NPC closing line then end
}


# TEMPORARY flavour input until entities carry real job data.
@export var npc_job: String = "drifter"
const SITUATIONS := [
	"the player approaches at dusk as a cold wind picks up",
	"the player walks up while the NPC is scavenging through rubble",
	"the player finds the NPC resting beside a dying campfire",
	"the player approaches in a tense, rain-soaked alley",
	"the player meets the NPC at a crowded, makeshift market stall",
	"the player approaches under a harsh midday sun, both low on water",
]

var target_id: int = -1
var _waiting: bool = false
var _display_options: Array = []
var _conversation_active: bool = false
var _situation: String = ""
var _last_npc_line: String = ""
var _history: Array = []
var _npc_turns: int = 0
var _pending_quest_id: String = ""
var _phase: int = Phase.GREETING


func _ready() -> void:
	super._ready()
	ConversationService.generation_completed.connect(_on_generation_completed)
	ConversationService.generation_failed.connect(_on_generation_failed)

func set_params(params: Dictionary) -> void:
	if params.has("target"):
		target_id = params["target"]
	# Resume: if the player exited mid-conversation, pick up where they
	# left off (unless the cooldown expired or the entity died).
	if target_id > 0:
		var saved: Dictionary = _GameWorld.get_entity_social_state(target_id)
		if not saved.is_empty():
			var f := int(_GameWorld.get_entity_friendship(target_id)) if _GameWorld else 50
			if _entity_exists(target_id) and not _can_talk_now(target_id, f):
				_restore_state(saved)
				return
			_GameWorld.clear_entity_social_state(target_id)
	_conversation_active = true
	_history = []
	_npc_turns = 0
	_phase = Phase.GREETING
	_situation = SITUATIONS[randi() % SITUATIONS.size()]
	_update_title()
	_load_relationship_into_bars()
	_pending_quest_id = _resolve_pending_quest_id()
	if _present_active_quest_state():
		return
	_request_npc_line("The traveller walks up and greets you for the first time.")

func _entity_exists(eid: int) -> bool:
	if not _GameWorld:
		return false
	return not _GameWorld.get_entity_name(eid).is_empty()

func _can_talk_now(entity_id: int, friendship: int) -> bool:
	var cooldown_turn: int = _GameWorld.get_entity_social_cooldown(entity_id)
	if cooldown_turn <= 0:
		return true
	return TimeManager.total_turns >= cooldown_turn

func _compute_cooldown_turn(friendship: int) -> int:
	var duration: int
	if friendship <= 50:
		duration = 28800 - friendship * 288
	else:
		duration = 14400 - (friendship - 50) * 144
	return TimeManager.total_turns + duration

func _resolve_pending_quest_id() -> String:
	if target_id <= 0:
		return ""
	for q in QuestService.get_active():
		if int(q.get("giver_entity_id", -1)) == target_id:
			return str(q.get("quest_id", ""))
	var offers: Array = QuestService.get_offers_for(target_id)
	if not offers.is_empty():
		return str(offers[0].get("quest_id", ""))
	# 3) Otherwise, generate a fresh quest for this NPC.
	return QuestService.offer(target_id)

func _present_active_quest_state() -> bool:
	if _pending_quest_id.is_empty():
		return false
	var q: Dictionary = QuestService.get_quest(_pending_quest_id)
	if str(q.get("status", "")) != QuestService.STATUS_ACTIVE:
		return false
	_phase = Phase.TURN_IN
	_waiting = false
	var label: String = str(q.get("label", "that task"))
	if bool(q.get("can_complete", false)) or QuestService.can_complete(_pending_quest_id):
		_last_npc_line = "You made it back. If you have %s finished, I can settle what I owe you." % label
		if npcMessage:
			npcMessage.text = _last_npc_line
		_display_options = [
			{ "text": QUEST_COMPLETE_TEXT, "tone": QUEST_TONE_COMPLETE },
			{ "text": TERMINAL_TEXT,       "tone": "terminal"           },
		]
	else:
		_last_npc_line = "That work is still running: %s. Come back when it is done." % label
		if npcMessage:
			npcMessage.text = _last_npc_line
		_display_options = [
			{ "text": TERMINAL_TEXT, "tone": "terminal" },
		]
	selected_index = 0
	_history.append("%s: %s" % [_npc_label(), _last_npc_line])
	refresh_view()
	return true

func _load_relationship_into_bars() -> void:
	if not _GameWorld:
		return
	if friendshipBar:
		friendshipBar.value = _GameWorld.get_entity_friendship(target_id)
	if romanceBar:
		romanceBar.value = _GameWorld.get_entity_romance(target_id)

func _request_npc_line(player_action: String, extra_directive: String = "") -> void:
	_waiting = true
	_display_options = []
	if npcMessage:
		npcMessage.text = "..."
	refresh_view()
	if stripContainer:
		stripContainer.deselect()
	var messages := _npc_messages(player_action, extra_directive)
	ConversationService.generate(messages, TAG_NPC, 1)

func _npc_messages(player_action: String, extra_directive: String = "") -> Array:
	var system_msg: String = _npc_system_prompt()
	if _npc_turns + 1 == HOOK_TURN and not _pending_quest_id.is_empty():
		system_msg += _quest_injection_block(_pending_quest_id)
	if not extra_directive.is_empty():
		system_msg += "\n" + extra_directive
	print("[ConversationTab] === NPC TURN %d ===\n%s" % [_npc_turns + 1, system_msg])
	return [
		{ "role": "system", "content": system_msg },
		{ "role": "user", "content": player_action + "\nSpeak your reply in character now. Output only your spoken words, nothing else." }
	]

func _quest_injection_block(quest_id: String) -> String:
	var q: Dictionary = QuestService.get_quest(quest_id)
	if q.is_empty():
		return ""
	var label: String = str(q.get("label", ""))
	var details: String = str(q.get("description", ""))
	return ("\n\n[HOOK TURN — this is your one chance to make the ask. State it directly. " +
		"Your line must end with a clear, unambiguous request to the traveller.]\n" +
		"- Request: %s\n" % label +
		"- Why: %s\n" % details +
		"- Stakes: the traveller will be rewarded with goods and your improved disposition.")

func _npc_system_prompt() -> String:
	var npc_name := _GameWorld.get_entity_name(target_id) if _GameWorld else ""
	var gender := _GameWorld.get_entity_gender(target_id) if _GameWorld else ""
	var anatomy := _GameWorld.get_entity_anatomy(target_id) if _GameWorld else {}
	var race: String = anatomy.get("race_id", "person") if not anatomy.is_empty() else "person"
	if npc_name.is_empty():
		npc_name = "a stranger"
	var friendship := int(_GameWorld.get_entity_friendship(target_id)) if _GameWorld else 50
	var romance := int(_GameWorld.get_entity_romance(target_id)) if _GameWorld else 0
	if friendship < 0: friendship = 50
	if romance < 0: romance = 0
	var ctx := _history_block()
	var quest_hint := ""
	if not _pending_quest_id.is_empty():
		var q := QuestService.get_quest(_pending_quest_id)
		if not q.is_empty():
			quest_hint = ("\n[YOU HAVE A REQUEST FOR THE TRAVELLER]\n" +
				"- Need: %s\n" % str(q.get("label", "")) +
				"- Why: %s\n" % str(q.get("description", "")) +
				"- Do NOT mention this yet. Follow the per-turn directive below. " +
				"You will be explicitly told when to make the formal ask.\n")
	var directive := _turn_directive(_npc_turns + 1)
	return ("[WORLD: gritty survival roguelike]\n" +
		"[YOU ARE: %s | Race: %s | Gender: %s | Job: %s]\n" % [npc_name, race, gender, npc_job] +
		"[YOUR FEELINGS TOWARD THE TRAVELLER: Friendship %d/100 (%s), Romance %d/100 (%s)]\n" % [friendship, _standing(friendship), romance, _standing(romance)] +
		"[SITUATION: %s]\n" % _situation +
		ctx +
		quest_hint +
		directive +
		"You are %s. Stay fully in character. Speak ONLY your own dialogue as %s, in first person, " % [npc_name, npc_name] +
		"two or three vivid sentences that fit your feelings and the situation. " +
		"Do NOT write the traveller's words. Do NOT add quotation marks, labels, or names. Output only what you say aloud.")

func _turn_directive(upcoming_turn: int) -> String:
	match upcoming_turn:
		1:
			return "\n[THIS TURN: Open with a brief greeting. Establish the situation you are in. Do NOT mention any task or ask for help yet.]\n"
		2:
			return "\n[THIS TURN: The traveller has just said something to you. React in character — a thought, a question, a memory. Do NOT make any request for help yet.]\n"
		3:
			return "\n[THIS TURN: Now is the time. Make your formal, in-character ask. End your line by clearly stating what you need done.]\n"
		4:
			return "\n[THIS TURN: The traveller has responded to your request. React to what they said — accept, push back, or express gratitude. Do NOT introduce a new request.]\n"
		_:
			return "\n[THIS TURN: The traveller is still here. Begin to wrap up — say goodbye, express gratitude, or hint that the conversation is coming to a natural close. Do NOT introduce a new request.]\n"

func _advance_phase() -> void:
	# Progress the phase machine based on turn count and quest state.
	match _phase:
		Phase.GREETING:
			if _npc_turns >= 3 and not _pending_quest_id.is_empty():
				_phase = Phase.QUEST_OFFER
			elif _npc_turns >= 3:
				_phase = Phase.SMALL_TALK
		Phase.SMALL_TALK:
			if _npc_turns >= FORCE_END_TURN:
				_phase = Phase.FORCE_CLOSE

func _request_options() -> void:
	var sys := _options_system_prompt()
	var usr := "The %s just said to the traveller: \"%s\"\nWrite the traveller's three possible replies now." % [_npc_label(), _last_npc_line]
	print("[ConversationTab] === OPTIONS TURN %d ===\n%s" % [_npc_turns, sys])
	var messages := [
		{ "role": "system", "content": sys },
		{ "role": "user", "content": usr }
	]
	ConversationService.generate(messages, TAG_OPTIONS, 3)

func _options_system_prompt() -> String:
	return ("You write dialogue choices for the player (the traveller) in a survival roguelike. " +
		"You are given what an NPC just said. Write EXACTLY THREE short first-person lines the traveller could say back, " +
		"separated by the pipe character | and nothing else (no JSON, no numbering, no labels, no NPC dialogue). " +
		"The three replies must be in this FIXED order: first friendly/positive, second neutral, third unfriendly/hostile. " +
		"Each reply is the TRAVELLER speaking to the NPC, never the NPC speaking. Never use | inside a line.\n" +
		"Example, if the NPC said 'You picked a bad time to be out here.', a valid reply is:\n" +
		"Glad to find someone friendly out here, mind if I join you? | I'm just passing through, which way to town? | Out of my way, I don't answer to strangers.")

func _npc_label() -> String:
	var n := _GameWorld.get_entity_name(target_id) if _GameWorld else ""
	return n if not n.is_empty() else "stranger"

func _history_block() -> String:
	if _history.is_empty():
		return ""
	var lines := "[RECENT EXCHANGE]\n"
	# Keep only the last few turns to stay within the small context window.
	var start: int = max(0, _history.size() - 4)
	for i in range(start, _history.size()):
		lines += str(_history[i]) + "\n"
	return lines

func _standing(value: int) -> String:
	if value >= 80: return "devoted"
	if value >= 60: return "warm"
	if value >= 40: return "neutral"
	if value >= 20: return "wary"
	return "hostile"

func _build_display_options(line: String) -> void:
	# Split the pipe-delimited player replies, tag by ordinal tone, append exit.
	var toned: Array = []
	for p in line.split("|", false):
		var text := str(p).strip_edges()
		if text.is_empty():
			continue
		toned.append({ "text": text, "tone": TONE_ORDER[toned.size()] })
		if toned.size() == 3:
			break
	toned.append({ "text": TERMINAL_TEXT, "tone": "terminal" })
	_display_options = toned

func _present_quest_options() -> void:
	# Replace the AI-generated 3-tone row with a hard-coded accept/decline/end row
	# so the player can respond to the quest the NPC just offered.
	_waiting = false
	_display_options = [
		{ "text": QUEST_ACCEPT_TEXT,  "tone": QUEST_TONE_ACCEPT  },
		{ "text": QUEST_DECLINE_TEXT, "tone": QUEST_TONE_DECLINE },
		{ "text": TERMINAL_TEXT,      "tone": "terminal"          },
	]
	selected_index = 0
	refresh_view()

func _get_display_data() -> Array:
	if _waiting:
		return [{ "display_name": "[END CONVERSATION]", "font_color": Color(0.65, 0.65, 0.65) }]
	if _is_quest_offer_pending():
		var qdata: Array = []
		for i in range(_display_options.size()):
			var qentry: Dictionary = _display_options[i]
			qdata.append({
				"display_name": _format_quest_option_text(i, qentry),
				"font_color": _color_for_quest_tone(str(qentry.get("tone", "")))
			})
		return qdata
	var data: Array = []
	for i in range(_display_options.size()):
		var entry: Dictionary = _display_options[i]
		var tone := str(entry.get("tone", ""))
		if tone == "terminal":
			data.append({ "display_name": "[END CONVERSATION]", "font_color": Color(0.65, 0.65, 0.65) })
		else:
			data.append({ "display_name": "%d. %s" % [i + 1, str(entry.get("text", ""))] })
	return data

func _is_quest_offer_pending() -> bool:
	if _display_options.is_empty():
		return false
	var first_tone: String = str(_display_options[0].get("tone", ""))
	return first_tone == QUEST_TONE_ACCEPT or first_tone == QUEST_TONE_DECLINE or first_tone == QUEST_TONE_COMPLETE

func _color_for_quest_tone(tone: String) -> Color:
	match tone:
		QUEST_TONE_ACCEPT:
			return Color(0.45, 1.0, 0.5)
		QUEST_TONE_COMPLETE:
			return Color(0.45, 0.8, 1.0)
		QUEST_TONE_DECLINE:
			return Color(1.0, 0.45, 0.45)
		"terminal":
			return Color(0.65, 0.65, 0.65)
		_:
			return Color(1, 1, 1)

func _format_quest_option_text(index: int, entry: Dictionary) -> String:
	var tone: String = str(entry.get("tone", ""))
	if tone == "terminal":
		return "[END CONVERSATION]"
	var tag: String
	match tone:
		QUEST_TONE_ACCEPT: tag = "[ACCEPT QUEST]"
		QUEST_TONE_COMPLETE: tag = "[COMPLETE QUEST]"
		QUEST_TONE_DECLINE: tag = "[DENY QUEST]"
		_: tag = ""
	var text: String = str(entry.get("text", ""))
	if tag.is_empty():
		return "%d. %s" % [index + 1, text]
	return "%d. %s  %s" % [index + 1, tag, text]

func _on_generation_completed(text: String, tag: String) -> void:
	if not _conversation_active or not is_inside_tree():
		return
	if tag == TAG_NPC:
		_last_npc_line = text
		if npcMessage:
			npcMessage.text = text
		_history.append("%s: %s" % [_npc_label(), text])
		_npc_turns += 1
		_advance_phase()
		match _phase:
			Phase.QUEST_OFFER:
				_present_quest_options()
			Phase.ACCEPTED, Phase.DECLINED:
				_waiting = false
				_display_options = [{ "text": TERMINAL_TEXT, "tone": "terminal" }]
				selected_index = 0
				refresh_view()
			Phase.FORCE_CLOSE:
				_end_conversation()
			_:
				_request_options()
		return
	if tag == TAG_OPTIONS:
		# Call 2 done: build the four selectable rows.
		_waiting = false
		_build_display_options(text)
		print("[ConversationTab] options=", _display_options.size())
		refresh_view()

func _on_generation_failed(reason: String, _tag: String) -> void:
	if not _conversation_active or not is_inside_tree():
		return
	_waiting = false
	if npcMessage:
		npcMessage.text = "[i]%s[/i]" % reason
	_display_options = []
	refresh_view()

func _on_item_activated() -> void:
	if _waiting:
		# The only selectable row during an in-flight request is the terminal option.
		_end_conversation()
		return
	if _display_options.is_empty():
		return
	var idx := selected_index
	if idx < 0 or idx >= _display_options.size():
		return
	var entry: Dictionary = _display_options[idx]
	var tone := str(entry.get("tone", ""))
	if tone == "terminal":
		_end_conversation()
		return
	if tone == QUEST_TONE_ACCEPT or tone == QUEST_TONE_DECLINE or tone == QUEST_TONE_COMPLETE:
		_resolve_quest_choice(tone, str(entry.get("text", "")))
		return
	_apply_choice(tone)
	var chosen := str(entry.get("text", ""))
	print("[ConversationTab] traveller chose (%s): %s" % [tone, chosen])
	_history.append("Traveller: %s" % chosen)
	# Continue: ask the NPC to respond to the traveller's chosen line.
	_request_npc_line("The traveller says to you: \"%s\"" % chosen)

func _resolve_quest_choice(tone: String, chosen_text: String) -> void:
	if _pending_quest_id.is_empty():
		return
	var qid := _pending_quest_id
	if tone == QUEST_TONE_ACCEPT:
		if QuestService.accept(qid):
			if npcMessage:
				npcMessage.text = str(npcMessage.text) + "\n\n[i]Quest accepted.[/i]"
		_adjust_relationship(FRIENDSHIP_STEP, 0)
		_phase = Phase.ACCEPTED
	elif tone == QUEST_TONE_COMPLETE:
		if QuestService.complete(qid):
			if npcMessage:
				npcMessage.text = "You kept your word. Here is what I promised."
			_pending_quest_id = ""
			_history.append("Traveller: %s" % chosen_text)
			_end_conversation()
			return
		if npcMessage:
			npcMessage.text = "I still cannot take that off your hands. Make sure you have everything I asked for."
		_display_options = [
			{ "text": TERMINAL_TEXT, "tone": "terminal" },
		]
		selected_index = 0
		_history.append("Traveller: %s" % chosen_text)
		refresh_view()
		return
	else:
		QuestService.decline(qid)
		_phase = Phase.DECLINED
	_pending_quest_id = ""
	_history.append("Traveller: %s" % chosen_text)
	print("[ConversationTab] quest=%s -> %s" % [qid, tone])
	_request_npc_line("The traveller says to you: \"%s\"" % chosen_text)

func _apply_choice(tone: String) -> void:
	match tone:
		"positive":
			_adjust_relationship(FRIENDSHIP_STEP, ROMANCE_STEP)
		"negative":
			_adjust_relationship(-FRIENDSHIP_STEP, -ROMANCE_STEP)
		# "neutral" applies no relationship change.

func _adjust_relationship(df: int, dr: int) -> void:
	if not _GameWorld:
		return
	var f := int(_GameWorld.get_entity_friendship(target_id))
	var r := int(_GameWorld.get_entity_romance(target_id))
	# C++ setters clamp to 0-100.
	_GameWorld.set_entity_friendship(target_id, f + df)
	_GameWorld.set_entity_romance(target_id, r + dr)
	if friendshipBar:
		friendshipBar.value = _GameWorld.get_entity_friendship(target_id)
	if romanceBar:
		romanceBar.value = _GameWorld.get_entity_romance(target_id)

func _end_conversation() -> void:
	var f := int(_GameWorld.get_entity_friendship(target_id)) if _GameWorld else 50
	_GameWorld.set_entity_social_cooldown(target_id, _compute_cooldown_turn(f))
	_GameWorld.set_entity_social_state(target_id, _serialize_state())
	_conversation_active = false
	_waiting = false
	InputManager.pop_mode()

func _serialize_state() -> Dictionary:
	return {
		"history": _history.duplicate(),
		"npc_turns": _npc_turns,
		"last_npc_line": _last_npc_line,
		"pending_quest_id": _pending_quest_id,
		"phase": _phase,
		"display_options": _display_options.duplicate(),
		"situation": _situation,
	}

func _restore_state(saved: Dictionary) -> void:
	_conversation_active = true
	_waiting = false
	_history = saved.get("history", [])
	_npc_turns = int(saved.get("npc_turns", 0))
	_last_npc_line = str(saved.get("last_npc_line", ""))
	_pending_quest_id = str(saved.get("pending_quest_id", ""))
	_phase = int(saved.get("phase", Phase.GREETING))
	_display_options = saved.get("display_options", [])
	if not _display_options is Array:
		_display_options = []
	_situation = str(saved.get("situation", ""))
	if npcMessage:
		npcMessage.text = _last_npc_line
	_update_title()
	_load_relationship_into_bars()
	refresh_view()

func open_setup() -> void:
	InputManager.pop_mode()
	InputManager.toggle_menu("ai_setup")

func _update_title() -> void:
	if not _GameWorld or target_id < 0:
		return
	var win := _find_window()
	if not win:
		return
	var full_name = _GameWorld.get_entity_name(target_id)
	win.title = full_name if not full_name.is_empty() else "Conversation"

func _find_window() -> Window:
	var node := get_parent()
	while node:
		if node is Window:
			return node
		node = node.get_parent()
	return null
