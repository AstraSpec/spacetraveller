extends Window
class_name AiSetupWindow

@export var provider_dropdown: OptionButton
@export var url_edit: LineEdit
@export var key_edit: LineEdit
@export var model_dropdown: OptionButton
@export var refresh_button: Button
@export var status_label: Label
@export var save_button: Button
@export var cancel_button: Button

const PROVIDERS := [
	{ "id": ConversationService.PROVIDER_LOCAL, "label": "Local (Ollama)" },
	{ "id": ConversationService.PROVIDER_CLOUD, "label": "Cloud (Groq)" },
]

func _ready() -> void:
	visible = false
	InputManager.menu_toggled.connect(_on_menu_toggled)
	close_requested.connect(_on_close_requested)
	if refresh_button:
		refresh_button.pressed.connect(_on_refresh_pressed)
	if save_button:
		save_button.pressed.connect(_on_save_pressed)
	if cancel_button:
		cancel_button.pressed.connect(_on_close_requested)
	if provider_dropdown:
		provider_dropdown.clear()
		for p in PROVIDERS:
			provider_dropdown.add_item(p["label"])
		provider_dropdown.item_selected.connect(_on_provider_changed)
	ConversationService.models_fetched.connect(_on_models_fetched)
	ConversationService.models_failed.connect(_on_models_failed)

func _on_menu_toggled(id: String, is_open: bool, _params: Dictionary) -> void:
	if id != "ai_setup":
		return
	if is_open:
		_open()
	else:
		visible = false

func _open() -> void:
	var provider := ConversationService.get_provider()
	if provider_dropdown:
		for i in range(PROVIDERS.size()):
			if PROVIDERS[i]["id"] == provider:
				provider_dropdown.select(i)
				break
	if url_edit:
		url_edit.text = ConversationService.get_server_url()
	if key_edit:
		key_edit.text = ConversationService.get_api_key()
	_apply_provider_ui(provider)
	call_deferred("set_visible", true)

func _selected_provider() -> String:
	if provider_dropdown and provider_dropdown.selected >= 0:
		return PROVIDERS[provider_dropdown.selected]["id"]
	return ConversationService.PROVIDER_LOCAL

func _on_provider_changed(_index: int) -> void:
	_apply_provider_ui(_selected_provider())

func _apply_provider_ui(provider: String) -> void:
	var is_cloud := provider == ConversationService.PROVIDER_CLOUD
	if url_edit:
		url_edit.get_parent().visible = not is_cloud
	if key_edit:
		key_edit.get_parent().visible = is_cloud
	if refresh_button:
		refresh_button.visible = not is_cloud
	if is_cloud:
		_populate_dropdown(ConversationService.CLOUD_MODELS)
		if status_label:
			status_label.text = "Get a free API key at console.groq.com, paste it above, pick a model, and Save."
	else:
		_populate_dropdown([ConversationService.get_model()])
		if status_label:
			status_label.text = "Requires Ollama. Press Refresh to list installed models."
		_on_refresh_pressed()

func _on_refresh_pressed() -> void:
	if _selected_provider() == ConversationService.PROVIDER_CLOUD:
		return
	if status_label:
		status_label.text = "Connecting to server..."
	ConversationService.fetch_models()

func _on_models_fetched(models: Array) -> void:
	if not visible or _selected_provider() == ConversationService.PROVIDER_CLOUD:
		return
	if models.is_empty():
		_populate_dropdown([ConversationService.get_model()])
		if status_label:
			status_label.text = "Connected, but no models installed. Run: ollama pull %s" % ConversationService.DEFAULT_MODEL
		return
	_populate_dropdown(models)
	if status_label:
		status_label.text = "Connected. %d model(s) available." % models.size()

func _on_models_failed(reason: String) -> void:
	if not visible or _selected_provider() == ConversationService.PROVIDER_CLOUD:
		return
	if status_label:
		status_label.text = reason

func _populate_dropdown(models: Array) -> void:
	if not model_dropdown:
		return
	model_dropdown.clear()
	var current := ConversationService.get_model()
	var selected_idx := 0
	for i in range(models.size()):
		model_dropdown.add_item(models[i])
		if models[i] == current:
			selected_idx = i
	if model_dropdown.item_count > 0:
		model_dropdown.select(selected_idx)

func _on_save_pressed() -> void:
	var provider := _selected_provider()
	SettingsManager.set_setting("AI", "provider", provider)
	if url_edit:
		SettingsManager.set_setting("AI", "server_url", url_edit.text.strip_edges())
	if key_edit:
		SettingsManager.set_setting("AI", "api_key", key_edit.text.strip_edges())
	if model_dropdown and model_dropdown.item_count > 0:
		var chosen := model_dropdown.get_item_text(model_dropdown.selected)
		if provider == ConversationService.PROVIDER_CLOUD:
			SettingsManager.set_setting("AI", "cloud_model", chosen)
		else:
			SettingsManager.set_setting("AI", "model", chosen)
	SettingsManager.set_setting("AI", "configured", true)
	SettingsManager.save_settings()
	visible = false
	InputManager.pop_mode()

func _on_close_requested() -> void:
	visible = false
	InputManager.pop_mode()
