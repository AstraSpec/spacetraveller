extends PanelContainer

@export var LogText :RichTextLabel
@export var SearchBar :LineEdit

var _all_entries: Array[String] = []

const CATEGORY_COLORS = {
	"combat_player": "55ff55",
	"combat_enemy": "ff5555",
	"ground": "aaaaaa",
	"smash": "d8a04a",
	"effect": "c77dff",
	"interact": "5bc0eb",
}
const DEFAULT_COLOR = "cccccc"

func _ready() -> void:
	EventBus.event_posted.connect(_on_event_posted)

func _on_event_posted(category: String, message: String, _metadata: Dictionary) -> void:
	var color = CATEGORY_COLORS.get(category, DEFAULT_COLOR)
	var entry = "[color=#%s]%s[/color]" % [color, message]
	_all_entries.append(entry)
	_apply_filter()

func add_entry(text: String) -> void:
	_all_entries.append(text)
	_apply_filter()

func _apply_filter() -> void:
	if not LogText:
		return
	var filter = SearchBar.text.to_lower() if SearchBar else ""
	LogText.clear()
	for entry in _all_entries:
		if filter.is_empty() or entry.to_lower().contains(filter):
			LogText.append_text(entry + "\n")

func _on_search_bar_text_changed(_new_text: String) -> void:
	_apply_filter()
