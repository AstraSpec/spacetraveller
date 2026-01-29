extends BaseListTab

@export var inventory: Inventory
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

func _update_details_ui(item_data: Dictionary) -> void:
	if not detailsContainer: return
	
	# Clear old children
	for child in detailsContainer.get_children():
		child.queue_free()
		
	var recipe_id = item_data["id"]
	var reqs = RecipeDb.get_recipe_requirements(recipe_id)
	var results = RecipeDb.get_recipe_results(recipe_id)
	
	# Show Requirements
	var req_header = Label.new()
	req_header.text = "Requirements:"
	detailsContainer.add_child(req_header)
	
	var can_craft = true
	for req in reqs:
		var label = Label.new()
		var has_item = inventory.has_item(req["id"], req["amount"]) if inventory else false
		var item_name = ItemDb.get_item_name(req["id"])
		label.text = "  - %s x%d" % [item_name, req["amount"]]
		
		if not has_item:
			label.modulate = Color(0xff6666ff)
			can_craft = false
		else:
			label.modulate = Color(0x66ff66ff)
			
		detailsContainer.add_child(label)
	
	# Show Results
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
	if _items_cache.is_empty() or selected_index < 0 or selected_index >= _items_cache.size():
		return
		
	var recipe_id = _items_cache[selected_index]["id"]
	_craft_recipe(recipe_id)

func _craft_recipe(recipe_id: String):
	if not inventory: return
	
	var reqs = RecipeDb.get_recipe_requirements(recipe_id)
	var results = RecipeDb.get_recipe_results(recipe_id)
	
	# Final check
	for req in reqs:
		if not inventory.has_item(req["id"], req["amount"]):
			return
			
	# Consume
	for req in reqs:
		inventory.remove_item(req["id"], req["amount"])
		
	# Produce
	for res in results:
		inventory.add_item(res["id"], res["amount"])
		
	# Feedback
	refresh_view()

func _on_craft_button_pressed() -> void:
	_on_item_activated()
