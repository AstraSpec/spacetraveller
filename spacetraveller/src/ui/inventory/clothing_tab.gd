extends BaseListTab

@onready var SpacerLabelScene = preload("res://src/ui/spacer_label.tscn")

@export var clothing: Clothing
@export var armorLabel: RichTextLabel

func _get_display_data() -> Array:
	if not clothing: return []
	
	var items = clothing.get_equipped_items_list()
	var formatted = []
	
	for entry in items:
		var item_id = entry["id"]
		formatted.append({
			"id": item_id,
			"display_name": ItemDb.get_item_name(item_id),
			"description": ItemDb.get_item_description(item_id),
			"part_name": entry["part_name"],
			"layer": entry["layer"],
			"quantity_text": entry["part_name"]
		})
	return formatted

func _update_details_ui(item_data: Dictionary) -> void:
	if not detailsContainer: return
	
	# Clear old children
	for child in detailsContainer.get_children():
		child.queue_free()
		
	var item_id = item_data["id"]
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
	_update_armor_display()

func _update_armor_display() -> void:
	if armorLabel and clothing:
		var total_armor = clothing.get_total_armor()
		armorLabel.text = "Total Armor: [color=#66ff66]%.1f[/color]" % total_armor

func handle_action(action_name: String, _params: Dictionary = {}):
	if action_name == "wear":
		_unequip_selected_item()

func _unequip_selected_item():
	if _items_cache.is_empty() or selected_index < 0 or selected_index >= _items_cache.size():
		return
	
	var item_data = _items_cache[selected_index]
	var item_id = item_data["id"]
	
	# Try to move back to inventory
	var player = clothing.get_parent()
	if player and "inventory" in player and player.inventory:
		var inv = player.inventory
		if inv.add_item(item_id, 1):
			clothing.unequip_item(item_id)
			TimeManager.advance_turn()
			refresh_view()
	else:
		clothing.unequip_item(item_id)
		TimeManager.advance_turn()
		refresh_view()
