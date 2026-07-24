#include "city_population_director.h"

#include "components/ai_controller.h"
#include "components/locomotion.h"
#include "components/perception.h"
#include "core/rng.h"
#include "core/tag_registry.h"
#include "core/world_coords.h"
#include "data/chunk_db.h"
#include "data/entity_group_db.h"
#include "data/tile_db.h"
#include "entities/entity_factory.h"
#include "entities/entity_ledger.h"
#include "entities/entity_pool.h"
#include "entities/entity_tracker.h"
#include "entity_lifecycle.h"
#include "light_level.h"
#include "point_of_interest_registry.h"
#include "turn_scheduler.h"
#include "world_bubble.h"
#include "world_generator.h"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <queue>
#include <unordered_map>

using namespace godot;

namespace {

constexpr int ROUTE_OPENING_LENGTH = 10;
constexpr int ROUTE_OPENING_REJECT_OVERLAP = 4;
constexpr int POPULATION_ROUTE_MARGIN = 4;
constexpr int ROAD_STEP_COST = 100;
constexpr int ROAD_LATERAL_COST = 8;
constexpr int ROAD_TURN_COST = 24;
constexpr int ORDINARY_EXPLORATION_ROUTE_ATTEMPTS = 4;
constexpr int BULK_EXPLORATION_ROUTE_ATTEMPTS = 24;
constexpr float BULK_EXPLORATION_SLOT_CHANCE = 0.5f;

const Vector2i CARDINAL_DIRECTIONS[] = {
    Vector2i(0, -1),
    Vector2i(1, 0),
    Vector2i(0, 1),
    Vector2i(-1, 0)
};

CellArea population_route_area(
    const WorldBubble* p_bubble,
    const Vector2i& p_center,
    int p_z
) {
    const int radius = p_bubble
        ? p_bubble->get_player_vision_radius() + POPULATION_ROUTE_MARGIN
        : POPULATION_ROUTE_MARGIN;
    return CellArea::square(p_center, p_z, radius);
}

int boundary_side(const Vector2i& p_position, const CellArea& p_area) {
    const int min_x = p_area.center.x - p_area.radius;
    const int min_y = p_area.center.y - p_area.radius;
    const int max_x = p_area.center.x + p_area.radius - 1;
    const int max_y = p_area.center.y + p_area.radius - 1;
    if (p_position.y == min_y) return 0;
    if (p_position.x == max_x) return 1;
    if (p_position.y == max_y) return 2;
    if (p_position.x == min_x) return 3;
    const int north = std::abs(p_position.y - min_y);
    const int east = std::abs(max_x - p_position.x);
    const int south = std::abs(max_y - p_position.y);
    const int west = std::abs(p_position.x - min_x);
    const int nearest = std::min(std::min(north, east), std::min(south, west));
    if (nearest == north) return 0;
    if (nearest == east) return 1;
    if (nearest == south) return 2;
    return 3;
}

bool is_on_boundary(const Vector2i& p_position, const CellArea& p_area) {
    const int min_x = p_area.center.x - p_area.radius;
    const int min_y = p_area.center.y - p_area.radius;
    const int max_x = p_area.center.x + p_area.radius - 1;
    const int max_y = p_area.center.y + p_area.radius - 1;
    return p_position.x == min_x
        || p_position.x == max_x
        || p_position.y == min_y
        || p_position.y == max_y;
}

int inward_direction_for_side(int p_side) {
    switch (p_side) {
        case 0: return 2;
        case 1: return 3;
        case 2: return 0;
        default: return 1;
    }
}

int direction_toward(const Vector3i& p_from, const Vector3i& p_to) {
    const int dx = p_to.x - p_from.x;
    const int dy = p_to.y - p_from.y;
    if (std::abs(dx) > std::abs(dy)) return dx > 0 ? 1 : 3;
    return dy > 0 ? 2 : 0;
}

int positive_mod(int p_value, int p_modulus) {
    const int result = p_value % p_modulus;
    return result < 0 ? result + p_modulus : result;
}

int floor_div_chunk(int p_value) {
    return p_value >= 0
        ? p_value / WorldCoords::CHUNK_SIZE
        : (p_value - (WorldCoords::CHUNK_SIZE - 1)) / WorldCoords::CHUNK_SIZE;
}

bool is_on_direction_boundary(
    const Vector2i& p_position,
    const CellArea& p_area,
    int p_direction
) {
    switch (p_direction) {
        case 0: return p_position.y == p_area.center.y - p_area.radius;
        case 1: return p_position.x == p_area.center.x + p_area.radius - 1;
        case 2: return p_position.y == p_area.center.y + p_area.radius - 1;
        default: return p_position.x == p_area.center.x - p_area.radius;
    }
}

template <typename T>
void seeded_shuffle(std::vector<T>& r_values, Rng::Seeded& p_rng) {
    for (int i = static_cast<int>(r_values.size()) - 1; i > 0; --i) {
        const int swap_index = p_rng.range(0, i);
        if (swap_index != i) std::swap(r_values[i], r_values[swap_index]);
    }
}

struct ExplorationCandidate {
    Vector3i position;
    uint64_t chunk_key = 0;
    uint8_t direction_mask = 0;
};

struct ExplorationChunkBucket {
    uint64_t chunk_key = 0;
    uint8_t direction_mask = 0;
    std::vector<Vector3i> positions;
};

int road_lateral_penalty(
    const Vector3i& p_position,
    int p_direction,
    RoadLateralPreference p_preference
) {
    const int low = (WorldCoords::CHUNK_SIZE - 1) / 4;
    const int center = (WorldCoords::CHUNK_SIZE - 1) / 2;
    const int high = ((WorldCoords::CHUNK_SIZE - 1) * 3) / 4;
    const bool vertical = p_direction == 0 || p_direction == 2;
    const int lateral = positive_mod(
        vertical ? p_position.x : p_position.y,
        WorldCoords::CHUNK_SIZE);
    if (p_preference == RoadLateralPreference::CENTER) {
        return std::abs(lateral - center);
    }

    const bool moving_north_or_east = p_direction == 0 || p_direction == 1;
    const bool prefer_low =
        (p_preference == RoadLateralPreference::LEFT)
        == moving_north_or_east;
    return std::abs(lateral - (prefer_low ? low : high));
}

RoadLateralPreference nearest_lateral_preference(
    const Vector3i& p_position,
    int p_direction
) {
    static const RoadLateralPreference preferences[] = {
        RoadLateralPreference::LEFT,
        RoadLateralPreference::CENTER,
        RoadLateralPreference::RIGHT
    };
    RoadLateralPreference selected = RoadLateralPreference::CENTER;
    int selected_penalty = std::numeric_limits<int>::max();
    for (RoadLateralPreference preference : preferences) {
        const int penalty =
            road_lateral_penalty(p_position, p_direction, preference);
        if (penalty < selected_penalty) {
            selected = preference;
            selected_penalty = penalty;
        }
    }
    return selected;
}

int chebyshev_distance(const Vector3i& p_a, const Vector3i& p_b) {
    return std::max(std::abs(p_a.x - p_b.x), std::abs(p_a.y - p_b.y));
}

int manhattan_distance(const Vector3i& p_a, const Vector3i& p_b) {
    return std::abs(p_a.x - p_b.x) + std::abs(p_a.y - p_b.y);
}

bool routes_overlap_opening(
    const std::vector<Vector3i>& p_a,
    const std::vector<Vector3i>& p_b
) {
    const int a_limit = std::min(ROUTE_OPENING_LENGTH, static_cast<int>(p_a.size()));
    const int b_limit = std::min(ROUTE_OPENING_LENGTH, static_cast<int>(p_b.size()));
    int overlap = 0;
    for (int a = 0; a < a_limit; ++a) {
        for (int b = 0; b < b_limit; ++b) {
            if (p_a[a] == p_b[b]) {
                ++overlap;
                break;
            }
        }
    }
    return overlap >= ROUTE_OPENING_REJECT_OVERLAP;
}

}

