extends BaseListTab

@onready var SpacerLabelScene = preload("res://src/ui/spacer_label.tscn")

@export var _GameWorld :GameWorld
@export var weightVolumeLabel: RichTextLabel

const MODIFIER_NAMES = {
	"weight": "Weight",
	"volume": "Volume"
}

const EQUIPMENT_SLOT_ORDER = ["main_hand", "off_hand"]

func _item_label(item_id: String) -> String:
	var item_name := String(ItemDb.get_item_name(item_id))
	return item_name if not item_name.is_empty() else item_id

func _post_inventory(message: String, metadata: Dictionary = {}) -> void:
	EventBus.post("inventory", message, metadata)

func _post_inventory_warning(message: String, metadata: Dictionary = {}) -> void:
	EventBus.post("inventory_warning", message, metadata)

func _get_display_data() -> Array:
	var formatted = []
	var equipment = _GameWorld.get_entity_equipment(0)
	for slot_name in EQUIPMENT_SLOT_ORDER:
		if not equipment.has(slot_name):
			continue
		var slot_data = equipment[slot_name]
		if not slot_data is Dictionary:
			continue
		var item_id = slot_data.get("item_id", "")
		if item_id == "":
			continue
		formatted.append({
			"id": item_id,
			"amount": 1,
			"display_name": ItemDb.get_item_name(item_id),
			"description": ItemDb.get_item_description(item_id),
			"quantity_text": "Wielded",
			"type": ItemDb.get_item_type(item_id),
			"separator_key": "Wielded",
			"separator_sort": -1,
			"is_wielded": true,
			"slot_name": slot_name
		})

	var inv = _GameWorld.get_entity_inventory(0)
	var items = inv.get("items", {})
	
	var keys = items.keys()
	for item_id in keys:
		var amount = items[item_id]
		var item_type = ItemDb.get_item_type(item_id)
		formatted.append({
			"id": item_id,
			"amount": amount,
			"display_name": ItemDb.get_item_name(item_id),
			"description": ItemDb.get_item_description(item_id),
			"quantity_text": "x" + str(amount),
			"type": item_type,
			"separator_key": item_type,
			"separator_sort": TYPE_ORDER.find(item_type) if TYPE_ORDER.find(item_type) >= 0 else TYPE_ORDER.size(),
			"is_wielded": false
		})
	return formatted

func _update_details_ui(item_data: Dictionary) -> void:
	if not detailsContainer: return
	
	for child in detailsContainer.get_children():
		child.queue_free()
		
	var item_id = item_data.get("id", "")
	if item_id == "": return

	if item_data.get("is_wielded", false):
		_add_spacer_label("State", "Wielded")
		_add_spacer_label("Slot", _format_slot_name(str(item_data.get("slot_name", ""))))
	
	var modifiers = ItemDb.get_item_modifiers(item_id)
	
	for key in modifiers.keys():
		var display_name = MODIFIER_NAMES.get(key, key.capitalize())
		var value = modifiers[key]
		
		var inst = SpacerLabelScene.instantiate()
		detailsContainer.add_child(inst)
		inst.Label1.text = display_name
		inst.Label2.text = "%.1f" % value

	var clothing_data = ItemDb.get_clothing_data(item_id)
	if not clothing_data.is_empty():
		_add_spacer_label("Part", clothing_data.get("part", "any").capitalize())
		_add_spacer_label("Layer", clothing_data.get("layer", "middle").capitalize())
		_add_spacer_label("Armor", str(clothing_data.get("armor", 0.0)))

	var weapon_data = ItemDb.get_weapon_data(item_id)
	if not weapon_data.is_empty():
		_add_spacer_label("Damage", str(weapon_data.get("damage", 0.0)))
		_add_spacer_label("Style", str(weapon_data.get("style", "")).capitalize())
		_add_spacer_label("Hands", str(weapon_data.get("grasp_required", 1)))

