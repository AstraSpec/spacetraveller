extends ListDetailTab

signal scenario_selected(scenario_id: String)

func _get_display_data() -> Array:
	var ids: Array = ScenarioDb.get_ids()
	if ids.is_empty():
		ScenarioDb.initialize_data()
		ids = ScenarioDb.get_ids()

	var scenarios: Array = []
	for id_value in ids:
		var scenario_id := str(id_value)
		if scenario_id.is_empty():
			continue
		scenarios.append({
			"scenario_id": scenario_id,
			"display_name": ScenarioDb.get_display_name(scenario_id),
			"description": ScenarioDb.get_description(scenario_id),
			"location": ScenarioDb.get_location(scenario_id),
			"items": ScenarioDb.get_items(scenario_id),
			"equipment": ScenarioDb.get_equipment(scenario_id),
		})
	return scenarios

func _update_details_ui(item_data: Dictionary) -> void:
	_clear_detail_rows()

	var scenario_id := str(item_data.get("scenario_id", ""))
	scenario_selected.emit(scenario_id)

	var location: Dictionary = item_data.get("location", {})
	var items: Array = item_data.get("items", [])
	var equipment: Array = item_data.get("equipment", [])

	_add_detail("Location", _get_location_display_name(location))

	if items.is_empty():
		_add_detail("Items", "None")
	else:
		_add_detail("Items", "%d entries" % items.size())

	if equipment.is_empty():
		_add_detail("Equipment", "None")
	else:
		_add_detail("Equipment", "%d entries" % equipment.size())

func _on_refresh() -> void:
	if _items_cache.is_empty():
		_clear_details()

func get_selected_scenario_id() -> String:
	if _items_cache.is_empty():
		refresh_view()
	if selected_index < 0 or selected_index >= _items_cache.size():
		return ""
	return str(_items_cache[selected_index].get("scenario_id", ""))

func _get_location_display_name(location: Dictionary) -> String:
	var display_name := str(location.get("display_name", ""))
	if not display_name.is_empty():
		return display_name
	return "Unknown"