void CityPopulationDirector::configure(
    WorldGenerator* p_generator,
    WorldBubble* p_bubble,
    EntityLedger* p_ledger,
    EntityTracker* p_tracker,
    TurnScheduler* p_scheduler,
    PointOfInterestRegistry* p_poi_registry,
    const int* p_world_seed
) {
    generator = p_generator;
    bubble = p_bubble;
    ledger = p_ledger;
    tracker = p_tracker;
    scheduler = p_scheduler;
    poi_registry = p_poi_registry;
    world_seed = p_world_seed;
    load_config();
}

bool CityPopulationDirector::load_config() {
    const String path = "res://data/populations/city.json";
    const String text = FileAccess::get_file_as_string(path);
    if (text.is_empty()) {
        UtilityFunctions::push_error("[CityPopulationDirector] Unable to read ", path);
        return false;
    }
    const Variant parsed = JSON::parse_string(text);
    if (parsed.get_type() != Variant::DICTIONARY) {
        UtilityFunctions::push_error("[CityPopulationDirector] Invalid population config: ", path);
        return false;
    }

    const Dictionary row = parsed;
    config.day_target = std::max(0, static_cast<int>(row.get("day_target", config.day_target)));
    config.night_target = std::max(0, static_cast<int>(row.get("night_target", config.night_target)));
    const Array replenish = row.get("replenish_turns", Array());
    if (replenish.size() == 2
        && replenish[0].get_type() == Variant::INT
        && replenish[1].get_type() == Variant::INT) {
        config.replenish_min = std::max(1, static_cast<int>(replenish[0]));
        config.replenish_max = std::max(config.replenish_min, static_cast<int>(replenish[1]));
    }
    config.entry_cooldown_turns = std::max(
        0, static_cast<int>(row.get("entry_cooldown_turns", config.entry_cooldown_turns)));
    config.entry_min_distance = std::max(
        0, static_cast<int>(row.get("entry_min_distance", config.entry_min_distance)));
    config.exploration_spawn_chance = std::clamp(
        static_cast<float>(row.get(
            "exploration_spawn_chance", config.exploration_spawn_chance)),
        0.0f,
        1.0f
    );
    config.detour_chance = std::clamp(
        static_cast<float>(row.get("detour_chance", config.detour_chance)), 0.0f, 1.0f);
    config.detour_radius = std::max(1, static_cast<int>(row.get("detour_radius", config.detour_radius)));
    config.entity_group = String(row.get("entity_group", config.entity_group));
    config.detour_tags.clear();
    const Array tags = row.get("detour_tags", Array());
    for (int i = 0; i < tags.size(); ++i) {
        if (tags[i].get_type() != Variant::STRING) continue;
        const String tag = String(tags[i]).strip_edges().to_lower();
        if (!tag.is_empty()) config.detour_tags.push_back(tag);
    }
    return !config.entity_group.is_empty();
}

void CityPopulationDirector::clear() {
    journeys.clear();
    entry_last_used.clear();
    road_cell_reservations.clear();
    entity_road_reservations.clear();
    pending_activation = PendingActivationBatch();
    next_replenish_turn = std::numeric_limits<int64_t>::min();
    latest_calendar_turn = 0;
    latest_is_day = true;
    has_population_context = false;
    last_activation_center = Vector2i();
    last_activation_z = 0;
    has_last_activation_center = false;
    spawn_serial = 0;
}

void CityPopulationDirector::queue_exploration_activation(
    std::vector<uint64_t>&& p_cells,
    const Vector2i& p_center,
    int p_z
) {
    if (p_cells.empty()) return;
    const bool ordinary_movement =
        has_last_activation_center
        && p_z == last_activation_z
        && std::max(
            std::abs(p_center.x - last_activation_center.x),
            std::abs(p_center.y - last_activation_center.y)) == 1;
    pending_activation.cells = std::move(p_cells);
    pending_activation.center = p_center;
    pending_activation.z = p_z;
    pending_activation.bulk = !ordinary_movement;
    last_activation_center = p_center;
    last_activation_z = p_z;
    has_last_activation_center = true;
    if (pending_activation.bulk && has_population_context && p_z == 0) {
        const int target =
            latest_is_day ? config.day_target : config.night_target;
        process_exploration_activation(
            target, latest_calendar_turn, p_center);
    }
}

bool CityPopulationDirector::is_road_position(const Vector3i& p_position) const {
    TileDb* tile_db = TileDb::get_singleton();
    TagRegistry* tags = TagRegistry::get_singleton();
    if (!bubble || !tile_db || !tags) return false;
    const uint16_t road_tag = tags->get_tag_id("ROAD");
    if (road_tag == 0) return false;
    const uint16_t tile_id = bubble->query_tile_id_at_z(p_position.x, p_position.y, p_position.z);
    const TileInfo* tile = tile_db->get_tile_info(tile_id);
    return tile && !tile->solid && tile_db->has_tag(tile_id, road_tag);
}

bool CityPopulationDirector::is_city_entry_cell(const Vector3i& p_position) const {
    if (!generator || !world_seed || !is_road_position(p_position)) return false;
    ChunkDb* chunk_db = ChunkDb::get_singleton();
    TagRegistry* tags = TagRegistry::get_singleton();
    if (!chunk_db || !tags) return false;
    const uint16_t entry_tag = tags->get_tag_id("CITY_AMBIENT_ENTRY");
    if (entry_tag == 0) return false;
    const uint16_t chunk_id = generator->get_biome_id_for_cell(
        p_position.x, p_position.y, p_position.z, *world_seed);
    return chunk_db->has_tag(chunk_id, entry_tag);
}

bool CityPopulationDirector::is_gameplay_visible(
    const Vector3i& p_position,
    const Vector2i& p_player_position
) const {
    if (!bubble || p_position.z != bubble->get_active_z()) return false;
    const int distance = std::max(
        std::abs(p_position.x - p_player_position.x),
        std::abs(p_position.y - p_player_position.y));
    if (distance > bubble->get_player_vision_radius()) return false;
    TileDb* tile_db = TileDb::get_singleton();
    if (!tile_db || !Perception::has_line_of_sight(
            p_player_position.x,
            p_player_position.y,
            p_position.x,
            p_position.y,
            *bubble,
            *tile_db)) {
        return false;
    }
    return light_reveals_dynamics(
        bubble->get_light_level_at(p_position.x, p_position.y));
}

