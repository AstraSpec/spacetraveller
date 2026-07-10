extends RefCounted
class_name NpcActionRegistry

var _providers: Array[Callable] = []

func register_provider(provider: Callable) -> void:
	_providers.append(provider)

func build(world: GameWorld, target_id: int) -> Array:
	var actions: Array = []
	for provider in _providers:
		var provided: Variant = provider.call(world, target_id)
		if not provided is Array:
			continue
		for action in provided:
			if action is NpcAction:
				actions.append(action)
	return actions
