class_name ListDetailTab extends BaseListTab

const SpacerLabelScene = preload("res://src/ui/spacer_label.tscn")

func _clear_detail_rows() -> void:
	for child in detailsContainer.get_children():
		child.queue_free()

func _add_detail(label: String, value: String) -> void:
	var inst = SpacerLabelScene.instantiate()
	detailsContainer.add_child(inst)
	inst.Label1.text = label
	inst.Label2.text = value

func _clear_details() -> void:
	titleLabel.text = ""
	descriptionLabel.text = ""
	_clear_detail_rows()