int CityPopulationDirector::roll_replenish_delay(
    const Vector2i& p_center,
    int64_t p_calendar_turn
) {
    if (!world_seed) return config.replenish_min;
    Rng::Seeded rng = Rng::at(
        static_cast<uint32_t>(*world_seed),
        p_center,
        Rng::SPAWN,
        Rng::mix64(static_cast<uint64_t>(p_calendar_turn) ^ (++spawn_serial << 16))
    );
    return rng.range(config.replenish_min, config.replenish_max);
}

bool CityPopulationDirector::entry_is_available(
    const Vector3i& p_entry,
    int64_t p_calendar_turn,
    const std::vector<Vector3i>& p_batch_entries
) const {
    const uint64_t key = WorldCoords::pack_coords_3d(p_entry.x, p_entry.y, p_entry.z);
    auto recent = entry_last_used.find(key);
    if (recent != entry_last_used.end()
        && p_calendar_turn - recent->second < config.entry_cooldown_turns) {
        return false;
    }
    for (const Vector3i& selected : p_batch_entries) {
        if (chebyshev_distance(p_entry, selected) < config.entry_min_distance) return false;
    }
    for (const auto& pair : journeys) {
        if (!pair.second.route.empty()
            && chebyshev_distance(p_entry, pair.second.route.front()) < config.entry_min_distance) {
            return false;
        }
    }
    return true;
}

bool CityPopulationDirector::build_route_to_exit(
    const Vector3i& p_start,
    const Vector2i& p_center,
    const Vector3i* p_preferred_exit,
    bool p_allow_same_side,
    bool p_avoid_occupied,
    RoadLateralPreference p_lateral_preference,
    std::vector<Vector3i>& r_route
) const {
    if (!bubble || !is_road_position(p_start)) return false;
    const CellArea area = population_route_area(bubble, p_center, p_start.z);
    if (!area.contains_world(p_start.x, p_start.y, p_start.z)) return false;

    using RouteNode = std::pair<int, uint64_t>;
    std::priority_queue<RouteNode, std::vector<RouteNode>, std::greater<RouteNode>> frontier;
    std::unordered_map<uint64_t, uint64_t> parents;
    std::unordered_map<uint64_t, int> costs;
    std::unordered_map<uint64_t, int> steps;
    std::unordered_map<uint64_t, bool> road_cache;
    auto is_route_road = [&](const Vector2i& position) {
        const uint64_t key = WorldCoords::pack_coords(position.x, position.y);
        auto cached = road_cache.find(key);
        if (cached != road_cache.end()) return cached->second;
        const bool is_road = is_road_position(
            Vector3i(position.x, position.y, p_start.z));
        road_cache[key] = is_road;
        return is_road;
    };
    auto add_start = [&](int direction) {
        const uint64_t key = WorldCoords::pack_coords_3d(
            p_start.x, p_start.y, direction);
        frontier.emplace(0, key);
        parents[key] = key;
        costs[key] = 0;
        steps[key] = 0;
    };

    const Vector2i start_position(p_start.x, p_start.y);
    if (is_on_boundary(start_position, area)) {
        add_start(inward_direction_for_side(boundary_side(start_position, area)));
    } else if (p_preferred_exit) {
        add_start(direction_toward(p_start, *p_preferred_exit));
    } else {
        for (int direction = 0; direction < 4; ++direction) add_start(direction);
    }

    while (!frontier.empty()) {
        const auto [current_cost, current_key] = frontier.top();
        frontier.pop();
        auto known_cost = costs.find(current_key);
        if (known_cost == costs.end() || current_cost != known_cost->second) continue;
        const Vector3i current_state = WorldCoords::unpack_coords_3d(current_key);
        const Vector2i current(current_state.x, current_state.y);
        const int previous_direction = current_state.z;
        for (int direction = 0; direction < 4; ++direction) {
            const Vector2i next = current + CARDINAL_DIRECTIONS[direction];
            if (!area.contains_world(next.x, next.y, p_start.z)) continue;
            const Vector3i next_world(next.x, next.y, p_start.z);
            if (!is_route_road(next)) continue;
            if (p_avoid_occupied
                && tracker
                && tracker->get_at(next_world) != EntityPool::INVALID_ID) {
                continue;
            }
            const uint64_t key = WorldCoords::pack_coords_3d(
                next.x, next.y, direction);
            int move_cost = ROAD_STEP_COST
                + road_lateral_penalty(
                    next_world, direction, p_lateral_preference)
                    * ROAD_LATERAL_COST;
            if (direction != previous_direction) {
                move_cost += ROAD_TURN_COST;
                if ((direction + 2) % 4 == previous_direction) {
                    move_cost += ROAD_TURN_COST;
                }
            }
            const int next_cost = current_cost + move_cost;
            auto existing = costs.find(key);
            if (existing != costs.end() && existing->second <= next_cost) continue;
            costs[key] = next_cost;
            steps[key] = steps[current_key] + 1;
            parents[key] = current_key;
            frontier.emplace(next_cost, key);
        }
    }

    const int start_side = boundary_side(Vector2i(p_start.x, p_start.y), area);
    uint64_t selected_state = 0;
    int selected_score = std::numeric_limits<int>::min();
    bool selected_any = false;
    const int min_x = area.center.x - area.radius;
    const int min_y = area.center.y - area.radius;
    const int max_x = area.center.x + area.radius - 1;
    const int max_y = area.center.y + area.radius - 1;
    auto consider = [&](const Vector2i& candidate) {
        if (candidate == Vector2i(p_start.x, p_start.y)) return;
        const Vector3i world(candidate.x, candidate.y, p_start.z);
        if (!is_city_entry_cell(world)) return;
        const int side = boundary_side(candidate, area);
        if (!p_allow_same_side && side == start_side) return;
        uint64_t best_state = 0;
        int best_cost = std::numeric_limits<int>::max();
        for (int direction = 0; direction < 4; ++direction) {
            const uint64_t key = WorldCoords::pack_coords_3d(
                candidate.x, candidate.y, direction);
            auto cost = costs.find(key);
            if (cost != costs.end() && cost->second < best_cost) {
                best_state = key;
                best_cost = cost->second;
            }
        }
        if (best_cost == std::numeric_limits<int>::max()) return;
        int score = steps.at(best_state);
        if (start_side >= 0 && side == (start_side + 2) % 4) score += area.radius * 3;
        if (p_preferred_exit && world == *p_preferred_exit) score += area.radius * 8;
        for (const auto& pair : journeys) {
            if (!pair.second.route.empty() && pair.second.route.back() == world) {
                score -= area.radius * 2;
            }
        }
        if (!selected_any || score > selected_score) {
            selected_state = best_state;
            selected_score = score;
            selected_any = true;
        }
    };
    for (int x = min_x; x <= max_x; ++x) {
        consider(Vector2i(x, min_y));
        consider(Vector2i(x, max_y));
    }
    for (int y = min_y + 1; y < max_y; ++y) {
        consider(Vector2i(min_x, y));
        consider(Vector2i(max_x, y));
    }
    if (!selected_any) return false;

    std::vector<Vector3i> reversed;
    uint64_t cursor = selected_state;
    while (true) {
        const Vector3i state = WorldCoords::unpack_coords_3d(cursor);
        reversed.push_back(Vector3i(state.x, state.y, p_start.z));
        auto parent = parents.find(cursor);
        if (parent == parents.end()) return false;
        if (parent->second == cursor) break;
        cursor = parent->second;
    }
    r_route.assign(reversed.rbegin(), reversed.rend());
    return r_route.size() >= 2;
}

