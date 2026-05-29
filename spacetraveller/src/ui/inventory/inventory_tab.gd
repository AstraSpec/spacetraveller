extends BaseListTab

@onready var SpacerLabelScene = preload("res://src/ui/spacer_label.tscn")

@export var _GameWorld :GameWorld
@export var weightVolumeLabel: RichTextLabel

const MODIFIER_NAMES = {
	"weight": "Weight",
	"volume": "Volume"
}

func _get_display_data() -> Array:
	var inv = _GameWorld.get_entity_inventory(0)
	var items = inv.get("items", {})
	var formatted = []
	
	var keys = items.keys()
	for item_id in keys:
		var amount = items[item_id]
		formatted.append({
			"id": item_id,
			"amount": amount,
			"display_name": ItemDb.get_item_name(item_id),
			"description": ItemDb.get_item_description(item_id),
			"quantity_text": "x" + str(amount),
			"type": ItemDb.get_item_type(item_id)
		})
	return formatted

func _update_details_ui(item_data: Dictionary) -> void:
	if not detailsContainer: return
	
	for child in detailsContainer.get_children():
		child.queue_free()
		
	var item_id = item_data.get("id", "")
	if item_id == "": return
	
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

func _wear_selected_item():
	if _items_cache.is_empty() or selected_index < 0:
		return
	
	var item_id = _items_cache[selected_index]["id"]
	if _GameWorld.equip_entity_clothing_by_string(0, item_id):
		refresh_view()

func _drop_selected_item(all: bool):
	if _items_cache.is_empty() or selected_index < 0:
		return
	
	var item_data = _items_cache[selected_index]
	var item_id = item_data["id"]
	var amount_to_remove = item_data["amount"] if all else 1
	
	if _GameWorld.remove_entity_inventory_item(0, item_id, amount_to_remove):
		InputManager.inventory_item_dropped.emit(item_id, amount_to_remove)
		TimeManager.advance_turn()
		refresh_view()
