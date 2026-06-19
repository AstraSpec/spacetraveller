extends BaseListTab

@export var _GameWorld: GameWorld
@export var debitLabel: RichTextLabel
@export var npcMoneyLabel: RichTextLabel
@export var playerItems: ButtonListContainer
@export var npcItems: ButtonListContainer

var target_id: int = -1
var active_side: String = "player"
var trade_vendor_id: int = -1
var trade_summary: Dictionary = {}

func _ready() -> void:
	super._ready()
	playerItems.item_selected.connect(func(_index: int, _data: Variant) -> void: _set_active_side("player"))
	playerItems.item_activated.connect(_on_player_item_activated)
	npcItems.item_selected.connect(func(_index: int, _data: Variant) -> void: _set_active_side("npc"))
	npcItems.item_activated.connect(_on_npc_item_activated)

func set_params(params: Dictionary) -> void:
	_reset_ui_state()
	target_id = int(params["target"]) if params.has("target") else _target_from_talk_tab()
	trade_vendor_id = -1
	refresh_view()

func refresh_view() -> void:
	if target_id <= 0:
		target_id = _target_from_talk_tab()

	_ensure_trade_session()
	trade_summary = _GameWorld.trade_get_summary() if _GameWorld else {}

	debitLabel.text = "Credit: %d" % int(trade_summary.get("credit", 0))

	var funds: int = int(trade_summary.get("funds", 0))
	if funds < 0:
		npcMoneyLabel.text = "Funds: [color=red]%d[/color]" % funds
	else:
		npcMoneyLabel.text = "Funds: %d" % funds

	playerItems.set_data(_inventory_strip_data(
		0,
		trade_summary.get("vendor_offer", []),
		trade_summary.get("player_offer", []),
		"pending_from_vendor",
		"player"
	))
	npcItems.set_data(_inventory_strip_data(
		target_id,
		trade_summary.get("player_offer", []),
		trade_summary.get("vendor_offer", []),
		"pending_from_player",
		"npc"
	))
	_update_active_selection()

func handle_directional_input(direction: Vector2) -> void:
	if direction.x < 0:
		if _side_has_items("player"):
			_set_active_side("player")
		return
	if direction.x > 0:
		if _side_has_items("npc"):
			_set_active_side("npc")
		return

	var active_list := _active_list()
	if active_list and direction.y != 0:
		active_list.handle_directional_input(Vector2(0, direction.y))

func _side_has_items(side: String) -> bool:
	if side == "player":
		return playerItems and playerItems.get_button_count() > 0
	elif side == "npc":
		return npcItems and npcItems.get_button_count() > 0
	return false

func _on_item_activated() -> void:
	var active := _active_list()
	if not active:
		return
	var data = active._get_data_for_button_index(active.selected_index)
	_activate_trade_item(data, active_side)

func _on_player_item_activated(_index: int, data: Variant) -> void:
	_activate_trade_item(data, "player")

func _on_npc_item_activated(_index: int, data: Variant) -> void:
	_activate_trade_item(data, "npc")

func _activate_trade_item(data: Variant, side: String) -> void:
	if not data is Dictionary or not _GameWorld:
		return
	_ensure_trade_session()
	var item_id := str(data.get("id", ""))
	var changed := false

	if side == "npc":
		if int(data.get("pending_from_player", 0)) > 0:
			changed = _GameWorld.trade_remove_player_item(item_id, 1)
		else:
			changed = _GameWorld.trade_add_vendor_item(item_id, 1)
	else:
		if int(data.get("pending_from_vendor", 0)) > 0:
			changed = _GameWorld.trade_remove_vendor_item(item_id, 1)
		else:
			changed = _GameWorld.trade_add_player_item(item_id, 1)

	if changed:
		refresh_view()

func _ensure_trade_session() -> void:
	if target_id <= 0:
		return
	if trade_vendor_id == target_id:
		return
	if _GameWorld.begin_trade(target_id):
		trade_vendor_id = target_id

func _set_active_side(side: String) -> void:
	if active_side == side:
		return
	active_side = side
	_update_active_selection()

func _active_list() -> ButtonListContainer:
	if active_side == "npc":
		return npcItems
	return playerItems

func _inactive_list() -> ButtonListContainer:
	if active_side == "npc":
		return playerItems
	return npcItems

func _update_active_selection() -> void:
	var inactive := _inactive_list()
	if inactive:
		inactive.deselect()

	var active := _active_list()
	if not active:
		return
	if active.get_button_count() <= 0:
		active.selected_index = 0
		active.deselect()
		return
	active.selected_index = clamp(active.selected_index, 0, active.get_button_count() - 1)
	active._update_selection_visuals()

func _target_from_talk_tab() -> int:
	var tabs := get_parent()
	if not tabs:
		return -1
	var talk := tabs.get_node_or_null("Talk")
	if talk:
		return int(talk.get("target_id"))
	return -1

