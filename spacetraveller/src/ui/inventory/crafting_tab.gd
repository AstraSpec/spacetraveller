extends BaseListTab

@export var _GameWorld: GameWorld
@export var CraftButton: Button
@export var InsufficientLabel: Label

var crafting_context: Dictionary = {}

func set_params(params: Dictionary) -> void:
	crafting_context.clear()
	if params.has("station_pos"):
		crafting_context["station_pos"] = params["station_pos"]
	var window := get_window()
	if crafting_context.is_empty():
		window.title = "Inventory"
	else:
		var tile_id := str(params.get("station_id", ""))
		window.title = "Crafting — %s" % TileDb.get_tile_name(tile_id)
	refresh_view()

func on_menu_closed() -> void:
	crafting_context.clear()
	get_window().title = "Inventory"

func _get_display_data() -> Array:
	var formatted: Array = []
	for id in RecipeDb.get_ids():
		var recipe_id := str(id)
		var status: Dictionary = _GameWorld.get_player_crafting_status(recipe_id, crafting_context)
		if not crafting_context.is_empty() and not bool(status.get("station_relevant", false)):
			continue
		var craftable := bool(status.get("craftable", false))
		formatted.append({
			"id": recipe_id,
			"display_name": RecipeDb.get_recipe_name(recipe_id),
			"description": RecipeDb.get_recipe_description(recipe_id),
			"quantity_text": _format_crafting_time(RecipeDb.get_recipe_time(recipe_id)) if craftable else "",
			"craftable": craftable,
			"disabled": not craftable,
			"status": status,
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

func _update_details_ui(item_data: Dictionary) -> void:
	for child in detailsContainer.get_children():
		child.queue_free()

	var status: Dictionary = _GameWorld.get_player_crafting_status(
		str(item_data["id"]), crafting_context)
	_add_header("Components:")
	for component in status.get("components", []):
		var item_id := str(component["item_id"])
		var label := Label.new()
		label.text = "  - %s x%d (%d carried)" % [
			ItemDb.get_item_name(item_id),
			int(component["required"]),
			int(component["available"]),
		]
		label.modulate = Color(0x66ff66ff) if bool(component["satisfied"]) else Color(0xff6666ff)
		detailsContainer.add_child(label)

	var tools: Array = status.get("tools", [])
	if not tools.is_empty():
		_add_header("\nTools:")
		for tool in tools:
			var required := str(tool["required_rarity"]).capitalize()
			var best := str(tool["best_rarity"])
			var availability := best.capitalize() if not best.is_empty() else "None"
			var label := Label.new()
			label.text = "  - %s: %s (best: %s)" % [tool["quality_name"], required, availability]
			label.modulate = Color(0x66ff66ff) if bool(tool["satisfied"]) else Color(0xff6666ff)
			detailsContainer.add_child(label)

	_add_header("\nProduces:")
	for result in RecipeDb.get_recipe_results(str(item_data["id"])):
		var label := Label.new()
		label.text = "  - %s x%d" % [ItemDb.get_item_name(result["id"]), result["amount"]]
		detailsContainer.add_child(label)

	var craftable := bool(status.get("craftable", false))
	CraftButton.visible = craftable
	InsufficientLabel.visible = not craftable
	InsufficientLabel.text = "Missing crafting requirements"

func _add_header(text: String) -> void:
	var label := Label.new()
	label.text = text
	detailsContainer.add_child(label)

func _on_item_activated() -> void:
	if _items_cache.is_empty() or selected_index < 0 or selected_index >= _items_cache.size():
		return

	var recipe_id := str(_items_cache[selected_index]["id"])
	var status: Dictionary = _GameWorld.get_player_crafting_status(recipe_id, crafting_context)
	if not bool(status.get("craftable", false)):
		return
	_craft_recipe(recipe_id)

func _craft_recipe(recipe_id: String) -> void:
	if not _GameWorld.start_player_crafting(recipe_id, crafting_context):
		EventBus.post(
			"inventory_warning",
			"You cannot begin crafting %s." % RecipeDb.get_recipe_name(recipe_id),
			{"recipe_id": recipe_id}
		)

func _on_craft_button_pressed() -> void:
	_on_item_activated()
