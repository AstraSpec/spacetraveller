extends Node
class_name DbAccess

const DIR_FILEPATH : String = "res://data/structures/"

static func save_structure(ID: String, RLE: Dictionary, filepath: String):
	var structures = _read_all(filepath)
	
	var found = false
	for i in range(structures.size()):
		if structures[i]["id"] == ID:
			structures[i] = _preserve_rule_metadata(structures[i], RLE)
			found = true
			break
	
	if not found:
		structures.append(RLE)
		
	_write_all(structures, filepath)
	StructureDb.initialize_data()

static func _preserve_rule_metadata(existing: Dictionary, replacement: Dictionary) -> Dictionary:
	var merged: Dictionary = replacement.duplicate(true)
	var existing_levels: Dictionary = _get_structure_levels(existing)
	var merged_levels: Dictionary = _get_structure_levels(merged)
	for key in existing_levels.keys():
		if !merged_levels.has(key):
			continue
		var existing_level: Dictionary = existing_levels[key]
		if !existing_level.has("rules"):
			continue
		var merged_level: Dictionary = merged_levels[key]
		if !merged_level.has("rules"):
			merged_level["rules"] = existing_level["rules"]
			merged_levels[key] = merged_level
	if !merged_levels.is_empty():
		merged["levels"] = merged_levels
	merged.erase("blueprint")
	merged.erase("palette")
	merged.erase("rules")
	return merged

static func _get_structure_levels(structure_data: Dictionary) -> Dictionary:
	var result: Dictionary = {}
	if structure_data.has("levels") and structure_data["levels"] is Dictionary:
		var raw_levels: Dictionary = structure_data["levels"]
		for key in raw_levels.keys():
			var level_data: Variant = raw_levels[key]
			if level_data is Dictionary:
				result[str(key)] = level_data.duplicate(true)
	elif structure_data.has("blueprint") or structure_data.has("palette") or structure_data.has("rules"):
		var level_zero: Dictionary = {}
		level_zero["blueprint"] = structure_data.get("blueprint", "")
		level_zero["palette"] = structure_data.get("palette", [])
		if structure_data.has("rules"):
			level_zero["rules"] = structure_data["rules"]
		result["0"] = level_zero
	return result

static func delete_structure(id: String, filepath: String = ""):
	if filepath == "":
		filepath = _find_file_with_id(id, DIR_FILEPATH)
		
	if filepath == "":
		printerr("DbAccess: Could not find file containing structure ID: ", id)
		return

	var structures = _read_all(filepath)
	var newStructures = []
	
	for s in structures:
		if s["id"] != id:
			newStructures.append(s)
			
	_write_all(newStructures, filepath)
	StructureDb.initialize_data()

static func _read_all(filepath: String) -> Array:
	if not FileAccess.file_exists(filepath):
		return []
		
	var file = FileAccess.open(filepath, FileAccess.READ)
	var json = JSON.new()
	if json.parse(file.get_as_text()) == OK:
		var data = json.get_data()
		if data is Array:
			return data
	return []

static func _write_all(data: Array, filepath: String):
	var file = FileAccess.open(filepath, FileAccess.WRITE)
	if file:
		file.store_string(JSON.stringify(data, "    "))
		file.close()

static func _find_file_with_id(id: String, path: String) -> String:
	var dir = DirAccess.open(path)
	if not dir: return ""
	
	dir.list_dir_begin()
	var fileName = dir.get_next()
	while fileName != "":
		var fullPath = path.path_join(fileName)
		if dir.current_is_dir():
			if not fileName.begin_with("."):
				var found = _find_file_with_id(id, fullPath)
				if found != "": return found
		elif fileName.ends_with(".json"):
			var structures = _read_all(fullPath)
			for s in structures:
				if s.get("id") == id:
					return fullPath
		fileName = dir.get_next()
	
	return ""
