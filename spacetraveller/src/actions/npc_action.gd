extends RefCounted
class_name NpcAction

var id: String
var display_name: String
var description: String
var kind: String
var payload: Dictionary
var disabled: bool = false

func _init(
	p_id: String,
	p_display_name: String,
	p_description: String,
	p_kind: String,
	p_payload: Dictionary = {},
	p_disabled: bool = false
) -> void:
	id = p_id
	display_name = p_display_name
	description = p_description
	kind = p_kind
	payload = p_payload.duplicate(true)
	disabled = p_disabled

func to_display_data() -> Dictionary:
	return {
		"id": id,
		"kind": kind,
		"display_name": display_name,
		"description": description,
		"payload": payload.duplicate(true),
		"disabled": disabled
	}
