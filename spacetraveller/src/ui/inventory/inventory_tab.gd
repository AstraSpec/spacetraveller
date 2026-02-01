extends BaseListTab

@onready var SpacerLabelScene = preload("res://src/ui/spacer_label.tscn")

@export var inventory: Inventory
@export var weightVolumeLabel: RichTextLabel

const MODIFIER_NAMES = {
	"weight": "Weight",
	"volume": "Volume"
}

func _get_display_data() -> Array:
	if not inventory: return []
	
	var items = inventory.get_items_list()
	var formatted = []
	
	for item in items:
		formatted.append({
			"id": item["id"],
			"amount": item["amount"],
			"display_name": ItemDb.get_item_name(item["id"]),
			"description": ItemDb.get_item_description(item["id"]),
			"quantity_text": "x" + str(item["amount"])
		})
	return formatted

func _update_details_ui(item_data: Dictionary) -> void:
	if not detailsContainer: return
	
	# Clear old children
	for child in detailsContainer.get_children():
		child.queue_free()
		
	var item_id = item_data["id"]
	var modifiers = ItemDb.get_item_modifiers(item_id)
	
	for key in modifiers.keys():
		var display_name = MODIFIER_NAMES.get(key, key.capitalize())
		var value = modifiers[key]
		
		var inst = SpacerLabelScene.instantiate()
		detailsContainer.add_child(inst)
		inst.Label1.text = display_name
		inst.Label2.text = "%.1f" % value

	# Add clothing info if applicable
	var clothing_data = ItemDb.get_clothing_data(item_id)
	if not clothing_data.is_empty():
		_add_spacer_label("Part", clothing_data.get("part", "any").capitalize())
		_add_spacer_label("Layer", clothing_data.get("layer", "middle").capitalize())
		_add_spacer_label("Armor", str(clothing_data.get("armor", 0.0)))

func _add_spacer_label(label: String, value: String):
	var inst = SpacerLabelScene.instantiate()
	detailsContainer.add_child(inst)
	inst.Label1.text = label
	inst.Label2.text = value

func refresh_view() -> void:
	super.refresh_view()
	_update_totals()

func _update_totals() -> void:
	if weightVolumeLabel and inventory:
		var total_weight = inventory.get_total_weight()
		var total_volume = inventory.get_total_volume()
		weightVolumeLabel.text = "Weight: [color=#66ff66]%.1f[/color]\nVolume: [color=#66ff66]%.1f[/color]" % [total_weight, total_volume]

func handle_action(action_name: String, params: Dictionary = {}):
	if action_name == "drop":
		_drop_selected_item(params.get("all", false))
	elif action_name == "wear":
		_wear_selected_item()

func _wear_selected_item():
	if _items_cache.is_empty() or selected_index < 0 or selected_index >= _items_cache.size():
		return
	
	var item_data = _items_cache[selected_index]
	var item_id = item_data["id"]
	
	# Check if wearable
	if not ItemDb.has_tag(item_id, "WEARABLE"):
		return
		
	# Find player more robustly
	var player = null
	var parent = inventory.get_parent()
	if "Player" in parent:
		player = parent.Player
	elif parent.has_node("Player"):
		player = parent.get_node("Player")
		
	if not player: return

	var clothing = player.get_node_or_null("Clothing")
	var anatomy = player.get_node_or_null("Anatomy")
	
	if clothing and anatomy:
		var clothing_data = ItemDb.get_clothing_data(item_id)
		var part_type = clothing_data.get("part", "")
		
		# Find a suitable part instance
		var part_index = -1
		for i in range(anatomy.get_part_count()):
			if anatomy.get_part_type_id(i) == part_type or part_type == "":
				if anatomy.is_part_functional(i):
					# Check if this layer is already occupied on this part?
					# For now, let's just use the first functional part.
					# In a more advanced system, we'd show a list to the user.
					part_index = i
					break
		
		if part_index != -1:
			if clothing.equip_item(item_id, part_index):
				inventory.remove_item(item_id, 1)
				TimeManager.advance_turn()
				refresh_view()

func _drop_selected_item(all: bool):
	if _items_cache.is_empty() or selected_index < 0 or selected_index >= _items_cache.size():
		return
	
	var item_data = _items_cache[selected_index]
	var item_id = item_data["id"]
	var amount_to_remove = item_data["amount"] if all else 1
	
	if inventory.remove_item(item_id, amount_to_remove):
		InputManager.inventory_item_dropped.emit(item_id, amount_to_remove)
		TimeManager.advance_turn()
		refresh_view()
