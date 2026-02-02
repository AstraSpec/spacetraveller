extends MarginContainer

@onready var SpacerLabelScene = preload("res://src/ui/spacer_label.tscn")
@export var container :VBoxContainer

const STATS = [
	"Strength",
	"Dexterity",
	"Constitution",
	"Intelligence",
	"Wisdom",
	"Charisma"
]

const PLACEHOLDER_VALUE = 10

func _ready() -> void:
	for stat_name in STATS:
		var row = SpacerLabelScene.instantiate()
		row.Label1.text = stat_name
		row.Label2.text = str(PLACEHOLDER_VALUE)
		container.add_child(row)