bool CityPopulationDirector::build_directed_route_to_exit(
    const Vector3i& p_start,
    const Vector2i& p_center,
    int p_initial_direction,
    RoadLateralPreference p_lateral_preference,
    bool p_avoid_occupied,
    std::vector<Vector3i>& r_route
) const {
    if (!bubble
        || p_initial_direction < 0
        || p_initial_direction >= 4
        || !is_road_position(p_start)) {
        return false;
    }
    const CellArea area = population_route_area(bubble, p_center, p_start.z);
    if (!area.contains_world(p_start.x, p_start.y, p_start.z)) return false;

    using RouteNode = std::pair<int, uint64_t>;
    std::priority_queue<RouteNode, std::vector<RouteNode>, std::greater<RouteNode>> frontier;
    std::unordered_map<uint64_t, uint64_t> parents;
    std::unordered_map<uint64_t, int> costs;
    std::unordered_map<uint64_t, int> steps;
    std::unordered_map<uint64_t, bool> road_cache;
    auto is_route_road = [&](const Vector2i& p_position) {
        const uint64_t key = WorldCoords::pack_coords(p_position.x, p_position.y);
        auto cached = road_cache.find(key);
        if (cached != road_cache.end()) return cached->second;
        const bool road = is_road_position(
            Vector3i(p_position.x, p_position.y, p_start.z));
        road_cache[key] = road;
        return road;
    };

    const uint64_t start_key = WorldCoords::pack_coords_3d(
        p_start.x, p_start.y, p_initial_direction);
    frontier.emplace(0, start_key);
    parents[start_key] = start_key;
    costs[start_key] = 0;
    steps[start_key] = 0;

    uint64_t selected_state = 0;
    bool found_exit = false;
    while (!frontier.empty()) {
        const auto [current_cost, current_key] = frontier.top();
        frontier.pop();
        auto known_cost = costs.find(current_key);
        if (known_cost == costs.end() || current_cost != known_cost->second) continue;

        const Vector3i current_state = WorldCoords::unpack_coords_3d(current_key);
        const Vector2i current(current_state.x, current_state.y);
        const int previous_direction = current_state.z;
        if (steps[current_key] > 0
            && is_on_direction_boundary(current, area, p_initial_direction)
            && is_city_entry_cell(Vector3i(current.x, current.y, p_start.z))) {
            selected_state = current_key;
            found_exit = true;
            break;
        }

        for (int direction = 0; direction < 4; ++direction) {
            if (steps[current_key] == 0 && direction != p_initial_direction) continue;
            const Vector2i next = current + CARDINAL_DIRECTIONS[direction];
            if (!area.contains_world(next.x, next.y, p_start.z)
                || !is_route_road(next)) {
                continue;
            }
            const Vector3i next_world(next.x, next.y, p_start.z);
            if (p_avoid_occupied
                && tracker
                && tracker->get_at(next_world) != EntityPool::INVALID_ID) {
                continue;
            }
            const uint64_t key = WorldCoords::pack_coords_3d(
                next.x, next.y, direction);
            int move_cost = ROAD_STEP_COST
                + road_lateral_penalty(
                    next_world, direction, p_lateral_preference)
                    * ROAD_LATERAL_COST;
            if (direction != previous_direction) {
                move_cost += ROAD_TURN_COST;
                if ((direction + 2) % 4 == previous_direction) {
                    move_cost += ROAD_TURN_COST;
                }
            }
            const int next_cost = current_cost + move_cost;
            auto existing = costs.find(key);
            if (existing != costs.end() && existing->second <= next_cost) continue;
            costs[key] = next_cost;
            steps[key] = steps[current_key] + 1;
            parents[key] = current_key;
            frontier.emplace(next_cost, key);
        }
    }
    if (!found_exit) return false;

    std::vector<Vector3i> reversed;
    uint64_t cursor = selected_state;
    while (true) {
        const Vector3i state = WorldCoords::unpack_coords_3d(cursor);
        reversed.push_back(Vector3i(state.x, state.y, p_start.z));
        auto parent = parents.find(cursor);
        if (parent == parents.end()) return false;
        if (parent->second == cursor) break;
        cursor = parent->second;
    }
    r_route.assign(reversed.rbegin(), reversed.rend());
    return r_route.size() >= 2;
}

bool CityPopulationDirector::build_boundary_route(
    const Vector2i& p_center,
    int p_z,
    int64_t p_calendar_turn,
    uint64_t p_salt,
    RoadLateralPreference p_lateral_preference,
    const std::vector<Vector3i>& p_batch_entries,
    std::vector<Vector3i>& r_route
) const {
    if (!bubble || !world_seed) return false;
    const CellArea area = population_route_area(bubble, p_center, p_z);
    std::vector<Vector3i> entries;
    const int min_x = area.center.x - area.radius;
    const int min_y = area.center.y - area.radius;
    const int max_x = area.center.x + area.radius - 1;
    const int max_y = area.center.y + area.radius - 1;
    auto add = [&](int x, int y) {
        const Vector3i position(x, y, p_z);
        if (!is_gameplay_visible(position, p_center)
            && is_city_entry_cell(position)
            && entry_is_available(position, p_calendar_turn, p_batch_entries)
            && (!tracker || tracker->get_at(position) == EntityPool::INVALID_ID)) {
            entries.push_back(position);
        }
    };
    for (int x = min_x; x <= max_x; ++x) {
        add(x, min_y);
        add(x, max_y);
    }
    for (int y = min_y + 1; y < max_y; ++y) {
        add(min_x, y);
        add(max_x, y);
    }
    if (entries.empty()) return false;

    Rng::Seeded rng = Rng::at(
        static_cast<uint32_t>(*world_seed), p_center, Rng::SPAWN, p_salt);
    const int offset = rng.range(0, static_cast<int>(entries.size()) - 1);
    int selected_index = -1;
    int selected_score = std::numeric_limits<int>::max();
    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
        const int index = (offset + i) % entries.size();
        const Vector3i& entry = entries[index];
        int score = rng.range(0, 20);
        const uint64_t entry_key = WorldCoords::pack_coords_3d(
            entry.x, entry.y, entry.z);
        auto recent = entry_last_used.find(entry_key);
        if (recent != entry_last_used.end()) {
            const int64_t age = std::max<int64_t>(0, p_calendar_turn - recent->second);
            const int64_t reuse_window =
                static_cast<int64_t>(config.entry_cooldown_turns) * 4;
            score += static_cast<int>(
                std::max<int64_t>(0, reuse_window - age) * 25);
        }
        for (const auto& pair : journeys) {
            if (!pair.second.route.empty() && pair.second.route.back() == entry) {
                score += 100;
            }
        }
        score += road_lateral_penalty(
            entry,
            inward_direction_for_side(
                boundary_side(Vector2i(entry.x, entry.y), area)),
            p_lateral_preference) * 6;
        if (selected_index < 0 || score < selected_score) {
            selected_index = index;
            selected_score = score;
        }
    }
    if (selected_index < 0
        || !build_route_to_exit(
            entries[selected_index],
            p_center,
            nullptr,
            false,
            false,
            p_lateral_preference,
            r_route)) {
        return false;
    }
    for (const auto& pair : journeys) {
        if (routes_overlap_opening(r_route, pair.second.route)) {
            r_route.clear();
            return false;
        }
    }
    return r_route.size() >= 2;
}

