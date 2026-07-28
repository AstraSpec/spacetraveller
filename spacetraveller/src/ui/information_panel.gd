extends CanvasLayer

@onready var _banner: PanelContainer = $TopCenter/Banner
@onready var _message: RichTextLabel = $TopCenter/Banner/MarginContainer/Message

var _owner_id: String = ""

func _ready() -> void:
	_banner.visible = false

func show_info(text: String, owner_id: String = "default") -> void:
	var normalized_text := text.strip_edges()
	var normalized_owner := owner_id if not owner_id.is_empty() else "default"
	if normalized_text.is_empty():
		hide_info(normalized_owner)
		return

	_owner_id = normalized_owner
	_message.text = normalized_text
	_banner.visible = true

func hide_info(owner_id: String = "") -> void:
	if not owner_id.is_empty() and owner_id != _owner_id:
		return
	_banner.visible = false
	_message.text = ""
	_owner_id = ""

func is_showing(owner_id: String = "") -> bool:
	if not _banner.visible:
		return false
	return owner_id.is_empty() or owner_id == _owner_id
