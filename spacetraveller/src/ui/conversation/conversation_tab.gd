extends BaseListTab

@export var _GameWorld: GameWorld
@export var npcMessage: RichTextLabel
@export var friendshipBar: ProgressBar
@export var romanceBar: ProgressBar

var target_id: int = -1
var _conversation_active: bool = false
var _state: Dictionary = {}
var _display_options: Array = []

func set_params(params: Dictionary) -> void:
	if params.has("target"):
		target_id = int(params["target"])
	_update_title()
	_load_relationship_into_bars()
	if target_id <= 0 or not _GameWorld:
		_show_empty("There is no one here to talk to.")
		return

	var saved: Dictionary = _GameWorld.get_entity_social_state(target_id)
	if not saved.is_empty():
		if _entity_exists(target_id):
			_state = DialogueService.start_state(_GameWorld, target_id, saved)
		else:
			_GameWorld.clear_entity_social_state(target_id)
			_state = DialogueService.start_state(_GameWorld, target_id)
	else:
		_state = DialogueService.start_state(_GameWorld, target_id)

	if _state.is_empty():
		_show_empty("They have nothing to say.")
		return

	_conversation_active = true
	_present_current_node()

func _entity_exists(eid: int) -> bool:
	return _GameWorld and not _GameWorld.get_entity_name(eid).is_empty()

func _present_current_node(extra_message: String = "") -> void:
	if not _conversation_active:
		return
	var text := DialogueService.get_npc_text(_state, _GameWorld, target_id)
	if not extra_message.is_empty():
		text += "\n\n[i]%s[/i]" % extra_message
	if npcMessage:
		npcMessage.text = text
	_display_options = DialogueService.get_options(_state, _GameWorld, target_id)
	selected_index = 0
	refresh_view()

func _get_display_data() -> Array:
	var data: Array = []
	for i in range(_display_options.size()):
		var entry: Dictionary = _display_options[i]
		var text := str(entry.get("text", ""))
		var effects: Dictionary = entry.get("effects", {})
		var quest_offer = effects.get("quest_offer", false)
		var has_quest_offer: bool = (typeof(quest_offer) == TYPE_STRING and not str(quest_offer).is_empty()) or (typeof(quest_offer) == TYPE_BOOL and bool(quest_offer))
		var ui_color := str(entry.get("ui_color", ""))
		var color := Color(1, 1, 1)
		if ui_color == "quest_accept":
			color = Color(0.45, 1.0, 0.45)
		elif ui_color == "quest_decline":
			color = Color(1.0, 0.35, 0.35)
		elif bool(effects.get("end_conversation", false)):
			color = Color(0.65, 0.65, 0.65)
		elif bool(effects.get("start_fight", false)) or int(effects.get("friendship_delta", 0)) < 0:
			color = Color(1.0, 0.45, 0.45)
		elif bool(effects.get("quest_accept", false)) or bool(effects.get("quest_complete", false)):
			color = Color(0.45, 0.9, 1.0)
		elif has_quest_offer:
			color = Color(0.75, 1.0, 0.55)
		data.append({
			"display_name": "%d. %s" % [i + 1, text],
			"font_color": color,
		})
	return data

func _on_item_activated() -> void:
	if not _conversation_active or _display_options.is_empty():
		_end_conversation(false)
		return
	var idx := selected_index
	if idx < 0 or idx >= _display_options.size():
		return
	var result := DialogueService.apply_option(_display_options[idx], _state, _GameWorld, target_id)
	_state = result.get("state", _state)
	_load_relationship_into_bars()

	if bool(result.get("start_fight", false)):
		_start_fight()
		_end_conversation(false)
		return
	if bool(result.get("end_conversation", false)):
		_end_conversation(true)
		return
	_present_current_node(str(result.get("message", "")))

func _start_fight() -> void:
	if not _GameWorld or target_id <= 0:
		return
	var pos: Vector2i = _GameWorld.get_entity_position(target_id)
	_GameWorld.submit_player_intent(GameWorld.INTENT_ATTACK, pos.x, pos.y, "")

func _load_relationship_into_bars() -> void:
	if not _GameWorld or target_id <= 0:
		return
	if friendshipBar:
		friendshipBar.value = _GameWorld.get_entity_friendship(target_id)
	if romanceBar:
		romanceBar.value = _GameWorld.get_entity_romance(target_id)

func _end_conversation(save_state: bool = true) -> void:
	if _GameWorld and target_id > 0:
		_GameWorld.set_entity_social_cooldown(target_id, 0)
		if save_state and DialogueService.should_save_state(_state):
			_GameWorld.set_entity_social_state(target_id, _state)
		else:
			_GameWorld.clear_entity_social_state(target_id)
	_conversation_active = false
	_display_options = []
	InputManager.pop_mode()

func _show_empty(text: String) -> void:
	_conversation_active = false
	_state = {}
	_display_options = [{ "text": "End conversation.", "effects": { "end_conversation": true } }]
	if npcMessage:
		npcMessage.text = text
	refresh_view()

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
