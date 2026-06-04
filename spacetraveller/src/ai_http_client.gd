class_name AiHttpClient extends Node

signal text_received(content: String)
signal failed(reason: String)

func post(url: String, headers: PackedStringArray, body: String) -> Error:
	var http := HTTPRequest.new()
	http.use_threads = true
	add_child(http)
	http.timeout = 60.0
	http.request_completed.connect(_on_done.bind(http))
	var err := http.request(url, headers, HTTPClient.METHOD_POST, body)
	if err != OK:
		http.queue_free()
	return err

func _on_done(result: int, code: int, _hdrs: PackedStringArray, body: PackedByteArray, http: HTTPRequest) -> void:
	http.queue_free()
	if result != HTTPRequest.RESULT_SUCCESS or code != 200:
		failed.emit("Server returned error (code %d)" % code)
		return
	text_received.emit(body.get_string_from_utf8())
