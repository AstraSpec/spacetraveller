extends Node

signal models_fetched(models: Array)
signal models_failed(reason: String)
signal generation_completed(text: String, tag: String)
signal generation_failed(reason: String, tag: String)

const PROVIDER_LOCAL := "local"
const PROVIDER_CLOUD := "cloud"

# Local (Ollama) defaults.
const DEFAULT_URL := "http://127.0.0.1:11434"
const DEFAULT_MODEL := "llama3.2:3b-instruct-q4_K_M"

# Cloud (Groq) defaults.
const GROQ_URL := "https://api.groq.com/openai/v1/chat/completions"
const DEFAULT_CLOUD_MODEL := "llama-3.3-70b-versatile"

# Curated free-tier Groq models
const CLOUD_MODELS := [
	"llama-3.3-70b-versatile",
	"llama-3.1-8b-instant",
	"meta-llama/llama-4-scout-17b-16e-instruct",
	"openai/gpt-oss-20b",
	"qwen/qwen3-32b",
]

const NUM_CTX := 4096
const TEMPERATURE := 0.75

const MAX_ATTEMPTS := 3

func is_ready() -> bool:
	return SettingsManager.get_setting("AI", "configured", false)

func get_provider() -> String:
	return SettingsManager.get_setting("AI", "provider", PROVIDER_LOCAL)

func get_server_url() -> String:
	return SettingsManager.get_setting("AI", "server_url", DEFAULT_URL)

func get_api_key() -> String:
	return SettingsManager.get_setting("AI", "api_key", "")

func get_model() -> String:
	if get_provider() == PROVIDER_CLOUD:
		return SettingsManager.get_setting("AI", "cloud_model", DEFAULT_CLOUD_MODEL)
	return SettingsManager.get_setting("AI", "model", DEFAULT_MODEL)

var _messages: Array = []
var _tag: String = ""
var _min_segments: int = 1
var _attempt: int = 0

func fetch_models() -> void:
	var http := HTTPRequest.new()
	add_child(http)
	http.timeout = 5.0
	http.request_completed.connect(_on_models_done.bind(http))
	var err := http.request(get_server_url() + "/api/tags", [], HTTPClient.METHOD_GET)
	if err != OK:
		http.queue_free()
		models_failed.emit("Could not reach %s" % get_server_url())

func _on_models_done(result: int, code: int, _headers: PackedStringArray, body: PackedByteArray, http: HTTPRequest) -> void:
	http.queue_free()
	if result != HTTPRequest.RESULT_SUCCESS or code != 200:
		models_failed.emit("Ollama not reachable (is it running?)")
		return
	var parsed = JSON.parse_string(body.get_string_from_utf8())
	if not parsed is Dictionary or not parsed.has("models"):
		models_failed.emit("Unexpected response from server")
		return
	var names: Array = []
	for m in parsed["models"]:
		if m is Dictionary and m.has("name"):
			names.append(m["name"])
	models_fetched.emit(names)

func generate(messages: Array, tag: String, min_segments: int = 1) -> void:
	_messages = messages
	_tag = tag
	_min_segments = max(1, min_segments)
	_attempt = 0
	_send()

func _send() -> void:
	_attempt += 1
	var req := _build_request()
	if req.is_empty():
		generation_failed.emit("Cloud provider needs an API key (check AI setup)", _tag)
		return
	var http := HTTPRequest.new()
	http.use_threads = true
	add_child(http)
	http.timeout = 60.0
	http.set_meta("t_start_us", Time.get_ticks_usec())
	http.request_completed.connect(_on_done.bind(http))
	print("[ConversationService] POST provider=", get_provider(), " tag=", _tag, " attempt=", _attempt, " min_segments=", _min_segments)
	var err := http.request(req["url"], req["headers"], HTTPClient.METHOD_POST, req["body"])
	if err != OK:
		print("[ConversationService] request() error: ", err)
		http.queue_free()
		generation_failed.emit("Could not reach the AI server", _tag)

