extends BaseListTab

@onready var SpacerLabelScene = preload("res://src/ui/spacer_label.tscn")

@export var _GameWorld :GameWorld

func _get_display_data() -> Array:
	var clothing = _GameWorld.get_entity_clothing(0)
	var formatted = []
	
	var equipped = clothing.get("equipped", {})
	var part_keys = equipped.keys()
	
	for part_idx in part_keys:
		var layers = equipped[part_idx]
		var layer_keys = layers.keys()
		
		for layer in layer_keys:
			var item_id = layers[layer]
			var part_name = _GameWorld.get_entity_anatomy_part_name(0, int(part_idx))
			
			formatted.append({
				"id": item_id,
				"display_name": ItemDb.get_item_name(item_id),
				"description": ItemDb.get_item_description(item_id),
				"part_name": part_name,
				"layer": layer,
				"quantity_text": "",
				"separator_key": part_name
			})
	return formatted

func _update_details_ui(item_data: Dictionary) -> void:
	if not detailsContainer: return
	
	for child in detailsContainer.get_children():
		child.queue_free()
		
	var item_id = item_data.get("id", "")
	if item_id == "": return
	
	var slots: Array = ItemDb.get_clothing_slots(item_id)
	
	if not slots.is_empty():
		_add_detail("Parts", _format_clothing_slot_parts(slots))
		_add_detail("Layers", _format_clothing_slot_layers(slots))
		_add_detail("Coverage", _format_clothing_slot_stat(slots, "coverage", true))
		_add_detail("Bash", _format_clothing_slot_stat(slots, "bash"))
		_add_detail("Cut", _format_clothing_slot_stat(slots, "cut"))
		_add_detail("Pierce", _format_clothing_slot_stat(slots, "pierce"))
		_add_detail("Bash through", _format_clothing_slot_stat(slots, "bash_transmission", true))

func _add_detail(label: String, value: String):
	var inst = SpacerLabelScene.instantiate()
	detailsContainer.add_child(inst)
	inst.Label1.text = label
	inst.Label2.text = value

func _format_clothing_slot_parts(slots: Array) -> String:
	var parts: PackedStringArray = []
	for slot in slots:
		if slot is Dictionary:
			var part := str(slot.get("part", "any")).capitalize()
			if not parts.has(part):
				parts.append(part)
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

func _item_label(item_id: String) -> String:
	var item_name := String(ItemDb.get_item_name(item_id))
	return item_name if not item_name.is_empty() else item_id

func handle_action(action_name: String, _params: Dictionary = {}):
	if action_name == "wear":
		_unequip_selected_item()

func _unequip_selected_item():
	if _items_cache.is_empty() or selected_index < 0:
		return
	
	var item_id = _items_cache[selected_index]["id"]
	if _GameWorld.submit_player_remove_clothing(item_id):
		EventBus.post("inventory", "You take off %s." % _item_label(item_id), {"item_id": item_id})
		refresh_view()
	else:
		EventBus.post("inventory_warning", "You cannot take off %s." % _item_label(item_id), {"item_id": item_id})
