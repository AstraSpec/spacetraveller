extends BaseListTab

@export var _GameWorld :GameWorld
@export var CraftButton :Button
@export var InsufficientLabel :Label

func _get_display_data() -> Array:
	var ids = RecipeDb.get_ids()
	var formatted = []
	
	for id in ids:
		formatted.append({
			"id": id,
			"display_name": RecipeDb.get_recipe_name(id),
			"description": RecipeDb.get_recipe_description(id),
			"quantity_text": ""
		})
	return formatted

func _on_refresh() -> void:
	pass

func _item_label(item_id: String) -> String:
	var item_name := String(ItemDb.get_item_name(item_id))
	return item_name if not item_name.is_empty() else item_id

func _format_item_amount(item_id: String, amount: int) -> String:
	var label = _item_label(item_id)
	if amount <= 1:
		return label
	return "%s x%d" % [label, amount]

func _format_result_list(results: Array) -> String:
	var labels: Array[String] = []
	for res in results:
		labels.append(_format_item_amount(res["id"], res["amount"]))
	return ", ".join(labels)

func _update_details_ui(item_data: Dictionary) -> void:
	if not detailsContainer: return
	
	for child in detailsContainer.get_children():
		child.queue_free()
		
	var recipe_id = item_data["id"]
	var reqs = RecipeDb.get_recipe_requirements(recipe_id)
	var results = RecipeDb.get_recipe_results(recipe_id)
	
	var req_header = Label.new()
	req_header.text = "Requirements:"
	detailsContainer.add_child(req_header)
	
	var can_craft = true
	for req in reqs:
		var label = Label.new()
		var has_item = _GameWorld.get_entity_inventory_item_amount(0, req["id"]) >= req["amount"]
		var item_name = ItemDb.get_item_name(req["id"])
		label.text = "  - %s x%d" % [item_name, req["amount"]]
		
		if not has_item:
			label.modulate = Color(0xff6666ff)
			can_craft = false
		else:
			label.modulate = Color(0x66ff66ff)
			
		detailsContainer.add_child(label)
	
	var res_header = Label.new()
	res_header.text = "\nProduces:"
	detailsContainer.add_child(res_header)
	
	for res in results:
		var label = Label.new()
		var item_name = ItemDb.get_item_name(res["id"])
		label.text = "  - %s x%d" % [item_name, res["amount"]]
		detailsContainer.add_child(label)

	if can_craft:
		CraftButton.visible = true
		InsufficientLabel.visible = false
	else:
		CraftButton.visible = false
		InsufficientLabel.visible = true

func _on_item_activated() -> void:
	if _items_cache.is_empty() or selected_index < 0:
		return
		
	var recipe_id = _items_cache[selected_index]["id"]
	_craft_recipe(recipe_id)

func _craft_recipe(recipe_id: String):
	var reqs = RecipeDb.get_recipe_requirements(recipe_id)
	var results = RecipeDb.get_recipe_results(recipe_id)
	var craft_time = RecipeDb.get_recipe_time(recipe_id)
	
	for req in reqs:
		if _GameWorld.get_entity_inventory_item_amount(0, req["id"]) < req["amount"]:
			return
			
	for req in reqs:
		_GameWorld.remove_entity_inventory_item(0, req["id"], req["amount"])
		
	var added_results: Array[Dictionary] = []
	for res in results:
		if _GameWorld.add_entity_inventory_item(0, res["id"], res["amount"]):
			added_results.append({"id": res["id"], "amount": res["amount"]})
		else:
			for added in added_results:
				_GameWorld.remove_entity_inventory_item(0, added["id"], added["amount"])
			for req in reqs:
				_GameWorld.add_entity_inventory_item(0, req["id"], req["amount"])
			EventBus.post("inventory_warning", "You cannot craft %s. Carry weight is over limit." % RecipeDb.get_recipe_name(recipe_id), {"recipe_id": recipe_id})
			refresh_view()
			return
		
	var turns_to_advance = max(1, int(craft_time))
	TimeManager.advance_turn(turns_to_advance)
	EventBus.post("inventory", "You craft %s." % _format_result_list(results), {"recipe_id": recipe_id})
	
	refresh_view()

func _on_craft_button_pressed() -> void:
	_on_item_activated()