func _build_request() -> Dictionary:
	var headers := ["Content-Type: application/json"]
	if get_provider() == PROVIDER_CLOUD:
		var key := get_api_key()
		if key.is_empty():
			return {}
		headers.append("Authorization: Bearer %s" % key)
		var payload := {
			"model": get_model(),
			"messages": _messages,
			"stream": false,
			"temperature": TEMPERATURE
		}
		return { "url": GROQ_URL, "headers": headers, "body": JSON.stringify(payload) }
	
	var local_payload := {
		"model": get_model(),
		"messages": _messages,
		"stream": false,
		"options": {
			"num_ctx": NUM_CTX,
			"temperature": TEMPERATURE
		}
	}
	return { "url": get_server_url() + "/api/chat", "headers": headers, "body": JSON.stringify(local_payload) }

func _on_done(result: int, code: int, _headers: PackedStringArray, body: PackedByteArray, http: HTTPRequest) -> void:
	var wall_ms := 0.0
	if http.has_meta("t_start_us"):
		wall_ms = (Time.get_ticks_usec() - http.get_meta("t_start_us")) / 1000.0
	http.queue_free()
	print("[ConversationService] done tag=", _tag, " result=", result, " code=", code, " wall=", wall_ms, "ms")
	if result != HTTPRequest.RESULT_SUCCESS or code != 200:
		print("[ConversationService] body: ", body.get_string_from_utf8())
		generation_failed.emit(_http_error_reason(code), _tag)
		return
	var envelope = JSON.parse_string(body.get_string_from_utf8())
	if not envelope is Dictionary:
		print("[ConversationService] bad envelope: ", body.get_string_from_utf8())
		generation_failed.emit("Malformed response from server", _tag)
		return
	_print_timings(envelope, wall_ms)
	var content := _extract_content(envelope)
	if content.is_empty():
		print("[ConversationService] no content in: ", body.get_string_from_utf8())
		generation_failed.emit("Malformed response from server", _tag)
		return
	print("[ConversationService] model output (", _tag, "): ", content)
	if _usable_segments(content) < _min_segments:
		print("[ConversationService] insufficient segments (attempt ", _attempt, "/", MAX_ATTEMPTS, ")")
		if _attempt < MAX_ATTEMPTS:
			print("[ConversationService] retrying...")
			_send()
			return
		generation_failed.emit("Model did not return a usable reply", _tag)
		return
	generation_completed.emit(content, _tag)


func _extract_content(envelope: Dictionary) -> String:
	if envelope.has("message"):
		return str(envelope["message"].get("content", "")).strip_edges()
	if envelope.has("choices"):
		var choices = envelope["choices"]
		if choices is Array and choices.size() > 0 and choices[0] is Dictionary:
			var msg = choices[0].get("message", {})
			if msg is Dictionary:
				return str(msg.get("content", "")).strip_edges()
	return ""

func _http_error_reason(code: int) -> String:
	if get_provider() == PROVIDER_CLOUD:
		match code:
			401: return "Cloud key rejected (check your API key)"
			429: return "Rate limit reached, wait a moment"
			_: return "Cloud request failed (code %d)" % code
	return "No response from model (check AI setup)"

func _usable_segments(text: String) -> int:
	var parts := text.split("|", false)
	var count := 0
	for p in parts:
		if not str(p).strip_edges().is_empty():
			count += 1
	return count

func _print_timings(envelope: Dictionary, wall_ms: float) -> void:
	var to_ms := func(ns): return float(ns) / 1_000_000.0
	var total_ms: float = to_ms.call(envelope.get("total_duration", 0))
	var load_ms: float = to_ms.call(envelope.get("load_duration", 0))
	var prompt_ms: float = to_ms.call(envelope.get("prompt_eval_duration", 0))
	var gen_ms: float = to_ms.call(envelope.get("eval_duration", 0))
	var prompt_tokens: int = int(envelope.get("prompt_eval_count", 0))
	var gen_tokens: int = int(envelope.get("eval_count", 0))
	var gen_tps := (gen_tokens / (gen_ms / 1000.0)) if gen_ms > 0.0 else 0.0
	var unaccounted_ms := wall_ms - total_ms
	print("[ConversationService] timings ms: wall=", wall_ms,
		" ollama_total=", total_ms,
		" load=", load_ms,
		" prompt_eval=", prompt_ms, " (", prompt_tokens, " tok)",
		" generation=", gen_ms, " (", gen_tokens, " tok, ", gen_tps, " tok/s)",
		" unaccounted=", unaccounted_ms)
