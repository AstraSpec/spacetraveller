#include "activity.h"

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <algorithm>

using namespace godot;

Dictionary Activity::interruption_definition(const String& interruption_id) {
    Dictionary definition;
    definition["id"] = interruption_id;
    definition["allow_ignore"] = false;
    if (interruption_id == "manual_cancel") {
        definition["prompt"] = "Stop this activity? All progress will be lost.";
    } else if (interruption_id == "attacked") {
        definition["prompt"] = "You were attacked while working.";
        definition["allow_ignore"] = true;
    } else if (interruption_id == "hostile_spotted") {
        definition["prompt"] = "A hostile creature was spotted.";
        definition["allow_ignore"] = true;
    } else if (interruption_id == "missing_resources") {
        definition["prompt"] = "Required resources are no longer available.";
    } else if (interruption_id == "environment_changed") {
        definition["prompt"] = "The surrounding conditions changed.";
        definition["allow_ignore"] = true;
    } else {
        definition["prompt"] = String("Activity interrupted: ") + interruption_id.replace("_", " ");
    }
    return definition;
}

bool Activity::is_active(const ActivityData& data) {
    return !data.type.is_empty() && data.total_time > 0.0f;
}

bool Activity::is_running(const ActivityData& data) {
    return is_active(data) && data.state == "running";
}

bool Activity::is_interrupted(const ActivityData& data) {
    return is_active(data) && data.state == "interrupted";
}

bool Activity::ignores(const ActivityData& data, const String& interruption_id) {
    return std::find(
        data.ignored_interruptions.begin(),
        data.ignored_interruptions.end(),
        interruption_id
    ) != data.ignored_interruptions.end();
}

void Activity::ignore(ActivityData& data, const String& interruption_id) {
    if (!interruption_id.is_empty() && !ignores(data, interruption_id)) {
        data.ignored_interruptions.push_back(interruption_id);
    }
}

Dictionary Activity::to_dictionary(const ActivityData& data, float elapsed) {
    Dictionary out;
    out["active"] = is_active(data);
    out["type"] = data.type;
    out["subject_id"] = data.subject_id;
    out["total_time"] = data.total_time;
    out["completed_time"] = data.completed_time;
    out["remaining_time"] = MAX(0.0f, data.total_time - data.completed_time);
    out["state"] = data.state;
    out["start_position"] = data.start_position;
    out["pending_interruption"] = data.pending_interruption;
    out["pending_source_entity"] = data.pending_source_entity == UINT32_MAX
        ? -1
        : static_cast<int64_t>(data.pending_source_entity);
    out["return_menu"] = data.return_menu;
    out["return_tab"] = data.return_tab;
    out["has_crafting_station"] = data.has_crafting_station;
    if (data.has_crafting_station) {
        out["crafting_station_position"] = data.crafting_station_position;
        out["crafting_station_tile_id"] = data.crafting_station_tile_id;
    }
    out["elapsed"] = MAX(0.0f, elapsed);
    if (!data.pending_interruption.is_empty()) {
        out["interruption"] = interruption_definition(data.pending_interruption);
    }

    Array ignored;
    for (const String& id : data.ignored_interruptions) ignored.push_back(id);
    out["ignored_interruptions"] = ignored;
    return out;
}

Dictionary Activity::serialize(const ActivityData& data) {
    Dictionary out = to_dictionary(data);
    out.erase("active");
    out.erase("remaining_time");
    out.erase("elapsed");
    out.erase("interruption");
    out.erase("start_position");
    out["start_x"] = data.start_position.x;
    out["start_y"] = data.start_position.y;
    out["start_z"] = data.start_position.z;
    out["simulation_time"] = data.simulation_time;
    out["last_refresh_time"] = data.last_refresh_time;
    out["next_refresh_time"] = data.next_refresh_time;
    out["scheduled_work"] = data.scheduled_work;
    if (data.has_crafting_station) {
        out.erase("crafting_station_position");
        out["crafting_station_x"] = data.crafting_station_position.x;
        out["crafting_station_y"] = data.crafting_station_position.y;
        out["crafting_station_z"] = data.crafting_station_position.z;
    }
    return out;
}

void Activity::deserialize(ActivityData& data, const Dictionary& value) {
    data = ActivityData();
    data.type = value.get("type", String());
    data.subject_id = value.get("subject_id", String());
    data.total_time = static_cast<float>(static_cast<double>(value.get("total_time", 0.0)));
    data.completed_time = static_cast<float>(static_cast<double>(value.get("completed_time", 0.0)));
    const String saved_state = value.get("state", String("running"));
    data.state = saved_state == "interrupted" ? String("interrupted") : String("running");
    if (value.has("start_position")) {
        data.start_position = value.get("start_position", Vector3i());
    } else {
        data.start_position = Vector3i(
            static_cast<int>(value.get("start_x", 0)),
            static_cast<int>(value.get("start_y", 0)),
            static_cast<int>(value.get("start_z", 0))
        );
    }
    data.pending_interruption = value.get("pending_interruption", String());
    const int64_t source = value.get("pending_source_entity", static_cast<int64_t>(-1));
    data.pending_source_entity = source < 0 ? UINT32_MAX : static_cast<uint32_t>(source);
    data.return_menu = value.get("return_menu", String("inventory"));
    data.return_tab = value.get("return_tab", String("crafting"));
    data.has_crafting_station = value.get("has_crafting_station", false);
    if (data.has_crafting_station) {
        data.crafting_station_position = Vector3i(
            static_cast<int>(value.get("crafting_station_x", 0)),
            static_cast<int>(value.get("crafting_station_y", 0)),
            static_cast<int>(value.get("crafting_station_z", 0)));
        data.crafting_station_tile_id =
            value.get("crafting_station_tile_id", String());
    }
    data.simulation_time = static_cast<float>(static_cast<double>(value.get("simulation_time", 0.0)));
    data.last_refresh_time = static_cast<float>(static_cast<double>(value.get("last_refresh_time", data.simulation_time)));
    data.next_refresh_time = static_cast<float>(static_cast<double>(value.get("next_refresh_time", data.last_refresh_time + 30.0f)));
    data.scheduled_work = static_cast<float>(static_cast<double>(value.get("scheduled_work", 0.0)));

    Array ignored = value.get("ignored_interruptions", Array());
    for (int i = 0; i < ignored.size(); ++i) {
        String id = ignored[i];
        if (!id.is_empty()) data.ignored_interruptions.push_back(id);
    }
}
