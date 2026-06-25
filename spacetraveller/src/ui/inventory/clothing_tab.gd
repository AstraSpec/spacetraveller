extends BaseListTab

@onready var SpacerLabelScene = preload("res://src/ui/spacer_label.tscn")

@export var _GameWorld :GameWorld
@export var armorLabel :RichTextLabel

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
				"type": ItemDb.get_item_type(item_id),
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
		_add_detail("Armor", _format_clothing_slot_armor(slots))

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

func _format_clothing_slot_armor(slots: Array) -> String:
	var values: PackedStringArray = []
	for slot in slots:
		if slot is Dictionary:
			values.append("%s %.1f" % [str(slot.get("part", "any")).capitalize(), float(slot.get("armor", 0.0))])
	return ", ".join(values)

func refresh_view() -> void:
	super.refresh_view()

func _on_refresh() -> void:
	_update_armor_display()

func _update_armor_display() -> void:
	if armorLabel:
		var total_armor = _GameWorld.get_entity_armor_rating(0)
		armorLabel.text = "Total Armor: [color=#66ff66]%.1f[/color]" % total_armor

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
	if not _GameWorld.add_entity_inventory_item(0, item_id, 1):
		EventBus.post("inventory_warning", "You cannot take off %s. Carry weight is over limit." % _item_label(item_id), {"item_id": item_id})
		return
	if _GameWorld.unequip_entity_clothing_by_string(0, item_id):
		EventBus.post("inventory", "You take off %s." % _item_label(item_id), {"item_id": item_id})
		refresh_view()
	else:
		_GameWorld.remove_entity_inventory_item(0, item_id, 1)
		EventBus.post("inventory_warning", "You cannot take off %s." % _item_label(item_id), {"item_id": item_id})