bool CityPopulationDirector::spawn_route(
    std::vector<Vector3i>&& p_route,
    RoadLateralPreference p_lateral_preference,
    int64_t p_calendar_turn,
    uint64_t p_salt,
    int p_travel_direction
) {
    if (!bubble || !ledger || !tracker || !scheduler || !world_seed
        || p_route.size() < 2) {
        return false;
    }
    const Entity* player =
        ledger->get_entity_pool().get_entity(EntityPool::PLAYER_ID);
    if (!player || player->z != 0) return false;

    EntityGroupDb* groups = EntityGroupDb::get_singleton();
    if (!groups) return false;
    Rng::Seeded rng = Rng::at(
        static_cast<uint32_t>(*world_seed),
        Vector2i(p_route.front().x, p_route.front().y),
        Rng::SPAWN,
        p_salt
    );
    const EntityGroupEntry* entry =
        groups->pick_weighted_entry(config.entity_group, rng);
    if (!entry || entry->none || entry->entity.is_empty()) return false;

    EntityFactory::SpawnOverrides overrides;
    overrides.job = entry->job;
    overrides.dialogue_id = entry->dialogue_id;
    overrides.faction = entry->faction;
    overrides.reaction_policy = entry->reaction_policy;
    overrides.reaction_radius = entry->reaction_radius;
    overrides.ai_state = entry->ai_state;
    overrides.identity_salt = p_salt;
    const float initial_turn_time =
        player->next_turn_time + 0.1f + rng.unit() * 0.8f;
    const Vector3i spawn_position = p_route.front();
    const uint32_t id = EntityFactory::create_npc(
        entry->entity,
        Vector2i(spawn_position.x, spawn_position.y),
        *world_seed,
        *ledger,
        *tracker,
        *bubble,
        *scheduler,
        overrides,
        initial_turn_time
    );
    if (id == EntityPool::INVALID_ID) return false;

    ledger->mark_ambient(id);
    AmbientJourneyData journey;
    journey.route = std::move(p_route);
    journey.travel_direction = p_travel_direction;
    journey.lateral_preference = p_lateral_preference;
    journey.wants_detour =
        !config.detour_tags.empty() && rng.chance(config.detour_chance);
    journeys[id] = std::move(journey);
    entry_last_used[WorldCoords::pack_coords_3d(
        spawn_position.x, spawn_position.y, spawn_position.z)] = p_calendar_turn;
    return true;
}

bool CityPopulationDirector::spawn_ingress(
    const Vector2i& p_center,
    int64_t p_calendar_turn,
    std::vector<Vector3i>& r_batch_entries
) {
    if (!generator || !bubble || !ledger || !tracker || !scheduler || !world_seed) return false;
    const Entity* player = ledger->get_entity_pool().get_entity(EntityPool::PLAYER_ID);
    if (!player || player->z != 0) return false;

    const uint64_t salt = Rng::mix64(
        static_cast<uint64_t>(p_calendar_turn) ^ (++spawn_serial << 24));
    Rng::Seeded preference_rng = Rng::at(
        static_cast<uint32_t>(*world_seed),
        p_center,
        Rng::SPAWN,
        salt);
    const int preference_roll = preference_rng.range(0, 99);
    const RoadLateralPreference lateral_preference =
        preference_roll < 25
        ? RoadLateralPreference::LEFT
        : (preference_roll < 75
            ? RoadLateralPreference::CENTER
            : RoadLateralPreference::RIGHT);
    std::vector<Vector3i> route;
    if (!build_boundary_route(
            p_center,
            player->z,
            p_calendar_turn,
            salt,
            lateral_preference,
            r_batch_entries,
            route)) {
        return false;
    }
    const Vector3i spawn_position = route.front();
    const CellArea area =
        population_route_area(bubble, p_center, spawn_position.z);
    const int travel_direction =
        boundary_side(Vector2i(route.back().x, route.back().y), area);
    if (!spawn_route(
            std::move(route),
            lateral_preference,
            p_calendar_turn,
            salt,
            travel_direction)) {
        return false;
    }
    r_batch_entries.push_back(spawn_position);
    return true;
}

