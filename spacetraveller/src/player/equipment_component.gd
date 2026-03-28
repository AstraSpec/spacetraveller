extends Node
class_name EquipmentComponent

var Player: Sprite2D

var _Inventory :Inventory
var _Anatomy :Anatomy
var _Clothing :Clothing

func _ready() -> void:
	Player = get_parent()
	_Inventory = Player._Inventory
	_Anatomy = Player._Anatomy
	_Clothing = Player._Clothing

func equip_item(item_id: String) -> bool:
	if not ItemDb.has_tag(item_id, "WEARABLE"):
		return false
		
	var clothing_data = ItemDb.get_clothing_data(item_id)
	var part_type = clothing_data.get("part", "")
	
	var part_index = -1
	for i in range(_Anatomy.get_part_count()):
		if _Anatomy.get_part_type_id(i) == part_type or part_type == "":
			if _Anatomy.is_part_functional(i):
				part_index = i
				break
	
	if part_index != -1:
		if _Clothing.equip_item(item_id, part_index):
			_Inventory.remove_item(item_id, 1)
			TimeManager.advance_turn()
			return true
	return false

func unequip_item(item_id: String) -> bool:
	if _Inventory.add_item(item_id, 1):
		_Clothing.unequip_item(item_id)
		TimeManager.advance_turn()
		return true
	return false
