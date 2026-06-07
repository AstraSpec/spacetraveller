extends Node
class_name InputSettings

# Registry of bindable actions and their display names
# Organized by category/group

const BINDABLE_ACTIONS = {
	"Movement": [
		{"id": "up", "name": "Move Up"},
		{"id": "down", "name": "Move Down"},
		{"id": "left", "name": "Move Left"},
		{"id": "right", "name": "Move Right"}
	],
	"Actions": [
		{"id": "action_smash", "name": "Smash / Interact"},
		{"id": "action_pickup", "name": "Pick Up"},
		{"id": "open_inventory", "name": "Open Inventory"},
		{"id": "wield_item", "name": "Wield Item"},
		{"id": "open_quests", "name": "Open Quests"},
		{"id": "open_map", "name": "Open Map"},
		{"id": "open_structure_mode", "name": "Structure Mode"}
	],
	"Structure Editor": [
		{"id": "structure_undo", "name": "Undo"},
		{"id": "structure_redo", "name": "Redo"},
		{"id": "delete", "name": "Delete Selected"}
	]
}

static func get_action_display_name(action_id: String) -> String:
	for group in BINDABLE_ACTIONS:
		for action in BINDABLE_ACTIONS[group]:
			if action.id == action_id:
				return action.name
	return action_id.capitalize()

static func get_bound_key_text(action_id: String) -> String:
	var events = InputMap.action_get_events(action_id)
	if events.is_empty():
		return "None"
	
	# Prefer keyboard keys for display in this menu
	for event in events:
		if event is InputEventKey:
			return event.as_text().replace(" (Physical)", "")
	
	# Fallback to first event
	return events[0].as_text().replace(" (Physical)", "")

static func get_full_list() -> Array:
	var list = []
	for group in BINDABLE_ACTIONS:
		list.append({"separator": group})
		for action in BINDABLE_ACTIONS[group]:
			list.append({
				"id": action.id,
				"display_name": action.name,
				"left": action.name,
				"right": get_bound_key_text(action.id)
			})
	return list