int CityPopulationDirector::process_exploration_activation(
    int p_target,
    int64_t p_calendar_turn,
    const Vector2i& p_player_position
) {
    if (pending_activation.cells.empty()) return 0;
    PendingActivationBatch batch = std::move(pending_activation);
    pending_activation = PendingActivationBatch();
    if (!generator || !bubble || !ledger || !tracker || !world_seed
        || batch.z != 0
        || static_cast<int>(journeys.size()) >= p_target) {
        return 0;
    }

    TagRegistry* tags = TagRegistry::get_singleton();
    TileDb* tile_db = TileDb::get_singleton();
    ChunkDb* chunk_db = ChunkDb::get_singleton();
    if (!tags || !tile_db || !chunk_db) return 0;
    const uint16_t road_tag = tags->get_tag_id("ROAD");
    const uint16_t entry_tag = tags->get_tag_id("CITY_AMBIENT_ENTRY");
    if (road_tag == 0 || entry_tag == 0) return 0;

    std::vector<ExplorationChunkBucket> buckets;
    std::unordered_map<uint64_t, size_t> bucket_indices;
    buckets.reserve(std::min<size_t>(
        batch.cells.size(), static_cast<size_t>(32)));
    for (uint64_t packed_cell : batch.cells) {
        const Vector3i position = WorldCoords::unpack_coords_3d(packed_cell);
        if (position.z != batch.z
            || tracker->get_at(position) != EntityPool::INVALID_ID) {
            continue;
        }
        const int chunk_x = floor_div_chunk(position.x);
        const int chunk_y = floor_div_chunk(position.y);
        const uint64_t chunk_key = WorldCoords::pack_coords(chunk_x, chunk_y);
        auto bucket_it = bucket_indices.find(chunk_key);
        if (bucket_it == bucket_indices.end()) {
            const uint32_t chunk_data = generator->get_biome_chunk_data(
                chunk_x, chunk_y, batch.z, *world_seed);
            const uint16_t chunk_id =
                static_cast<uint16_t>(chunk_data & WorldCoords::ID_MASK);
            const uint8_t direction_mask = static_cast<uint8_t>(
                (chunk_data >> WorldCoords::NEIGHBOR_SHIFT)
                & WorldCoords::NEIGHBOR_MASK);
            if (!chunk_db->has_tag(chunk_id, entry_tag)
                || direction_mask == 0) {
                bucket_indices[chunk_key] = std::numeric_limits<size_t>::max();
                continue;
            }
            const size_t index = buckets.size();
            bucket_indices[chunk_key] = index;
            buckets.push_back(
                ExplorationChunkBucket{chunk_key, direction_mask, {}});
            bucket_it = bucket_indices.find(chunk_key);
        }
        if (bucket_it->second == std::numeric_limits<size_t>::max()) continue;

        const uint16_t tile_id = bubble->query_tile_id_at_z(
            position.x, position.y, position.z);
        const TileInfo* tile = tile_db->get_tile_info(tile_id);
        if (!tile || tile->solid || !tile_db->has_tag(tile_id, road_tag)) continue;
        buckets[bucket_it->second].positions.push_back(position);
    }
    buckets.erase(
        std::remove_if(
            buckets.begin(),
            buckets.end(),
            [](const ExplorationChunkBucket& p_bucket) {
                return p_bucket.positions.empty();
            }),
        buckets.end()
    );
    if (buckets.empty()) return 0;

    const uint64_t batch_salt = Rng::mix64(
        static_cast<uint64_t>(p_calendar_turn)
        ^ WorldCoords::pack_coords(batch.center.x, batch.center.y)
        ^ (++spawn_serial << 20)
        ^ (batch.bulk ? UINT64_C(0xB071C5EED) : UINT64_C(0x0ED6E5EED)));
    Rng::Seeded rng = Rng::at(
        static_cast<uint32_t>(*world_seed),
        batch.center,
        Rng::SPAWN,
        batch_salt
    );

    int desired_spawns = 0;
    const int current_population = static_cast<int>(journeys.size());
    if (batch.bulk) {
        for (int slot = current_population; slot < p_target; ++slot) {
            if (rng.chance(BULK_EXPLORATION_SLOT_CHANCE)) {
                ++desired_spawns;
            }
        }
    } else if (rng.chance(config.exploration_spawn_chance)) {
        desired_spawns = 1;
    }
    if (desired_spawns <= 0) return 0;

    std::sort(
        buckets.begin(),
        buckets.end(),
        [](const ExplorationChunkBucket& p_a, const ExplorationChunkBucket& p_b) {
            return p_a.chunk_key < p_b.chunk_key;
        });
    seeded_shuffle(buckets, rng);
    for (ExplorationChunkBucket& bucket : buckets) {
        seeded_shuffle(bucket.positions, rng);
    }

    std::vector<ExplorationCandidate> candidates;
    size_t candidate_depth = 0;
    bool added_candidate = true;
    while (added_candidate) {
        added_candidate = false;
        for (const ExplorationChunkBucket& bucket : buckets) {
            if (candidate_depth >= bucket.positions.size()) continue;
            candidates.push_back(ExplorationCandidate{
                bucket.positions[candidate_depth],
                bucket.chunk_key,
                bucket.direction_mask
            });
            added_candidate = true;
        }
        ++candidate_depth;
    }

    std::vector<Vector3i> selected_entries;
    const int route_attempt_limit = batch.bulk
        ? BULK_EXPLORATION_ROUTE_ATTEMPTS
        : ORDINARY_EXPLORATION_ROUTE_ATTEMPTS;
    int route_attempts = 0;
    int spawned = 0;
    for (const ExplorationCandidate& candidate : candidates) {
        if (spawned >= desired_spawns
            || static_cast<int>(journeys.size()) >= p_target
            || route_attempts >= route_attempt_limit) {
            break;
        }
        if (tracker->get_at(candidate.position) != EntityPool::INVALID_ID) continue;

        bool separated = true;
        for (const Vector3i& entry : selected_entries) {
            if (chebyshev_distance(candidate.position, entry)
                < config.entry_min_distance) {
                separated = false;
                break;
            }
        }
        if (!separated) continue;
        for (const auto& pair : journeys) {
            if (!pair.second.route.empty()
                && chebyshev_distance(
                    candidate.position, pair.second.route.front())
                    < config.entry_min_distance) {
                separated = false;
                break;
            }
        }
        if (!separated) continue;

        std::vector<int> directions;
        for (int direction = 0; direction < 4; ++direction) {
            if ((candidate.direction_mask & (1 << direction)) != 0) {
                directions.push_back(direction);
            }
        }
        seeded_shuffle(directions, rng);
        for (int direction : directions) {
            if (route_attempts >= route_attempt_limit) break;
            ++route_attempts;
            const RoadLateralPreference lateral_preference =
                nearest_lateral_preference(candidate.position, direction);
            std::vector<Vector3i> route;
            if (!build_directed_route_to_exit(
                    candidate.position,
                    p_player_position,
                    direction,
                    lateral_preference,
                    false,
                    route)) {
                continue;
            }
            const uint64_t spawn_salt = Rng::mix64(
                batch_salt
                ^ WorldCoords::pack_coords_3d(
                    candidate.position.x,
                    candidate.position.y,
                    candidate.position.z)
                ^ (++spawn_serial << 24));
            if (!spawn_route(
                    std::move(route),
                    lateral_preference,
                    p_calendar_turn,
                    spawn_salt,
                    direction)) {
                continue;
            }
            selected_entries.push_back(candidate.position);
            ++spawned;
            break;
        }
    }
    return spawned;
}

void CityPopulationDirector::update(
    int64_t p_calendar_turn,
    bool p_is_day,
    const Vector2i& p_player_position
) {
    if (!ledger || !bubble) return;
    latest_calendar_turn = p_calendar_turn;
    latest_is_day = p_is_day;
    has_population_context = true;
    std::vector<uint32_t> stale;
    for (const auto& pair : journeys) {
        if (!ledger->is_alive(pair.first) || !ledger->is_ambient(pair.first)) {
            stale.push_back(pair.first);
        }
    }
    for (uint32_t id : stale) release_for_entity(id);

    const int target = p_is_day ? config.day_target : config.night_target;
    const bool initialize_ingress_timer =
        next_replenish_turn == std::numeric_limits<int64_t>::min();
    if (initialize_ingress_timer) {
        next_replenish_turn =
            p_calendar_turn + roll_replenish_delay(
                p_player_position, p_calendar_turn);
    }

    process_exploration_activation(
        target, p_calendar_turn, p_player_position);

    if (initialize_ingress_timer
        || static_cast<int>(journeys.size()) >= target
        || p_calendar_turn < next_replenish_turn) {
        return;
    }

    std::vector<Vector3i> batch_entries;
    spawn_ingress(p_player_position, p_calendar_turn, batch_entries);
    next_replenish_turn =
        p_calendar_turn + roll_replenish_delay(p_player_position, p_calendar_turn);
}

AmbientJourneyData* CityPopulationDirector::get_journey(uint32_t p_entity_id) {
    auto it = journeys.find(p_entity_id);
    return it == journeys.end() ? nullptr : &it->second;
}

const AmbientJourneyData* CityPopulationDirector::get_journey(uint32_t p_entity_id) const {
    auto it = journeys.find(p_entity_id);
    return it == journeys.end() ? nullptr : &it->second;
}

bool CityPopulationDirector::is_ambient(uint32_t p_entity_id) const {
    return ledger && ledger->is_ambient(p_entity_id)
        && journeys.find(p_entity_id) != journeys.end();
}

