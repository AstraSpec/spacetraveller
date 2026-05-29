#ifndef SPACETRAVELLER_SIMULATION_EVENT_SINK_H
#define SPACETRAVELLER_SIMULATION_EVENT_SINK_H

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <cstdint>

namespace godot {

class ISimulationEventSink {
public:
    virtual ~ISimulationEventSink() = default;

    virtual void on_entity_moved(uint32_t entity_id, const Vector2i& new_pos, const Vector2i& new_chunk) = 0;
    virtual void on_entity_died(uint32_t entity_id, const String& cause) = 0;
    virtual void on_player_turn_ready(uint32_t entity_id) = 0;
    virtual void on_player_action_resolved(uint32_t entity_id, float cost, float next_turn_time) = 0;
    virtual void on_combat_event(uint32_t attacker_id, uint32_t defender_id, float damage, const String& result) = 0;
};

}

#endif // SPACETRAVELLER_SIMULATION_EVENT_SINK_H
