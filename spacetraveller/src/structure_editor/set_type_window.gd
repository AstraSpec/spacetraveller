extends Window

@export var structureEditor :Node2D
@export var TypePicker :OptionButton
@export var SizeLabel :Label
@export var WidthText :SpinBox
@export var HeightText :SpinBox

var CHUNK_SIZE :int = GameWorld.get_chunk_size()
const MAX_STRUCTURE_CELLS :int = 1024 * 1024

func _ready() -> void:
	hide()
	TypePicker.clear()
	TypePicker.add_item("Structure")
	TypePicker.add_item("Feature")
	if !TypePicker.item_selected.is_connected(_on_type_selected):
		TypePicker.item_selected.connect(_on_type_selected)
	_configure_inputs()

func open() -> void:
	var feature_selected = structureEditor.is_feature_structure()
	TypePicker.select(1 if feature_selected else 0)
	_set_mode(feature_selected, false)
	show()

func _configure_inputs() -> void:
	for input in [WidthText, HeightText]:
		input.min_value = 1
		input.step = 1
		input.rounded = true
		input.allow_greater = false
		input.allow_lesser = false

func _on_type_selected(index: int) -> void:
	_set_mode(index == 1, true)

func _set_mode(feature_selected: bool, use_defaults: bool) -> void:
	if feature_selected:
		SizeLabel.text = "Size (cells)"
		WidthText.max_value = MAX_STRUCTURE_CELLS
		HeightText.max_value = MAX_STRUCTURE_CELLS
		if use_defaults:
			WidthText.value = CHUNK_SIZE
			HeightText.value = CHUNK_SIZE
		else:
			WidthText.value = clampi(structureEditor.structure_size.x, 1, MAX_STRUCTURE_CELLS)
			HeightText.value = clampi(structureEditor.structure_size.y, 1, MAX_STRUCTURE_CELLS)
	else:
		SizeLabel.text = "Size (chunks)"
		WidthText.max_value = int(MAX_STRUCTURE_CELLS / CHUNK_SIZE)
		HeightText.max_value = int(MAX_STRUCTURE_CELLS / CHUNK_SIZE)
		if use_defaults:
			WidthText.value = 1
			HeightText.value = 1
		else:
			WidthText.value = ceili(float(structureEditor.editor_area_size.x) / CHUNK_SIZE)
			HeightText.value = ceili(float(structureEditor.editor_area_size.y) / CHUNK_SIZE)

func _on_apply_pressed() -> void:
	var feature_selected := TypePicker.selected == 1
	var _size := Vector2i(int(WidthText.value), int(HeightText.value))
	if !feature_selected:
		_size *= CHUNK_SIZE
	structureEditor.set_structure_type_mode(feature_selected, _size)
	hide()

func _on_cancel_pressed() -> void:
	hide()

func _on_close_requested() -> void:
	hide()