bool CityPopulationDirector::is_inside_route_area(
    const Vector3i& p_position,
    const Vector2i& p_center
) const {
    if (!bubble || p_position.z != bubble->get_active_z()) return false;
    return population_route_area(bubble, p_center, p_position.z).contains_world(
        p_position.x, p_position.y, p_position.z);
}

bool CityPopulationDirector::find_detour_approach(
    const Vector3i& p_start,
    const Vector3i& p_target,
    std::vector<Vector3i>& r_road_route,
    Vector3i& r_approach
) const {
    if (!bubble || !is_road_position(p_start) || p_start.z != p_target.z) {
        return false;
    }
    const Entity* player = ledger
        ? ledger->get_entity_pool().get_entity(EntityPool::PLAYER_ID)
        : nullptr;
    if (!player) return false;
    const CellArea area = population_route_area(
        bubble, Vector2i(player->x, player->y), p_start.z);
    if (!area.contains_world(p_start.x, p_start.y, p_start.z)) {
        return false;
    }

    const uint64_t start_key = WorldCoords::pack_coords(p_start.x, p_start.y);
    std::queue<Vector2i> frontier;
    std::unordered_map<uint64_t, uint64_t> parents;
    std::unordered_map<uint64_t, int> distances;
    frontier.push(Vector2i(p_start.x, p_start.y));
    parents[start_key] = start_key;
    distances[start_key] = 0;
    static const Vector2i directions[] = {
        Vector2i(0, -1), Vector2i(1, 0), Vector2i(0, 1), Vector2i(-1, 0)
    };
    while (!frontier.empty()) {
        const Vector2i current = frontier.front();
        frontier.pop();
        const uint64_t current_key = WorldCoords::pack_coords(current.x, current.y);
        for (const Vector2i& direction : directions) {
            const Vector2i next = current + direction;
            if (!area.contains_world(next.x, next.y, p_start.z)) continue;
            const uint64_t key = WorldCoords::pack_coords(next.x, next.y);
            if (parents.find(key) != parents.end()
                || !is_road_position(Vector3i(next.x, next.y, p_start.z))) {
                continue;
            }
            parents[key] = current_key;
            distances[key] = distances[current_key] + 1;
            frontier.push(next);
        }
    }

    bool found = false;
    uint64_t approach_key = 0;
    int best_score = std::numeric_limits<int>::max();
    for (int oy = -2; oy <= 2; ++oy) {
        for (int ox = -2; ox <= 2; ++ox) {
            const Vector3i candidate(p_target.x + ox, p_target.y + oy, p_target.z);
            if (chebyshev_distance(candidate, p_target) > 2
                || !area.contains_world(candidate.x, candidate.y, candidate.z)
                || !is_road_position(candidate)) {
                continue;
            }
            const uint64_t key = WorldCoords::pack_coords(candidate.x, candidate.y);
            auto distance = distances.find(key);
            if (distance == distances.end()) continue;
            const int score = distance->second * 4
                + chebyshev_distance(candidate, p_target);
            if (!found || score < best_score) {
                approach_key = key;
                r_approach = candidate;
                best_score = score;
                found = true;
            }
        }
    }
    if (!found) return false;

    std::vector<Vector3i> reversed;
    uint64_t cursor = approach_key;
    while (true) {
        const Vector2i position = WorldCoords::unpack_coords(cursor);
        reversed.push_back(Vector3i(position.x, position.y, p_start.z));
        if (cursor == start_key) break;
        cursor = parents[cursor];
    }
    r_road_route.assign(reversed.rbegin(), reversed.rend());
    return true;
}

bool CityPopulationDirector::find_alternate_road_step(
    uint32_t p_entity_id,
    const Vector3i& p_current,
    const Vector3i& p_goal,
    Vector3i& r_step
) const {
    static const Vector2i directions[] = {
        Vector2i(0, -1), Vector2i(1, 0), Vector2i(0, 1), Vector2i(-1, 0)
    };
    const int current_distance = manhattan_distance(p_current, p_goal);
    bool found = false;
    int best_score = std::numeric_limits<int>::max();
    for (const Vector2i& direction : directions) {
        const Vector3i candidate(
            p_current.x + direction.x,
            p_current.y + direction.y,
            p_current.z
        );
        if (!is_road_position(candidate)
            || is_road_cell_reserved_by_other(candidate, p_entity_id)) {
            continue;
        }
        const uint32_t occupant = tracker
            ? tracker->get_at(candidate)
            : EntityPool::INVALID_ID;
        if (occupant != EntityPool::INVALID_ID && occupant != p_entity_id) continue;
        const int distance = manhattan_distance(candidate, p_goal);
        if (distance > current_distance) continue;
        int score = distance * 10;
        for (const auto& pair : journeys) {
            if (pair.first == p_entity_id) continue;
            const AmbientJourneyData& journey = pair.second;
            const int end = std::min(
                static_cast<int>(journey.route.size()), journey.route_index + 5);
            for (int i = std::max(0, journey.route_index); i < end; ++i) {
                if (journey.route[i] == candidate) score += 5;
            }
        }
        if (!found || score < best_score) {
            r_step = candidate;
            best_score = score;
            found = true;
        }
    }
    return found;
}

bool CityPopulationDirector::try_reserve_road_cell(
    const Vector3i& p_position,
    uint32_t p_entity_id
) {
    if (!is_road_position(p_position)) return false;
    const uint64_t key = WorldCoords::pack_coords_3d(
        p_position.x, p_position.y, p_position.z);
    auto occupied = road_cell_reservations.find(key);
    if (occupied != road_cell_reservations.end() && occupied->second != p_entity_id) {
        return false;
    }
    release_road_reservation(p_entity_id);
    road_cell_reservations[key] = p_entity_id;
    entity_road_reservations[p_entity_id] = key;
    return true;
}

bool CityPopulationDirector::is_road_cell_reserved_by_other(
    const Vector3i& p_position,
    uint32_t p_entity_id
) const {
    const uint64_t key = WorldCoords::pack_coords_3d(
        p_position.x, p_position.y, p_position.z);
    auto it = road_cell_reservations.find(key);
    return it != road_cell_reservations.end() && it->second != p_entity_id;
}

void CityPopulationDirector::release_road_reservation(uint32_t p_entity_id) {
    auto it = entity_road_reservations.find(p_entity_id);
    if (it == entity_road_reservations.end()) return;
    road_cell_reservations.erase(it->second);
    entity_road_reservations.erase(it);
}

bool CityPopulationDirector::reroute_from(
    uint32_t p_entity_id,
    const Vector3i& p_start
) {
    AmbientJourneyData* journey = get_journey(p_entity_id);
    const Entity* player = ledger
        ? ledger->get_entity_pool().get_entity(EntityPool::PLAYER_ID)
        : nullptr;
    if (!journey || !player || !is_road_position(p_start)) return false;
    const Vector3i preferred = journey->route.empty() ? Vector3i() : journey->route.back();
    const Vector2i player_position(player->x, player->y);
    std::vector<Vector3i> route;
    bool used_directed_route = journey->travel_direction >= 0
        && build_directed_route_to_exit(
            p_start,
            player_position,
            journey->travel_direction,
            journey->lateral_preference,
            false,
            route);
    if (!used_directed_route && !build_route_to_exit(
            p_start,
            player_position,
            journey->route.empty() ? nullptr : &preferred,
            false,
            false,
            journey->lateral_preference,
            route)) {
        return false;
    }
    if (!used_directed_route) {
        const CellArea area =
            population_route_area(bubble, player_position, p_start.z);
        journey->travel_direction =
            boundary_side(Vector2i(route.back().x, route.back().y), area);
    }
    release_road_reservation(p_entity_id);
    journey->route = std::move(route);
    journey->route_index = 1;
    journey->blocked_turns = 0;
    return true;
}