func request_menu_close() -> bool:
	if not _has_pending_trade():
		return false
	var popup := _get_confirmation_popup()
	if not popup:
		return false
	popup.show_confirm("Accept this trade?", [
		{"label": "No", "callback": Callable(self, "_discard_trade_and_close")},
		{"label": "Yes", "callback": Callable(self, "_accept_trade_and_close")},
		{"label": "Cancel"},
	])
	return true

func on_menu_closed() -> void:
	if trade_vendor_id > 0:
		_GameWorld.end_trade()
	trade_vendor_id = -1
	_reset_ui_state()

func _has_pending_trade() -> bool:
	if trade_vendor_id <= 0:
		return false
	var summary: Dictionary = _GameWorld.trade_get_summary()
	return not summary.get("player_offer", []).is_empty() or not summary.get("vendor_offer", []).is_empty()

func _get_confirmation_popup() -> ConfirmationPopup:
	return get_tree().root.get_node_or_null("Main/Canvas/Window/ConfirmationPopup") as ConfirmationPopup

func _accept_trade_and_close() -> void:
	if _GameWorld:
		_GameWorld.trade_accept()
	_finish_trade_close()

func _discard_trade_and_close() -> void:
	if _GameWorld:
		_GameWorld.end_trade()
	_finish_trade_close()

func _finish_trade_close() -> void:
	trade_vendor_id = -1
	_reset_ui_state()
	InputManager.pop_mode()

func _reset_ui_state() -> void:
	active_side = "player"
	target_id = -1
	trade_summary = {}
	if playerItems:
		playerItems.selected_index = 0
	if npcItems:
		npcItems.selected_index = 0
		npcItems.deselect()

func _inventory_strip_data(entity_id: int, incoming: Array = [], outgoing: Array = [], pending_key: String = "", side: String = "player") -> Array:
	if entity_id < 0:
		return []

	var inv: Dictionary = _GameWorld.get_entity_inventory(entity_id)
	var items: Dictionary = inv.get("items", {})
	var effective: Dictionary = {}
	for base_item_id in items.keys():
		effective[base_item_id] = int(items[base_item_id])
	for entry in outgoing:
		if not entry is Dictionary:
			continue
		var outgoing_item_id := str(entry.get("id", ""))
		effective[outgoing_item_id] = int(effective.get(outgoing_item_id, 0)) - int(entry.get("amount", 0))

	var data: Array = []
	for display_item_id in effective.keys():
		var amount := int(effective[display_item_id])
		if amount <= 0:
			continue
		var type := str(ItemDb.get_item_type(display_item_id))
		var row := {
			"id": display_item_id,
			"amount": amount,
			"display_name": _item_label(display_item_id),
			"left": _item_label(display_item_id),
			"right": _item_right_label(display_item_id, amount, side),
			"type": type,
			"separator_sort": TYPE_ORDER.find(type) if TYPE_ORDER.find(type) >= 0 else TYPE_ORDER.size(),
		}
		data.append(row)
	_append_pending_rows(data, incoming, pending_key)
	return data

func _append_pending_rows(data: Array, incoming: Array, pending_key: String) -> void:
	if pending_key.is_empty():
		return
	for entry in incoming:
		if not entry is Dictionary:
			continue
		var item_id := str(entry.get("id", ""))
		var amount := int(entry.get("amount", 0))
		if item_id.is_empty() or amount <= 0:
			continue
		var type := str(ItemDb.get_item_type(item_id))
		var row := {
			"id": item_id,
			"amount": amount,
			"display_name": _item_label(item_id),
			"left": _yellow_text(_item_label(item_id)),
			"right": _yellow_text(_item_right_label_with_price(item_id, amount, int(entry.get("unit_value", 0)))),
			"type": type,
			"separator_sort": TYPE_ORDER.find(type) if TYPE_ORDER.find(type) >= 0 else TYPE_ORDER.size(),
		}
		row[pending_key] = amount
		data.append(row)

func _item_label(item_id: String) -> String:
	var item_name := String(ItemDb.get_item_name(item_id))
	return item_name if not item_name.is_empty() else item_id

func _item_right_label(item_id: String, amount: int, side: String) -> String:
	var price :int = int(_GameWorld.trade_get_item_value(item_id, 1, side == "player"))
	return _item_right_label_with_price(item_id, amount, price)

func _item_right_label_with_price(item_id: String, amount: int, price: int) -> String:
	var modifiers: Dictionary = ItemDb.get_item_modifiers(item_id)
	var weight := float(modifiers.get("weight", 0.0))
	return "%sw       %d       x%d" % [_format_decimal(weight), price, amount]

func _yellow_text(text: String) -> String:
	return "[color=yellow]%s[/color]" % text

func _format_decimal(value: float) -> String:
	var text := "%.2f" % value
	while text.ends_with("0"):
		text = text.substr(0, text.length() - 1)
	if text.ends_with("."):
		text = text.substr(0, text.length() - 1)
	return text
