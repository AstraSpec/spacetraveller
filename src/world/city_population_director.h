#ifndef SPACETRAVELLER_CITY_POPULATION_DIRECTOR_H
#define SPACETRAVELLER_CITY_POPULATION_DIRECTOR_H

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/vector3i.hpp>

#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

namespace godot {

class EntityLedger;
class EntityTracker;
class PointOfInterestRegistry;
class TurnScheduler;
class WorldBubble;
class WorldGenerator;

enum class AmbientJourneyPhase : uint8_t {
    FOLLOWING_ROUTE,
    TRAVELLING_TO_DETOUR,
    DWELLING,
    RETURNING_TO_ROAD
};

enum class RoadLateralPreference : int8_t {
    LEFT = -1,
    CENTER = 0,
    RIGHT = 1
};

struct AmbientJourneyData {
    std::vector<Vector3i> route;
    int route_index = 1;
    int travel_direction = -1;
    RoadLateralPreference lateral_preference = RoadLateralPreference::CENTER;
    AmbientJourneyPhase phase = AmbientJourneyPhase::FOLLOWING_ROUTE;
    bool wants_detour = false;
    bool detour_attempted = false;
    Vector3i detour_target;
    std::vector<Vector3i> detour_road_route;
    int detour_road_index = 1;
    std::vector<Vector3i> detour_final_path;
    int detour_final_index = 1;
    int return_final_index = -1;
    int dwell_remaining = 0;
    int blocked_turns = 0;
};

struct CityPopulationConfig {
    int day_target = 4;
    int night_target = 2;
    int replenish_min = 1;
    int replenish_max = 100;
    int entry_cooldown_turns = 12;
    int entry_min_distance = 10;
    float exploration_spawn_chance = 0.03f;
    float detour_chance = 0.35f;
    int detour_radius = 10;
    String entity_group = "city_ambient_population";
    std::vector<String> detour_tags;
};

class CityPopulationDirector {
    struct PendingActivationBatch {
        std::vector<uint64_t> cells;
        Vector2i center;
        int z = 0;
        bool bulk = true;
    };

    WorldGenerator* generator = nullptr;
    WorldBubble* bubble = nullptr;
    EntityLedger* ledger = nullptr;
    EntityTracker* tracker = nullptr;
    TurnScheduler* scheduler = nullptr;
    PointOfInterestRegistry* poi_registry = nullptr;
    const int* world_seed = nullptr;

    CityPopulationConfig config;
    std::unordered_map<uint32_t, AmbientJourneyData> journeys;
    std::unordered_map<uint64_t, int64_t> entry_last_used;
    std::unordered_map<uint64_t, uint32_t> road_cell_reservations;
    std::unordered_map<uint32_t, uint64_t> entity_road_reservations;
    PendingActivationBatch pending_activation;
    int64_t next_replenish_turn = std::numeric_limits<int64_t>::min();
    int64_t latest_calendar_turn = 0;
    bool latest_is_day = true;
    bool has_population_context = false;
    Vector2i last_activation_center;
    int last_activation_z = 0;
    bool has_last_activation_center = false;
    uint64_t spawn_serial = 0;

    bool load_config();
    bool is_city_entry_cell(const Vector3i& p_position) const;
    bool is_gameplay_visible(const Vector3i& p_position, const Vector2i& p_player_position) const;
    int roll_replenish_delay(const Vector2i& p_center, int64_t p_calendar_turn);
    bool entry_is_available(
        const Vector3i& p_entry,
        int64_t p_calendar_turn,
        const std::vector<Vector3i>& p_batch_entries
    ) const;
    bool build_boundary_route(
        const Vector2i& p_center,
        int p_z,
        int64_t p_calendar_turn,
        uint64_t p_salt,
        RoadLateralPreference p_lateral_preference,
        const std::vector<Vector3i>& p_batch_entries,
        std::vector<Vector3i>& r_route
    ) const;
    bool build_route_to_exit(
        const Vector3i& p_start,
        const Vector2i& p_center,
        const Vector3i* p_preferred_exit,
        bool p_allow_same_side,
        bool p_avoid_occupied,
        RoadLateralPreference p_lateral_preference,
        std::vector<Vector3i>& r_route
    ) const;
    bool build_directed_route_to_exit(
        const Vector3i& p_start,
        const Vector2i& p_center,
        int p_initial_direction,
        RoadLateralPreference p_lateral_preference,
        bool p_avoid_occupied,
        std::vector<Vector3i>& r_route
    ) const;
    bool spawn_route(
        std::vector<Vector3i>&& p_route,
        RoadLateralPreference p_lateral_preference,
        int64_t p_calendar_turn,
        uint64_t p_salt,
        int p_travel_direction
    );
    bool spawn_ingress(
        const Vector2i& p_center,
        int64_t p_calendar_turn,
        std::vector<Vector3i>& r_batch_entries
    );
    int process_exploration_activation(
        int p_target,
        int64_t p_calendar_turn,
        const Vector2i& p_player_position
    );

public:
    void configure(
        WorldGenerator* p_generator,
        WorldBubble* p_bubble,
        EntityLedger* p_ledger,
        EntityTracker* p_tracker,
        TurnScheduler* p_scheduler,
        PointOfInterestRegistry* p_poi_registry,
        const int* p_world_seed
    );
    void clear();
    void queue_exploration_activation(
        std::vector<uint64_t>&& p_cells,
        const Vector2i& p_center,
        int p_z
    );
    void update(int64_t p_calendar_turn, bool p_is_day, const Vector2i& p_player_position);

    AmbientJourneyData* get_journey(uint32_t p_entity_id);
    const AmbientJourneyData* get_journey(uint32_t p_entity_id) const;
    bool is_ambient(uint32_t p_entity_id) const;
    bool is_inside_route_area(
        const Vector3i& p_position,
        const Vector2i& p_center
    ) const;
    bool is_road_position(const Vector3i& p_position) const;
    bool find_detour_approach(
        const Vector3i& p_start,
        const Vector3i& p_target,
        std::vector<Vector3i>& r_road_route,
        Vector3i& r_approach
    ) const;
    bool find_alternate_road_step(
        uint32_t p_entity_id,
        const Vector3i& p_current,
        const Vector3i& p_goal,
        Vector3i& r_step
    ) const;
    bool try_reserve_road_cell(const Vector3i& p_position, uint32_t p_entity_id);
    bool is_road_cell_reserved_by_other(
        const Vector3i& p_position,
        uint32_t p_entity_id
    ) const;
    void release_road_reservation(uint32_t p_entity_id);
    bool reroute(uint32_t p_entity_id);
    bool reroute_from(uint32_t p_entity_id, const Vector3i& p_start);
    bool reroute_around_congestion(uint32_t p_entity_id);
    bool continue_route(uint32_t p_entity_id);
    void cancel_detour(uint32_t p_entity_id);
    void release_for_entity(uint32_t p_entity_id);
    void despawn_ambient(uint32_t p_entity_id);
    bool promote(uint32_t p_entity_id);

    int get_detour_radius() const { return config.detour_radius; }
    const std::vector<String>& get_detour_tags() const { return config.detour_tags; }
};

}

#endif // SPACETRAVELLER_CITY_POPULATION_DIRECTOR_H