bool CityPopulationDirector::reroute(uint32_t p_entity_id) {
    const Entity* entity = ledger
        ? ledger->get_entity_pool().get_entity(p_entity_id)
        : nullptr;
    return entity && reroute_from(
        p_entity_id, Vector3i(entity->x, entity->y, entity->z));
}

bool CityPopulationDirector::reroute_around_congestion(uint32_t p_entity_id) {
    AmbientJourneyData* journey = get_journey(p_entity_id);
    const Entity* entity = ledger
        ? ledger->get_entity_pool().get_entity(p_entity_id)
        : nullptr;
    const Entity* player = ledger
        ? ledger->get_entity_pool().get_entity(EntityPool::PLAYER_ID)
        : nullptr;
    if (!journey || !entity || !player) return false;
    const Vector3i current(entity->x, entity->y, entity->z);
    const Vector2i player_position(player->x, player->y);
    std::vector<Vector3i> route;
    bool used_directed_route = journey->travel_direction >= 0
        && build_directed_route_to_exit(
            current,
            player_position,
            journey->travel_direction,
            journey->lateral_preference,
            true,
            route);
    if (!used_directed_route && !build_route_to_exit(
            current,
            player_position,
            nullptr,
            true,
            true,
            journey->lateral_preference,
            route)) {
        return false;
    }
    if (!used_directed_route) {
        const CellArea area =
            population_route_area(bubble, player_position, current.z);
        journey->travel_direction =
            boundary_side(Vector2i(route.back().x, route.back().y), area);
    }
    release_road_reservation(p_entity_id);
    journey->route = std::move(route);
    journey->route_index = 1;
    journey->blocked_turns = 0;
    return true;
}

bool CityPopulationDirector::continue_route(uint32_t p_entity_id) {
    AmbientJourneyData* journey = get_journey(p_entity_id);
    const Entity* entity = ledger
        ? ledger->get_entity_pool().get_entity(p_entity_id)
        : nullptr;
    const Entity* player = ledger
        ? ledger->get_entity_pool().get_entity(EntityPool::PLAYER_ID)
        : nullptr;
    if (!journey || !entity || !player
        || entity->z != player->z
        || journey->route.size() < 2) {
        return false;
    }

    const Vector3i current(entity->x, entity->y, entity->z);
    const Vector2i player_position(player->x, player->y);
    const CellArea current_area =
        population_route_area(bubble, player_position, current.z);
    if (!current_area.contains_world(current.x, current.y, current.z)
        || is_on_boundary(Vector2i(current.x, current.y), current_area)
        || !is_road_position(current)) {
        return false;
    }

    const int direction = journey->travel_direction >= 0
        ? journey->travel_direction
        : direction_toward(
            journey->route[journey->route.size() - 2],
            journey->route.back());
    std::vector<Vector3i> route;
    if (!build_directed_route_to_exit(
            current,
            player_position,
            direction,
            journey->lateral_preference,
            false,
            route)
        && !build_route_to_exit(
            current,
            player_position,
            nullptr,
            true,
            false,
            journey->lateral_preference,
            route)) {
        return false;
    }

    release_road_reservation(p_entity_id);
    journey->route = std::move(route);
    journey->travel_direction = direction;
    journey->route_index = 1;
    journey->blocked_turns = 0;
    return true;
}

void CityPopulationDirector::cancel_detour(uint32_t p_entity_id) {
    AmbientJourneyData* journey = get_journey(p_entity_id);
    if (!journey) return;
    if (poi_registry) poi_registry->release_for_entity(p_entity_id);
    release_road_reservation(p_entity_id);
    const Entity* entity = ledger
        ? ledger->get_entity_pool().get_entity(p_entity_id)
        : nullptr;
    int current_final_index = -1;
    if (entity && !is_road_position(Vector3i(entity->x, entity->y, entity->z))) {
        for (int i = 0; i < static_cast<int>(journey->detour_final_path.size()); ++i) {
            if (journey->detour_final_path[i]
                == Vector3i(entity->x, entity->y, entity->z)) {
                current_final_index = i;
                break;
            }
        }
    }
    journey->phase = current_final_index > 0
        ? AmbientJourneyPhase::RETURNING_TO_ROAD
        : AmbientJourneyPhase::FOLLOWING_ROUTE;
    journey->detour_target = Vector3i();
    journey->detour_road_route.clear();
    journey->detour_road_index = 1;
    journey->detour_final_index = 1;
    journey->return_final_index = current_final_index - 1;
    if (journey->phase == AmbientJourneyPhase::FOLLOWING_ROUTE) {
        journey->detour_final_path.clear();
        journey->return_final_index = -1;
    }
    journey->dwell_remaining = 0;
    journey->blocked_turns = 0;
    if (LocomotionData* loco = ledger->try_get_locomotion(p_entity_id)) {
        Locomotion::clear_path(*loco);
    }
}

void CityPopulationDirector::release_for_entity(uint32_t p_entity_id) {
    if (poi_registry) poi_registry->release_for_entity(p_entity_id);
    release_road_reservation(p_entity_id);
    journeys.erase(p_entity_id);
}

void CityPopulationDirector::despawn_ambient(uint32_t p_entity_id) {
    if (!ledger || !ledger->is_ambient(p_entity_id)) return;
    const AmbientJourneyData* journey = get_journey(p_entity_id);
    if (journey && !journey->route.empty()) {
        const Vector3i& entry = journey->route.front();
        entry_last_used[WorldCoords::pack_coords_3d(
            entry.x, entry.y, entry.z)] = latest_calendar_turn;
    }
    release_for_entity(p_entity_id);
    EntityLifecycle::despawn_entity(
        p_entity_id,
        *ledger,
        *tracker,
        *bubble,
        *scheduler,
        world_seed ? static_cast<uint32_t>(*world_seed) : 0,
        false,
        poi_registry
    );
}

bool CityPopulationDirector::promote(uint32_t p_entity_id) {
    if (!ledger || !ledger->is_ambient(p_entity_id)) return false;
    release_for_entity(p_entity_id);
    ledger->promote_to_persistent(p_entity_id);

    AIData* ai = ledger->try_get_ai(p_entity_id);
    const Entity* entity = ledger->get_entity_pool().get_entity(p_entity_id);
    if (ai) {
        ai->routine_has_target = false;
        ai->routine_phase = RoutinePhase::SEEKING;
        ai->routine_dwell_remaining = 0;
        ai->routine_failed_attempts = 0;
        ai->home_state = AIState::WANDER;
        if (entity) ai->home_position = Vector2i(entity->x, entity->y);
    }
    if (LocomotionData* loco = ledger->try_get_locomotion(p_entity_id)) {
        Locomotion::clear_path(*loco);
    }
    return true;
}
