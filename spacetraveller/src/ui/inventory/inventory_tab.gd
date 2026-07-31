extends BaseListTab

@onready var SpacerLabelScene = preload("res://src/ui/spacer_label.tscn")

@export var _GameWorld :GameWorld
@export var weightLabel: RichTextLabel

const MODIFIER_NAMES = {
	"weight": "Weight"
}

const EQUIPMENT_SLOT_ORDER = ["main_hand", "off_hand"]

func _item_label(item_id: String) -> String:
	var item_name := String(ItemDb.get_item_name(item_id))
	return item_name if not item_name.is_empty() else item_id

func _post_inventory(message: String, metadata: Dictionary = {}) -> void:
	EventBus.post("inventory", message, metadata)

func _post_inventory_warning(message: String, metadata: Dictionary = {}) -> void:
	EventBus.post("inventory_warning", message, metadata)

func _category_fields(item_id: String) -> Dictionary:
	var category_id := str(ItemDb.get_item_category(item_id))
	return {
		"category_id": category_id,
		"category_label": str(ItemCategoryDb.get_display_name(category_id)),
		"category_sort": int(ItemCategoryDb.get_sort_rank(category_id))
	}

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
		var wielded_category := _category_fields(item_id)
		formatted.append({
			"id": item_id,
			"amount": 1,
			"display_name": ItemDb.get_item_name(item_id),
			"description": ItemDb.get_item_description(item_id),
			"quantity_text": "Wielded",
			"category_id": wielded_category.category_id,
			"category_label": wielded_category.category_label,
			"category_sort": wielded_category.category_sort,
			"separator_key": "Wielded",
			"separator_label": "Wielded",
			"separator_sort": -1,
			"item_sort": EQUIPMENT_SLOT_ORDER.find(slot_name),
			"is_wielded": true,
			"slot_name": slot_name,
			"handling_ratio": float(slot_data.get("handling_ratio", 1.0)),
			"accuracy_modifier": float(slot_data.get("accuracy_modifier", 0.0)),
			"speed_multiplier": float(slot_data.get("speed_multiplier", 1.0)),
			"damage_multiplier": float(slot_data.get("damage_multiplier", 1.0))
		})

	var inv = _GameWorld.get_entity_inventory(0)
	var items = inv.get("items", {})
	
	var keys = items.keys()
	for item_id in keys:
		var amount = items[item_id]
		var category := _category_fields(item_id)
		formatted.append({
			"id": item_id,
			"amount": amount,
			"display_name": ItemDb.get_item_name(item_id),
			"description": ItemDb.get_item_description(item_id),
			"quantity_text": "x" + str(amount),
			"category_id": category.category_id,
			"category_label": category.category_label,
			"category_sort": category.category_sort,
			"separator_key": category.category_id,
			"separator_label": category.category_label,
			"separator_sort": category.category_sort,
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
		_add_spacer_label("Handling", "%d%%" % roundi(float(item_data.get("handling_ratio", 1.0)) * 100.0))
		_add_spacer_label("Accuracy", "%+.0f points" % (float(item_data.get("accuracy_modifier", 0.0)) * 100.0))
		_add_spacer_label("Speed", "×%.2f" % float(item_data.get("speed_multiplier", 1.0)))
		_add_spacer_label("Damage handling", "×%.2f" % float(item_data.get("damage_multiplier", 1.0)))
	
	var modifiers = ItemDb.get_item_modifiers(item_id)
	
	for key in modifiers.keys():
		var display_name = MODIFIER_NAMES.get(key, key.capitalize())
		var value = modifiers[key]
		
		var inst = SpacerLabelScene.instantiate()
		detailsContainer.add_child(inst)
		inst.Label1.text = display_name
		inst.Label2.text = "%.1f" % value

	var clothing_slots: Array = ItemDb.get_clothing_slots(item_id)
	if not clothing_slots.is_empty():
		_add_spacer_label("Parts", _format_clothing_slot_parts(clothing_slots))
		_add_spacer_label("Layers", _format_clothing_slot_layers(clothing_slots))
		_add_spacer_label("Coverage", _format_clothing_slot_stat(clothing_slots, "coverage", true))
		_add_spacer_label("Bash", _format_clothing_slot_stat(clothing_slots, "bash"))
		_add_spacer_label("Cut", _format_clothing_slot_stat(clothing_slots, "cut"))
		_add_spacer_label("Pierce", _format_clothing_slot_stat(clothing_slots, "pierce"))
		_add_spacer_label("Bash through", _format_clothing_slot_stat(clothing_slots, "bash_transmission", true))

	var weapon_data = ItemDb.get_weapon_data(item_id)
	if not weapon_data.is_empty():
		_add_spacer_label("Damage", str(weapon_data.get("damage", 0.0)))
		_add_spacer_label("Profile", str(weapon_data.get("attack_profile", "")).replace("_", " ").capitalize())
		_add_spacer_label("Manipulation", "%.1f" % float(weapon_data.get("manipulation_load", weapon_data.get("grasp_required", 1.0))))
		_add_spacer_label("Reach", str(int(weapon_data.get("reach", 1))))

func _format_slot_name(slot_name: String) -> String:
	return slot_name.replace("_", " ").capitalize()

func _add_spacer_label(label: String, value: String):
	var inst = SpacerLabelScene.instantiate()
	detailsContainer.add_child(inst)
	inst.Label1.text = label
	inst.Label2.text = value

func _format_clothing_slot_parts(slots: Array) -> String:
	var parts: PackedStringArray = []
	for slot in slots:
		if slot is Dictionary:
			parts.append(str(slot.get("part", "any")).capitalize())
	return ", ".join(parts)

func _format_clothing_slot_layers(slots: Array) -> String:
	var layers: PackedStringArray = []
	for slot in slots:
		if slot is Dictionary:
			var layer := str(slot.get("layer", "middle")).capitalize()
			if not layers.has(layer):
				layers.append(layer)
	return ", ".join(layers)

func _format_clothing_slot_stat(slots: Array, key: String, as_percent := false) -> String:
	var values: PackedStringArray = []
	for slot in slots:
		if slot is Dictionary:
			var part := str(slot.get("part", "any")).capitalize()
			var amount := float(slot.get(key, 0.0))
			var display := "%d%%" % roundi(amount * 100.0) if as_percent else "%.1f" % amount
			values.append("%s %s" % [part, display])
	return ", ".join(values)

func refresh_view() -> void:
	super.refresh_view()

func _on_refresh() -> void:
	_update_totals()

func _update_totals() -> void:
	if weightLabel:
		var total_weight = _GameWorld.get_entity_inventory_weight(0)
		weightLabel.text = "Weight: [color=#66ff66]%.1f[/color]" % total_weight

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
	if _GameWorld.submit_player_wear(item_id):
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
		if _GameWorld.submit_player_unwield(slot_name):
			_post_inventory("You stop wielding %s." % _item_label(item_id), {"item_id": item_id, "slot": slot_name})
			refresh_view()
		else:
			_post_inventory_warning("You cannot stop wielding %s." % _item_label(item_id), {"item_id": item_id, "slot": slot_name})
	else:
		if _GameWorld.submit_player_wield(item_id):
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
	
	if _GameWorld.submit_player_drop(item_id, amount_to_remove):
		_post_inventory("You drop %s." % _format_item_amount(item_id, amount_to_remove), {"item_id": item_id, "amount": amount_to_remove})
		refresh_view()

func _format_item_amount(item_id: String, amount: int) -> String:
	var label = _item_label(item_id)
	if amount <= 1:
		return label
	return "%s x%d" % [label, amount]
