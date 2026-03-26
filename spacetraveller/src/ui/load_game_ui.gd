extends BaseWindow

@export var LoadContainer: VBoxContainer
@export var LoadButton: Button
@export var DeleteButton: Button

@onready var SaveButtonScene = preload("res://src/structure_editor/structure_button.tscn")

var selectedID: String = ""

func _ready() -> void:
	super._ready()
	visible = false
	InputManager.menu_toggled.connect(_on_menu_toggled)
	_update_buttons()

func _on_menu_toggled(id: String, is_open: bool, _params: Dictionary) -> void:
	if id == "load_game":
		if is_open:
			open()
		else:
			visible = false

func open() -> void:
	visible = true
	selectedID = ""
	_update_buttons()
	
	for child in LoadContainer.get_children():
		child.queue_free()
		
	var saves = SaveManager.get_save_list()
	for save_id in saves:
		var instance = SaveButtonScene.instantiate()
		LoadContainer.add_child(instance)
		
		instance.text = save_id
		instance.pressed.connect(_on_save_selected.bind(save_id))
		
		instance.gui_input.connect(func(event):
			if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT and event.double_click:
				_on_save_selected(save_id)
				_on_load_pressed()
		)

func _on_save_selected(id: String) -> void:
	selectedID = id
	_update_buttons()

func _update_buttons() -> void:
	var hasSelection = selectedID != ""
	LoadButton.disabled = !hasSelection
	DeleteButton.disabled = !hasSelection

func _on_load_pressed() -> void:
	if selectedID == "": return
	SaveManager.load_game(selectedID)
	InputManager.set_mode(InputManager.InputMode.EXPLORATION)
	visible = false

func _on_delete_pressed() -> void:
	if selectedID == "": return
	SaveManager.delete_save(selectedID)
	open()

func _on_close_pressed() -> void:
	InputManager.set_mode(InputManager.InputMode.EXPLORATION)
	visible = false

func _on_close_requested() -> void:
	super._on_close_requested()
	visible = false
