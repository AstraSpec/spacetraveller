extends Node

signal event_posted(category: String, message: String, metadata: Dictionary)

func post(category: String, message: String, metadata: Dictionary = {}) -> void:
	event_posted.emit(category, message, metadata)
