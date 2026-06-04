#ifndef SPACETRAVELLER_GAME_EVENT_H
#define SPACETRAVELLER_GAME_EVENT_H

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <cstdint>

namespace godot {

enum class GameEventType : uint8_t {
    ENTITY_KILLED,    // subject=attacker, target=victim, position=victim pos
    ITEM_PICKED_UP,   // subject=picker, item_id (resolved uint16), amount, position=picker pos
    ENTITY_MOVED,     // subject=entity, position=new_pos
};

struct GameEvent {
    GameEventType type     = GameEventType::ENTITY_MOVED;
    uint32_t      subject_id = 0;
    uint32_t      target_id  = 0;
    uint16_t      item_id    = 0;
    uint16_t      _pad       = 0;
    Vector2i      position   = Vector2i();
    int           amount     = 0;
};

class IGameEventListener {
public:
    virtual ~IGameEventListener() = default;
    virtual void on_game_event(const GameEvent& p_event) = 0;
};

}

#endif // SPACETRAVELLER_GAME_EVENT_H
