extends BaseListTab
class_name NpcActionTab

enum ActionView { ROOT, BODY_PARTS }

@export var _GameWorld: GameWorld
@export var _ConfirmationPopup: Window

var target_id: int = -1
var _view: ActionView = ActionView.ROOT
var _selected_attack: Dictionary = {}
var _registry := NpcActionRegistry.new()

func _ready() -> void:
	super._ready()
	_registry.register_provider(Callable(self, "_build_attack_actions"))
	_registry.register_provider(Callable(self, "_build_follow_action"))

func set_params(params: Dictionary) -> void:
	var next_target := int(params.get("target", -1))
	if next_target != target_id:
		target_id = next_target
		_view = ActionView.ROOT
		_selected_attack.clear()
	refresh_view()

func refresh_view() -> void:
	var talk_tab = get_parent().get_node("Talk")
	if int(talk_tab.target_id) != target_id:
		target_id = int(talk_tab.target_id)
		_view = ActionView.ROOT
		_selected_attack.clear()
	super.refresh_view()

func _get_display_data() -> Array:
	if _view == ActionView.BODY_PARTS:
		return _get_body_part_data()
	return _get_action_data()

func _get_action_data() -> Array:
	var result: Array = []
	if target_id <= 0:
		return [{"display_name": "No NPC selected", "disabled": true}]

	for action in _registry.build(_GameWorld, target_id):
		result.append(action.to_display_data())
	return result

func _build_attack_actions(world: GameWorld, npc_id: int) -> Array:
	var result: Array = []
	for option_value in world.get_player_attack_options(npc_id):
		if not option_value is Dictionary:
			continue
		var option: Dictionary = option_value
		var ability_id := str(option.get("id", ""))
		if ability_id.is_empty():
			continue
		var ability_name := str(option.get("display_name", ability_id.capitalize()))
		var verb := str(option.get("verb", "attack"))
		var disabled := bool(option.get("disabled", false))
		result.append(NpcAction.new(
			ability_id,
			ability_name,
			"Use %s against this NPC." % verb,
			"attack",
			{"verb": verb},
			disabled
		))
	return result

func _build_follow_action(world: GameWorld, npc_id: int) -> Array:
	var following := world.get_entity_behavior_state(npc_id).to_lower() == "follow"
	if following:
		return [NpcAction.new(
			"stop_follow",
			"Stop Following",
			"Ask this NPC to stop following you.",
			"follow",
			{}
		)]

	return [NpcAction.new(
		"follow",
		"Follow",
		"Ask this NPC to follow you.",
		"follow",
		{}
	)]

func _get_body_part_data() -> Array:
	var result: Array = []
	for part_value in _GameWorld.get_entity_targetable_body_parts(target_id):
		if not part_value is Dictionary:
			continue
		var part: Dictionary = part_value
		var part_index := int(part.get("index", -1))
		if part_index < 0:
			continue
		var part_name := str(part.get("name", "Body part"))
		result.append({
			"id": part_index,
			"kind": "body_part",
			"body_part_index": part_index,
			"display_name": part_name,
			"description": "Select %s as the target." % part_name
		})
	return result

func _on_item_activated() -> void:
	if _items_cache.is_empty() or selected_index < 0 or selected_index >= _items_cache.size():
		return

	var item_data: Dictionary = _items_cache[selected_index]
	if bool(item_data.get("disabled", false)):
		return

	if _view == ActionView.ROOT:
		if str(item_data.get("kind", "")) == "follow":
			_activate_follow_action(str(item_data.get("id", "")))
			return
		if str(item_data.get("kind", "")) != "attack":
			return
		_selected_attack = item_data.duplicate(true)
		_view = ActionView.BODY_PARTS
		selected_index = 0
		refresh_view()
		return

	_show_attack_confirmation(item_data)

func _activate_follow_action(action_id: String) -> void:
	var success := false
	if action_id == "follow":
		success = _GameWorld.start_follow(target_id)
	elif action_id == "stop_follow":
		success = _GameWorld.set_entity_behavior(target_id, "idle")

	if success:
		InputManager.pop_mode()

func _show_attack_confirmation(body_part: Dictionary) -> void:
	var ability_name := str(_selected_attack.get("display_name", "Attack"))
	var part_name := str(body_part.get("display_name", "body part"))
	var target_name := _GameWorld.get_entity_name(target_id)
	var message := "Attack %s with %s at %s?" % [target_name, ability_name, part_name]
	_ConfirmationPopup.show_confirm(
		message,
		[
			{"label": "No"},
			{
				"label": "Yes",
				"callback": Callable(self, "_confirm_attack").bind(int(body_part.get("body_part_index", -1)))
			}
		]
	)

func _confirm_attack(body_part_index: int) -> void:
	var cost := _GameWorld.submit_player_targeted_attack(
		target_id,
		str(_selected_attack.get("id", "")),
		body_part_index
	)
	if cost > 0.0:
		InputManager.pop_mode()
		return

	EventBus.post("combat_warning", "That attack is no longer available.", {"target": target_id})
	_view = ActionView.BODY_PARTS
	refresh_view()

func request_menu_close() -> bool:
	if _view != ActionView.BODY_PARTS:
		return false
	_view = ActionView.ROOT
	_selected_attack.clear()
	selected_index = 0
	refresh_view()
	return true

func on_menu_closed() -> void:
	_view = ActionView.ROOT
	_selected_attack.clear()

func _on_refresh() -> void:
	var selected_data: Variant = stripContainer._get_data_for_button_index(selected_index)
	if selected_data is Dictionary:
		return
	if _view == ActionView.BODY_PARTS:
		titleLabel.text = "Choose a body part"
		descriptionLabel.text = "Escape returns to the attack list."
	else:
		titleLabel.text = "NPC actions"
		descriptionLabel.text = "Choose an action for this NPC."
