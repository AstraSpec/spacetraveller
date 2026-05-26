#ifndef SPACETRAVELLER_ACTION_RESOLVER_H
#define SPACETRAVELLER_ACTION_RESOLVER_H

#include <godot_cpp/variant/vector2i.hpp>
#include <cstdint>

namespace godot {

class WorldBubble;
struct Entity;
struct LocomotionData;

enum class IntentType { NONE, MOVE };

struct Intent {
    IntentType type = IntentType::NONE;
    Vector2i target;
};

namespace ActionResolver {
    float resolve_move(const Intent& intent, Entity& entity,
                       WorldBubble& bubble, LocomotionData& loco);
}

}

#endif // SPACETRAVELLER_ACTION_RESOLVER_H