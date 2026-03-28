extends BaseListTab

@export var world: WorldGeneration
@export var itemDetailsLabel: Label

var filter_pos: Vector2i = Vector2i(-1, -1)
var use_filter: bool = false

func refresh_view() -> void:
	super.refresh_view()

func _on_refresh() -> void:
	if itemDetailsLabel:
		itemDetailsLabel.visible = !_items_cache.is_empty()

func set_params(params: Dictionary) -> void:
	if params.has("filter_pos"):
		filter_pos = params["filter_pos"]
		use_filter = true
	else:
		use_filter = false
	
	refresh_view()

func _get_display_data() -> Array:
	var center = Vector2i(Player.cellPos)
	var items_map = {} # id -> {amount, positions: [ {pos, amount} ] }
	
	var tiles_to_scan = []
	if use_filter:
		tiles_to_scan.append(filter_pos)
	else:
		for x in range(-1, 2):
			for y in range(-1, 2):
				tiles_to_scan.append(center + Vector2i(x, y))
	
	for pos in tiles_to_scan:
		var raw_items = world.get_items_at(pos)
		for item in raw_items:
			var id = item["id"]
			if not items_map.has(id):
				items_map[id] = {
					"id": id,
					"amount": 0,
					"display_name": ItemDb.get_item_name(id),
					"description": ItemDb.get_item_description(id),
					"sources": [] # List of {pos, amount}
				}
			
			items_map[id]["amount"] += item["amount"]
			items_map[id]["sources"].append({"pos": pos, "amount": item["amount"]})
	
	var final_list = []
	for id in items_map:
		var data = items_map[id]
		data["quantity_text"] = "x" + str(data["amount"])
		final_list.append(data)
	
	return final_list

func handle_action(action_name: String, params: Dictionary = {}):
	if action_name == "drop": # Redirecting 'Q' (drop) to pickup in this tab
		_pickup_selected_item(params.get("all", false))

func _pickup_selected_item(all: bool):
	if _items_cache.is_empty() or selected_index < 0:
		return
	
	var item_data = _items_cache[selected_index]
	var item_id = item_data["id"]
	var total_to_pickup = item_data["amount"] if all else 1
	var picked_up_so_far = 0
	
	# Pick up from sources until we have enough
	for source in item_data["sources"]:
		var to_get = min(total_to_pickup - picked_up_so_far, source["amount"])
		if world.pickup_item_specific(source["pos"], item_id, to_get, Player._Inventory):
			picked_up_so_far += to_get
			TimeManager.advance_turn()

			if picked_up_so_far >= total_to_pickup:
				break
			
	if picked_up_so_far > 0:
		world.update_world_bubble(Player.cellPos)
		refresh_view()