func _format_slot_name(slot_name: String) -> String:
	return slot_name.replace("_", " ").capitalize()

func _add_spacer_label(label: String, value: String):
	var inst = SpacerLabelScene.instantiate()
	detailsContainer.add_child(inst)
	inst.Label1.text = label
	inst.Label2.text = value

func refresh_view() -> void:
	super.refresh_view()

func _on_refresh() -> void:
	_update_totals()

func _update_totals() -> void:
	if weightVolumeLabel:
		var total_weight = _GameWorld.get_entity_inventory_weight(0)
		var total_volume = _GameWorld.get_entity_inventory_volume(0)
		weightVolumeLabel.text = "Weight: [color=#66ff66]%.1f[/color]\nVolume: [color=#66ff66]%.1f[/color]" % [total_weight, total_volume]

func handle_action(action_name: String, params: Dictionary = {}):
	if action_name == "drop":
		_drop_selected_item(params.get("all", false))
	elif action_name == "wear":
		_wear_selected_item()
	elif action_name == "wield":
		_toggle_wield_selected_item()

func _wear_selected_item():
	if _items_cache.is_empty() or selected_index < 0:
		return
	
	var item_data = _items_cache[selected_index]
	if item_data.get("is_wielded", false):
		return
	var item_id = item_data["id"]
	if _GameWorld.equip_entity_clothing_by_string(0, item_id):
		_GameWorld.remove_entity_inventory_item(0, item_id, 1)
		_post_inventory("You wear %s." % _item_label(item_id), {"item_id": item_id})
		refresh_view()
	else:
		_post_inventory_warning("You cannot wear %s." % _item_label(item_id), {"item_id": item_id})

func _toggle_wield_selected_item():
	if _items_cache.is_empty() or selected_index < 0:
		return

	var item_data = _items_cache[selected_index]
	var item_id = item_data.get("id", "")
	if item_id == "":
		return

	if item_data.get("is_wielded", false):
		var slot_name = str(item_data.get("slot_name", ""))
		if slot_name == "":
			return
		if not _GameWorld.add_entity_inventory_item(0, item_id, 1):
			_post_inventory_warning("You cannot stop wielding %s. Carry weight or volume is over limit." % _item_label(item_id), {"item_id": item_id})
			return
		if _GameWorld.unwield_entity_weapon(0, slot_name):
			_post_inventory("You stop wielding %s." % _item_label(item_id), {"item_id": item_id, "slot": slot_name})
			refresh_view()
		else:
			_GameWorld.remove_entity_inventory_item(0, item_id, 1)
			_post_inventory_warning("You cannot stop wielding %s." % _item_label(item_id), {"item_id": item_id, "slot": slot_name})
	else:
		if _GameWorld.wield_entity_weapon_by_string(0, item_id):
			_GameWorld.remove_entity_inventory_item(0, item_id, 1)
			_post_inventory("You wield %s." % _item_label(item_id), {"item_id": item_id})
			refresh_view()
		else:
			_post_inventory_warning("You cannot wield %s." % _item_label(item_id), {"item_id": item_id})

func _drop_selected_item(all: bool):
	if _items_cache.is_empty() or selected_index < 0:
		return
	
	var item_data = _items_cache[selected_index]
	if item_data.get("is_wielded", false):
		return
	var item_id = item_data["id"]
	var amount_to_remove = item_data["amount"] if all else 1
	
	if _GameWorld.remove_entity_inventory_item(0, item_id, amount_to_remove):
		InputManager.inventory_item_dropped.emit(item_id, amount_to_remove)
		_post_inventory("You drop %s." % _format_item_amount(item_id, amount_to_remove), {"item_id": item_id, "amount": amount_to_remove})
		TimeManager.advance_turn()
		refresh_view()

func _format_item_amount(item_id: String, amount: int) -> String:
	var label = _item_label(item_id)
	if amount <= 1:
		return label
	return "%s x%d" % [label, amount]
