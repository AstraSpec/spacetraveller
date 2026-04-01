extends BaseWindow
class_name CreditsWindow

@onready var rich_label : RichTextLabel = $Panel/VBoxContainer/MarginContainer/RichTextLabel

func _ready() -> void:
	super._ready()
	visible = false
	InputManager.menu_toggled.connect(_on_menu_toggled)
	
	if rich_label:
		rich_label.meta_clicked.connect(_on_meta_clicked)

func _on_meta_clicked(meta: Variant) -> void:
	OS.shell_open(str(meta))

func _on_menu_toggled(id: String, is_open: bool, _params: Dictionary) -> void:
	if id == "credits":
		if is_open:
			open()
		else:
			visible = false

func open() -> void:
	call_deferred("set_visible", true)
