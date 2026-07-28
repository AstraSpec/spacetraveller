#ifndef SPACETRAVELLER_ACTIVITY_H
#define SPACETRAVELLER_ACTIVITY_H

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3i.hpp>
#include <cstdint>
#include <vector>

namespace godot {

struct ActivityData {
    String type;
    String subject_id;
    float total_time = 0.0f;
    float completed_time = 0.0f;
    String state = "running";
    Vector3i start_position;
    std::vector<String> ignored_interruptions;
    String pending_interruption;
    uint32_t pending_source_entity = UINT32_MAX;
    String return_menu = "inventory";
    String return_tab = "crafting";

    // Scheduler bookkeeping is persisted so an activity can continue exactly.
    float simulation_time = 0.0f;
    float last_refresh_time = 0.0f;
    float next_refresh_time = 30.0f;
    float scheduled_work = 0.0f;
};

namespace Activity {
    bool is_active(const ActivityData& data);
    bool is_running(const ActivityData& data);
    bool is_interrupted(const ActivityData& data);
    bool ignores(const ActivityData& data, const String& interruption_id);
    void ignore(ActivityData& data, const String& interruption_id);
    Dictionary to_dictionary(const ActivityData& data, float elapsed = 0.0f);
    Dictionary serialize(const ActivityData& data);
    void deserialize(ActivityData& data, const Dictionary& value);
    Dictionary interruption_definition(const String& interruption_id);
}

}

#endif
