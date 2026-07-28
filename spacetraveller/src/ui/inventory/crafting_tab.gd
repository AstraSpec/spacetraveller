extends BaseListTab

@export var _GameWorld :GameWorld
@export var CraftButton :Button
@export var InsufficientLabel :Label

func _get_display_data() -> Array:
	var ids = RecipeDb.get_ids()
	var formatted = []
	
	for id in ids:
		var recipe_id := str(id)
		var craftable := _can_craft_recipe(recipe_id)
		formatted.append({
			"id": recipe_id,
			"display_name": RecipeDb.get_recipe_name(recipe_id),
			"description": RecipeDb.get_recipe_description(recipe_id),
			"quantity_text": _format_crafting_time(RecipeDb.get_recipe_time(recipe_id)) if craftable else "",
			"craftable": craftable,
			"disabled": not craftable,
		})

	formatted.sort_custom(
		func(a: Dictionary, b: Dictionary) -> bool:
			var a_craftable := bool(a.get("craftable", false))
			var b_craftable := bool(b.get("craftable", false))
			if a_craftable != b_craftable:
				return a_craftable
			var a_name := str(a.get("display_name", "")).to_lower()
			var b_name := str(b.get("display_name", "")).to_lower()
			if a_name == b_name:
				return str(a.get("id", "")) < str(b.get("id", ""))
			return a_name < b_name
	)
	return formatted

func _can_craft_recipe(recipe_id: String) -> bool:
	var required: Dictionary = {}
	for requirement in RecipeDb.get_recipe_requirements(recipe_id):
		if not requirement is Dictionary:
			return false
		var item_id := str(requirement.get("id", ""))
		var amount := int(requirement.get("amount", 0))
		if item_id.is_empty() or amount <= 0:
			return false
		required[item_id] = int(required.get(item_id, 0)) + amount

	for item_id in required:
		if _GameWorld.get_entity_inventory_item_amount(0, str(item_id)) < int(required[item_id]):
			return false
	return true

func _format_crafting_time(seconds: float) -> String:
	var total_seconds := maxi(0, ceili(seconds))
	var hours := total_seconds / 3600
	var minutes := (total_seconds % 3600) / 60
	var remaining_seconds := total_seconds % 60
	var parts: Array[String] = []
	if hours > 0:
		parts.append("%dh" % hours)
	if minutes > 0:
		parts.append("%dm" % minutes)
	if remaining_seconds > 0 or parts.is_empty():
		parts.append("%ds" % remaining_seconds)
	return " ".join(parts)

func _on_refresh() -> void:
	pass

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
	
	var can_craft := _can_craft_recipe(recipe_id)
	for req in reqs:
		var label = Label.new()
		var has_item = _GameWorld.get_entity_inventory_item_amount(0, req["id"]) >= req["amount"]
		var item_name = ItemDb.get_item_name(req["id"])
		label.text = "  - %s x%d" % [item_name, req["amount"]]
		
		if not has_item:
			label.modulate = Color(0xff6666ff)
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
	if _items_cache.is_empty() or selected_index < 0 or selected_index >= _items_cache.size():
		return

	var item_data: Dictionary = _items_cache[selected_index]
	var recipe_id := str(item_data.get("id", ""))
	if bool(item_data.get("disabled", false)) or not _can_craft_recipe(recipe_id):
		return
	_craft_recipe(recipe_id)

func _craft_recipe(recipe_id: String):
	if not _GameWorld.start_player_crafting(recipe_id):
		EventBus.post(
			"inventory_warning",
			"You cannot begin crafting %s." % RecipeDb.get_recipe_name(recipe_id),
			{"recipe_id": recipe_id}
		)

func _on_craft_button_pressed() -> void:
	_on_item_activated()
