extends BaseListTab

@onready var SpacerLabelScene = preload("res://src/ui/spacer_label.tscn")

@export var inventory: Inventory
@export var weightVolumeLabel: Label

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

func refresh_view() -> void:
	super.refresh_view()
	_update_totals()

func _update_totals() -> void:
	if weightVolumeLabel and inventory:
		var total_weight = inventory.get_total_weight()
		var total_volume = inventory.get_total_volume()
		weightVolumeLabel.text = "Weight: %.1f\nVolume: %.1f" % [total_weight, total_volume]

func handle_action(action_name: String, params: Dictionary = {}):
	if action_name == "drop":
		_drop_selected_item(params.get("all", false))

func _drop_selected_item(all: bool):
	if _items_cache.is_empty() or selected_index < 0 or selected_index >= _items_cache.size():
		return
	
	var item_data = _items_cache[selected_index]
	var item_id = item_data["id"]
	var amount_to_remove = item_data["amount"] if all else 1
	
	if inventory.remove_item(item_id, amount_to_remove):
		InputManager.inventory_item_dropped.emit(item_id, amount_to_remove)
		refresh_view()
