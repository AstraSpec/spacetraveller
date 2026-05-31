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
# Each entry: { "text": String, "tone": "positive"|"neutral"|"negative"|"terminal" }
var _display_options: Array = []
var _conversation_active: bool = false
var _situation: String = ""
# The NPC's most recent spoken line, used to ground the options call and history.
var _last_npc_line: String = ""
# Running transcript of plain spoken lines, fed to both calls for context.
var _history: Array = []

func _ready() -> void:
	super._ready()
	ConversationService.generation_completed.connect(_on_generation_completed)
	ConversationService.generation_failed.connect(_on_generation_failed)

func set_params(params: Dictionary) -> void:
	if params.has("target"):
		target_id = params["target"]
	_conversation_active = true
	_history = []
	_situation = SITUATIONS[randi() % SITUATIONS.size()]
	_update_title()
	_load_relationship_into_bars()
	_request_npc_line("The traveller walks up and greets you for the first time.")

func _load_relationship_into_bars() -> void:
	if not _GameWorld:
		return
	if friendshipBar:
		friendshipBar.value = _GameWorld.get_entity_friendship(target_id)
	if romanceBar:
		romanceBar.value = _GameWorld.get_entity_romance(target_id)

# --- Call 1: the NPC's spoken line (pure in-character prose, no structure) ---

func _request_npc_line(player_action: String) -> void:
	_waiting = true
	_display_options = []
	if npcMessage:
		npcMessage.text = "..."
	refresh_view()
	var messages := [
		{ "role": "system", "content": _npc_system_prompt() },
		{ "role": "user", "content": player_action + "\nSpeak your reply in character now. Output only your spoken words, nothing else." }
	]
	ConversationService.generate(messages, TAG_NPC, 1)

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
	return ("[WORLD: gritty survival roguelike]\n" +
		"[YOU ARE: %s | Race: %s | Gender: %s | Job: %s]\n" % [npc_name, race, gender, npc_job] +
		"[YOUR FEELINGS TOWARD THE TRAVELLER: Friendship %d/100 (%s), Romance %d/100 (%s)]\n" % [friendship, _standing(friendship), romance, _standing(romance)] +
		"[SITUATION: %s]\n" % _situation +
		ctx +
		"You are %s. Stay fully in character. Speak ONLY your own dialogue as %s, in first person, " % [npc_name, npc_name] +
		"two or three vivid sentences that fit your feelings and the situation. " +
		"Do NOT write the traveller's words. Do NOT add quotation marks, labels, or names. Output only what you say aloud.")

# --- Call 2: the three traveller replies, in response to the NPC line ---

func _request_options() -> void:
	var messages := [
		{ "role": "system", "content": _options_system_prompt() },
		{ "role": "user", "content": "The %s just said to the traveller: \"%s\"\nWrite the traveller's three possible replies now." % [_npc_label(), _last_npc_line] }
	]
	ConversationService.generate(messages, TAG_OPTIONS, 3)

func _options_system_prompt() -> String:
	# Focused, mechanical task: write THREE first-person lines for the PLAYER.
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

func _get_display_data() -> Array:
	if _waiting:
		# Keep the terminal option selectable during an in-flight request.
		return [{ "display_name": TERMINAL_TEXT }]
	var data: Array = []
	for i in range(_display_options.size()):
		var entry: Dictionary = _display_options[i]
		data.append({ "display_name": "%d. %s" % [i + 1, str(entry.get("text", ""))] })
	return data

func _on_generation_completed(text: String, tag: String) -> void:
	if not _conversation_active or not is_inside_tree():
		return
	if tag == TAG_NPC:
		# Call 1 done: show the spoken line, record it, then fire call 2.
		_last_npc_line = text
		if npcMessage:
			npcMessage.text = text
		_history.append("%s: %s" % [_npc_label(), text])
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
	_apply_choice(tone)
	var chosen := str(entry.get("text", ""))
	print("[ConversationTab] traveller chose (%s): %s" % [tone, chosen])
	_history.append("Traveller: %s" % chosen)
	# Continue: ask the NPC to respond to the traveller's chosen line.
	_request_npc_line("The traveller says to you: \"%s\"" % chosen)

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
	_conversation_active = false
	_waiting = false
	InputManager.pop_mode()

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
