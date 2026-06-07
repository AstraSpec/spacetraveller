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
			var data = ItemDb.get_clothing_data(item_id)
			var part_key = data.get("part", "other")
			
			formatted.append({
				"id": item_id,
				"display_name": ItemDb.get_item_name(item_id),
				"description": ItemDb.get_item_description(item_id),
				"part_name": _GameWorld.get_entity_anatomy_part_name(0, int(part_idx)),
				"layer": layer,
				"quantity_text": "",
				"type": ItemDb.get_item_type(item_id),
				"separator_key": part_key
			})
	return formatted

func _update_details_ui(item_data: Dictionary) -> void:
	if not detailsContainer: return
	
	for child in detailsContainer.get_children():
		child.queue_free()
		
	var item_id = item_data.get("id", "")
	if item_id == "": return
	
	var data = ItemDb.get_clothing_data(item_id)
	
	if not data.is_empty():
		_add_detail("Armor", str(data.get("armor", 0.0)))
		_add_detail("Part Type", data.get("part", "any").capitalize())
		_add_detail("Layer", data.get("layer", "middle").capitalize())
		_add_detail("Coverage", str(data.get("coverage", 1.0) * 100) + "%")

func _add_detail(label: String, value: String):
	var inst = SpacerLabelScene.instantiate()
	detailsContainer.add_child(inst)
	inst.Label1.text = label
	inst.Label2.text = value

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
		EventBus.post("inventory_warning", "You cannot take off %s. Carry weight or volume is over limit." % _item_label(item_id), {"item_id": item_id})
		return
	if _GameWorld.unequip_entity_clothing_by_string(0, item_id):
		EventBus.post("inventory", "You take off %s." % _item_label(item_id), {"item_id": item_id})
		refresh_view()
	else:
		_GameWorld.remove_entity_inventory_item(0, item_id, 1)
		EventBus.post("inventory_warning", "You cannot take off %s." % _item_label(item_id), {"item_id": item_id})
