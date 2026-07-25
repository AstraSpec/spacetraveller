#include "city_population_director.h"

#include "components/ai_controller.h"
#include "components/locomotion.h"
#include "components/perception.h"
#include "core/rng.h"
#include "core/tag_registry.h"
#include "core/world_coords.h"
#include "data/chunk_db.h"
#include "data/entity_group_db.h"
#include "data/structure_db.h"
#include "data/tile_db.h"
#include "entities/entity_factory.h"
#include "entities/entity_ledger.h"
#include "entities/entity_pool.h"
#include "entities/entity_tracker.h"
#include "entity_lifecycle.h"
#include "light_level.h"
#include "point_of_interest_registry.h"
#include "turn_scheduler.h"
#include "traversal_rules.h"
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

Vector2i rotate_structure_local(
    const Vector2i& p_position,
    uint8_t p_rotation
) {
    const int max_coord = WorldCoords::CHUNK_SIZE - 1;
    switch (p_rotation) {
        case WorldCoords::ROT_WEST:
            return Vector2i(max_coord - p_position.y, p_position.x);
        case WorldCoords::ROT_NORTH:
            return Vector2i(
                max_coord - p_position.x,
                max_coord - p_position.y);
        case WorldCoords::ROT_EAST:
            return Vector2i(p_position.y, max_coord - p_position.x);
        case WorldCoords::ROT_SOUTH:
        default:
            return p_position;
    }
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

bool chunk_intersects_area(const Vector3i& p_chunk, const CellArea& p_area) {
    if (p_chunk.z != p_area.z) return false;
    const int chunk_min_x = p_chunk.x * WorldCoords::CHUNK_SIZE;
    const int chunk_min_y = p_chunk.y * WorldCoords::CHUNK_SIZE;
    const int chunk_max_x = chunk_min_x + WorldCoords::CHUNK_SIZE - 1;
    const int chunk_max_y = chunk_min_y + WorldCoords::CHUNK_SIZE - 1;
    const int area_min_x = p_area.center.x - p_area.radius;
    const int area_min_y = p_area.center.y - p_area.radius;
    const int area_max_x = p_area.center.x + p_area.radius - 1;
    const int area_max_y = p_area.center.y + p_area.radius - 1;
    return chunk_max_x >= area_min_x && chunk_min_x <= area_max_x
        && chunk_max_y >= area_min_y && chunk_min_y <= area_max_y;
}

uint32_t chunk_coverage_signature(
    const Vector3i& p_chunk,
    const CellArea& p_area
) {
    if (!chunk_intersects_area(p_chunk, p_area)) {
        return std::numeric_limits<uint32_t>::max();
    }
    const int chunk_min_x = p_chunk.x * WorldCoords::CHUNK_SIZE;
    const int chunk_min_y = p_chunk.y * WorldCoords::CHUNK_SIZE;
    const int chunk_max_x = chunk_min_x + WorldCoords::CHUNK_SIZE - 1;
    const int chunk_max_y = chunk_min_y + WorldCoords::CHUNK_SIZE - 1;
    const int area_min_x = p_area.center.x - p_area.radius;
    const int area_min_y = p_area.center.y - p_area.radius;
    const int area_max_x = p_area.center.x + p_area.radius - 1;
    const int area_max_y = p_area.center.y + p_area.radius - 1;
    const uint32_t min_x = static_cast<uint32_t>(
        std::max(chunk_min_x, area_min_x) - chunk_min_x);
    const uint32_t min_y = static_cast<uint32_t>(
        std::max(chunk_min_y, area_min_y) - chunk_min_y);
    const uint32_t max_x = static_cast<uint32_t>(
        std::min(chunk_max_x, area_max_x) - chunk_min_x);
    const uint32_t max_y = static_cast<uint32_t>(
        std::min(chunk_max_y, area_max_y) - chunk_min_y);
    return min_x | (min_y << 5) | (max_x << 10) | (max_y << 15);
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
    config.venue_initial_spawn_chance = std::clamp(
        static_cast<float>(row.get(
            "venue_initial_spawn_chance", config.venue_initial_spawn_chance)),
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
    active_venues.clear();
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
    if (pending_activation.bulk && has_population_context) {
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

uint32_t CityPopulationDirector::create_ambient_at(
    const Vector3i& p_position,
    uint64_t p_salt
) {
    if (!bubble || !ledger || !tracker || !scheduler || !world_seed) {
        return EntityPool::INVALID_ID;
    }
    const Entity* player =
        ledger->get_entity_pool().get_entity(EntityPool::PLAYER_ID);
    if (!player || player->z != p_position.z
        || tracker->get_at(p_position) != EntityPool::INVALID_ID) {
        return EntityPool::INVALID_ID;
    }

    EntityGroupDb* groups = EntityGroupDb::get_singleton();
    if (!groups) return EntityPool::INVALID_ID;
    Rng::Seeded rng = Rng::at(
        static_cast<uint32_t>(*world_seed),
        Vector2i(p_position.x, p_position.y),
        Rng::SPAWN,
        p_salt
    );
    const EntityGroupEntry* entry =
        groups->pick_weighted_entry(config.entity_group, rng);
    if (!entry || entry->none || entry->entity.is_empty()) {
        return EntityPool::INVALID_ID;
    }

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
    const uint32_t id = EntityFactory::create_npc(
        entry->entity,
        Vector2i(p_position.x, p_position.y),
        *world_seed,
        *ledger,
        *tracker,
        *bubble,
        *scheduler,
        overrides,
        initial_turn_time
    );
    if (id == EntityPool::INVALID_ID) return id;

    ledger->mark_ambient(id);
    return id;
}

bool CityPopulationDirector::spawn_route(
    std::vector<Vector3i>&& p_route,
    RoadLateralPreference p_lateral_preference,
    int64_t p_calendar_turn,
    uint64_t p_salt,
    int p_travel_direction,
    uint32_t* r_entity_id
) {
    if (r_entity_id) *r_entity_id = EntityPool::INVALID_ID;
    if (p_route.size() < 2) return false;
    const Vector3i spawn_position = p_route.front();
    const uint32_t id =
        create_ambient_at(spawn_position, p_salt);
    if (id == EntityPool::INVALID_ID) return false;

    Rng::Seeded rng = Rng::at(
        static_cast<uint32_t>(*world_seed),
        Vector2i(spawn_position.x, spawn_position.y),
        Rng::SPAWN,
        Rng::mix64(p_salt ^ UINT64_C(0xD370A2)));
    AmbientJourneyData journey;
    journey.route = std::move(p_route);
    journey.travel_direction = p_travel_direction;
    journey.lateral_preference = p_lateral_preference;
    journey.wants_detour =
        !config.detour_tags.empty() && rng.chance(config.detour_chance);
    journeys[id] = std::move(journey);
    entry_last_used[WorldCoords::pack_coords_3d(
        spawn_position.x, spawn_position.y, spawn_position.z)] = p_calendar_turn;
    if (r_entity_id) *r_entity_id = id;
    return true;
}

bool CityPopulationDirector::spawn_ingress(
    const Vector2i& p_center,
    int64_t p_calendar_turn,
    std::vector<Vector3i>& r_batch_entries,
    uint32_t* r_entity_id
) {
    if (r_entity_id) *r_entity_id = EntityPool::INVALID_ID;
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
             travel_direction,
             r_entity_id)) {
        return false;
    }
    r_batch_entries.push_back(spawn_position);
    return true;
}

int CityPopulationDirector::street_population_count() const {
    int count = 0;
    for (const auto& pair : journeys) {
        if (pair.second.venue_key == NO_AMBIENT_VENUE) ++count;
    }
    return count;
}

bool CityPopulationDirector::entrance_approach_less(
    const VenueEntranceApproach& p_a,
    const VenueEntranceApproach& p_b
) {
    const uint64_t a_entrance = WorldCoords::pack_coords_3d(
        p_a.entrance.x, p_a.entrance.y, p_a.entrance.z);
    const uint64_t b_entrance = WorldCoords::pack_coords_3d(
        p_b.entrance.x, p_b.entrance.y, p_b.entrance.z);
    if (a_entrance != b_entrance) return a_entrance < b_entrance;
    return WorldCoords::pack_coords_3d(
               p_a.road_access.x, p_a.road_access.y, p_a.road_access.z)
        < WorldCoords::pack_coords_3d(
               p_b.road_access.x, p_b.road_access.y, p_b.road_access.z);
}

bool CityPopulationDirector::entrance_approach_matches(
    const VenueEntranceApproach& p_approach,
    const Vector3i& p_entrance,
    const Vector3i& p_road_access
) {
    return p_approach.entrance == p_entrance
        && p_approach.road_access == p_road_access;
}

void CityPopulationDirector::update_admission_readiness(
    ActiveVenue& r_venue
) {
    r_venue.admission_ready =
        r_venue.has_activity && !r_venue.entrance_approaches.empty();
}

bool CityPopulationDirector::venue_has_vacancy(
    const ActiveVenue& p_venue
) {
    return p_venue.admission_ready
        && static_cast<int>(p_venue.visitors.size()) < p_venue.capacity;
}

void CityPopulationDirector::refresh_venue_entrance_approaches(
    ActiveVenue& r_venue
) {
    r_venue.entrance_approaches.clear();
    if (!generator || !world_seed || !bubble) {
        r_venue.admission_ready = false;
        return;
    }
    const int min_x = r_venue.chunk.x * WorldCoords::CHUNK_SIZE;
    const int min_y = r_venue.chunk.y * WorldCoords::CHUNK_SIZE;
    StructureDb* structure_db = StructureDb::get_singleton();
    if (structure_db) {
        const uint32_t packed_chunk = generator->get_biome_chunk_data(
            r_venue.chunk.x,
            r_venue.chunk.y,
            r_venue.chunk.z,
            *world_seed);
        const uint8_t rotation = static_cast<uint8_t>(
            (packed_chunk >> WorldCoords::ORIENTATION_SHIFT)
            & WorldCoords::ROTATION_MASK);
        const String structure_id = generator->get_structure_id_for_cell(
            min_x, min_y, r_venue.chunk.z, *world_seed);
        const StructureInfo* structure =
            structure_db->get_structure_info(structure_id);
        if (structure) {
            for (const int entrance_x : structure->entrances) {
                if (entrance_x < 0
                    || entrance_x >= WorldCoords::CHUNK_SIZE) {
                    continue;
                }
                const Vector2i local = rotate_structure_local(
                    Vector2i(
                        entrance_x,
                        WorldCoords::CHUNK_SIZE - 1),
                    rotation);
                const Vector3i entrance(
                    min_x + local.x,
                    min_y + local.y,
                    r_venue.chunk.z);
                for (int offset_y = -2; offset_y <= 2; ++offset_y) {
                    for (int offset_x = -2; offset_x <= 2; ++offset_x) {
                        const Vector3i road_access(
                            entrance.x + offset_x,
                            entrance.y + offset_y,
                            entrance.z);
                        if (!is_road_position(road_access)) continue;
                        const bool duplicate = std::any_of(
                            r_venue.entrance_approaches.begin(),
                            r_venue.entrance_approaches.end(),
                            [&](const VenueEntranceApproach& approach) {
                                return entrance_approach_matches(
                                    approach, entrance, road_access);
                            });
                        if (!duplicate) {
                            r_venue.entrance_approaches.push_back(
                                VenueEntranceApproach{entrance, road_access});
                        }
                    }
                }
            }
        }
    }
    std::sort(
        r_venue.entrance_approaches.begin(),
        r_venue.entrance_approaches.end(),
        entrance_approach_less);
    update_admission_readiness(r_venue);
}

void CityPopulationDirector::refresh_venue_activities(
    ActiveVenue& r_venue,
    uint32_t p_coverage_signature
) {
    r_venue.initial_spawn_points.clear();
    r_venue.has_activity = false;
    r_venue.coverage_signature = p_coverage_signature;
    if (!poi_registry) {
        r_venue.admission_ready = false;
        return;
    }
    const std::vector<Vector3i> positions =
        poi_registry->find_in_chunk(r_venue.chunk);
    r_venue.has_activity = !positions.empty();
    for (const Vector3i& position : positions) {
        const PointOfInterestInfo* point = poi_registry->get(position);
        if (point && point->initial_visitor_spawn) {
            r_venue.initial_spawn_points.push_back(position);
        }
    }
    std::sort(
        r_venue.initial_spawn_points.begin(),
        r_venue.initial_spawn_points.end(),
        [](const Vector3i& a, const Vector3i& b) {
            return WorldCoords::pack_coords_3d(a.x, a.y, a.z)
                < WorldCoords::pack_coords_3d(b.x, b.y, b.z);
        });
    update_admission_readiness(r_venue);
}

bool CityPopulationDirector::select_venue_activity(
    uint32_t p_entity_id,
    ActiveVenue& p_venue,
    const Vector3i& p_avoid,
    bool p_has_avoid,
    Vector3i& r_activity
) {
    if (!ledger || !tracker || !poi_registry || !world_seed) return false;
    const SocialProfileData* profile =
        ledger->try_get_social_profile(p_entity_id);
    const Entity* entity =
        ledger->get_entity_pool().get_entity(p_entity_id);
    if (!profile || !entity) return false;
    std::vector<Vector3i> candidates =
        poi_registry->find_compatible_in_chunk(
            p_venue.chunk,
            profile->context_tags,
            p_avoid,
            p_has_avoid);
    candidates.erase(
        std::remove_if(
            candidates.begin(),
            candidates.end(),
            [&](const Vector3i& position) {
                return !venue_activity_is_usable(p_entity_id, position);
            }),
        candidates.end());
    if (candidates.empty()) return false;

    Rng::Seeded rng = Rng::at(
        static_cast<uint32_t>(*world_seed),
        Vector2i(entity->x, entity->y),
        Rng::SPAWN,
        Rng::mix64(
            static_cast<uint64_t>(latest_calendar_turn)
            ^ (static_cast<uint64_t>(p_entity_id) << 17)
            ^ (++spawn_serial << 29)));
    return poi_registry->try_reserve_weighted(
        std::move(candidates), p_entity_id, rng.unit(), r_activity);
}

bool CityPopulationDirector::venue_activity_is_usable(
    uint32_t p_entity_id,
    const Vector3i& p_position
) const {
    if (!bubble || !ledger || !tracker || !poi_registry
        || !poi_registry->get(p_position)) {
        return false;
    }
    TileDb* tile_db = TileDb::get_singleton();
    if (!tile_db) return false;
    const uint16_t tile_id = bubble->query_tile_id_at_z(
        p_position.x, p_position.y, p_position.z);
    if (!TraversalRules::can_enter_or_open(
            p_entity_id, tile_id, *ledger)) {
        return false;
    }
    const uint32_t occupant = tracker->get_at(p_position);
    return occupant == EntityPool::INVALID_ID || occupant == p_entity_id;
}

bool CityPopulationDirector::seed_venue_visitors(
    uint64_t p_venue_key,
    ActiveVenue& r_venue
) {
    if (!r_venue.admission_ready || !world_seed || !ledger || !tracker
        || !poi_registry) {
        return false;
    }

    bool spawned_any = false;
    for (const Vector3i& position : r_venue.initial_spawn_points) {
        const uint64_t point_key =
            WorldCoords::pack_coords_3d(position.x, position.y, position.z);
        if (r_venue.rolled_initial_points.find(point_key)
            != r_venue.rolled_initial_points.end()) {
            continue;
        }
        r_venue.rolled_initial_points.insert(point_key);
        if (!venue_has_vacancy(r_venue)) break;

        const uint64_t salt = Rng::mix64(
            p_venue_key
            ^ static_cast<uint64_t>(r_venue.activation_turn)
            ^ point_key
            ^ UINT64_C(0x51EED));
        Rng::Seeded rng = Rng::at(
            static_cast<uint32_t>(*world_seed),
            Vector2i(position.x, position.y),
            Rng::SPAWN,
            salt);
        if (!rng.chance(config.venue_initial_spawn_chance)) continue;

        const PointOfInterestInfo* point = poi_registry->get(position);
        TileDb* tile_db = TileDb::get_singleton();
        const TileInfo* tile = tile_db && bubble
            ? tile_db->get_tile_info(
                bubble->query_tile_id_at_z(position.x, position.y, position.z))
            : nullptr;
        if (!point || !tile || tile->solid
            || poi_registry->is_reserved(position)
            || tracker->get_at(position) != EntityPool::INVALID_ID) {
            continue;
        }

        const uint32_t id = create_ambient_at(position, salt);
        if (id == EntityPool::INVALID_ID) continue;
        if (!venue_activity_is_usable(id, position)
            || !poi_registry->try_reserve(position, id)) {
            despawn_ambient(id);
            continue;
        }

        AmbientJourneyData journey;
        journey.phase = AmbientJourneyPhase::VENUE_ACTIVITY;
        journey.venue_key = p_venue_key;
        journey.venue_entry_activity = position;
        journey.venue_leave_at_turn = latest_calendar_turn
            + rng.range(r_venue.visit_min, r_venue.visit_max);
        journey.detour_attempted = true;
        journey.wants_detour = false;
        journeys[id] = std::move(journey);
        r_venue.visitors.insert(id);
        if (!begin_venue_activity(id)) {
            r_venue.visitors.erase(id);
            despawn_ambient(id);
            continue;
        }
        spawned_any = true;
    }
    return spawned_any;
}

void CityPopulationDirector::deactivate_venue(uint64_t p_venue_key) {
    auto venue_it = active_venues.find(p_venue_key);
    if (venue_it == active_venues.end()) return;
    std::vector<uint32_t> visitors(
        venue_it->second.visitors.begin(), venue_it->second.visitors.end());
    std::sort(visitors.begin(), visitors.end());
    for (uint32_t id : visitors) {
        AmbientJourneyData* journey = get_journey(id);
        const Entity* entity = ledger
            ? ledger->get_entity_pool().get_entity(id)
            : nullptr;
        if (!journey || !entity) {
            release_for_entity(id);
            continue;
        }
        if (journey->phase == AmbientJourneyPhase::APPROACHING_VENUE
            && is_road_position(Vector3i(entity->x, entity->y, entity->z))) {
            clear_venue_assignment(id, true);
        } else {
            despawn_ambient(id);
        }
    }
    active_venues.erase(p_venue_key);
}

void CityPopulationDirector::update_venues_from_activation(
    const PendingActivationBatch& p_batch,
    int64_t p_calendar_turn,
    const Vector2i& p_player_position
) {
    if (!generator || !world_seed || !bubble || !poi_registry) return;
    ChunkDb* chunk_db = ChunkDb::get_singleton();
    if (!chunk_db) return;

    std::unordered_set<uint64_t> discovered_chunks;
    for (const uint64_t packed : p_batch.cells) {
        const Vector3i position = WorldCoords::unpack_coords_3d(packed);
        const Vector3i chunk(
            WorldCoords::chunk_coord(position.x),
            WorldCoords::chunk_coord(position.y),
            position.z);
        const uint64_t key =
            WorldCoords::pack_coords_3d(chunk.x, chunk.y, chunk.z);
        if (!discovered_chunks.insert(key).second) continue;
        const uint32_t chunk_data = generator->get_biome_chunk_data(
            chunk.x, chunk.y, chunk.z, *world_seed);
        const ChunkInfo* info =
            chunk_db->get_chunk_info(static_cast<uint16_t>(
                chunk_data & WorldCoords::ID_MASK));
        if (!info || info->venue_visitor_capacity <= 0) continue;
        auto inserted = active_venues.emplace(key, ActiveVenue());
        ActiveVenue& venue = inserted.first->second;
        if (inserted.second) {
            venue.chunk = chunk;
            venue.capacity = info->venue_visitor_capacity;
            venue.visit_min = info->venue_visit_min;
            venue.visit_max = info->venue_visit_max;
            venue.activation_turn = p_calendar_turn;
            refresh_venue_entrance_approaches(venue);
        }
    }

    const CellArea active_area = bubble->get_active_area(p_player_position);
    std::vector<uint64_t> deactivated;
    for (const auto& pair : active_venues) {
        if (!chunk_intersects_area(pair.second.chunk, active_area)) {
            deactivated.push_back(pair.first);
        }
    }
    for (const uint64_t key : deactivated) deactivate_venue(key);

    std::vector<uint64_t> keys;
    keys.reserve(active_venues.size());
    for (const auto& pair : active_venues) keys.push_back(pair.first);
    std::sort(keys.begin(), keys.end());
    for (const uint64_t key : keys) {
        auto venue = active_venues.find(key);
        if (venue == active_venues.end()) continue;
        const uint32_t coverage =
            chunk_coverage_signature(venue->second.chunk, active_area);
        if (coverage == venue->second.coverage_signature) continue;
        refresh_venue_activities(venue->second, coverage);
        seed_venue_visitors(key, venue->second);
    }
}

int CityPopulationDirector::process_exploration_activation(
    int p_target,
    int64_t p_calendar_turn,
    const Vector2i& p_player_position
) {
    if (pending_activation.cells.empty()) return 0;
    PendingActivationBatch batch = std::move(pending_activation);
    pending_activation = PendingActivationBatch();
    update_venues_from_activation(
        batch, p_calendar_turn, p_player_position);
    if (!generator || !bubble || !ledger || !tracker || !world_seed
        || batch.z != 0
        || street_population_count() >= p_target) {
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
        const int chunk_x = WorldCoords::chunk_coord(position.x);
        const int chunk_y = WorldCoords::chunk_coord(position.y);
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
    const int current_population = street_population_count();
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
            || street_population_count() >= p_target
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

bool CityPopulationDirector::has_venue_vacancy() const {
    for (const auto& pair : active_venues) {
        if (venue_has_vacancy(pair.second)) return true;
    }
    return false;
}

bool CityPopulationDirector::build_road_route_to_any(
    const Vector3i& p_start,
    const std::vector<Vector3i>& p_targets,
    std::vector<Vector3i>& r_route,
    Vector3i& r_selected_target
) const {
    r_route.clear();
    if (!bubble || !ledger || !is_road_position(p_start)
        || p_targets.empty()) {
        return false;
    }
    const Entity* player =
        ledger->get_entity_pool().get_entity(EntityPool::PLAYER_ID);
    if (!player || player->z != p_start.z) return false;
    const CellArea area = population_route_area(
        bubble, Vector2i(player->x, player->y), p_start.z);
    if (!area.contains_world(p_start.x, p_start.y, p_start.z)) return false;

    std::unordered_set<uint64_t> target_keys;
    for (const Vector3i& target : p_targets) {
        if (target.z == p_start.z
            && area.contains_world(target.x, target.y, target.z)
            && is_road_position(target)) {
            target_keys.insert(WorldCoords::pack_coords(target.x, target.y));
        }
    }
    if (target_keys.empty()) return false;

    const uint64_t start_key =
        WorldCoords::pack_coords(p_start.x, p_start.y);
    std::queue<Vector2i> frontier;
    std::unordered_map<uint64_t, uint64_t> parents;
    frontier.push(Vector2i(p_start.x, p_start.y));
    parents[start_key] = start_key;
    uint64_t selected_key = 0;
    bool found = false;
    while (!frontier.empty()) {
        const Vector2i current = frontier.front();
        frontier.pop();
        const uint64_t current_key =
            WorldCoords::pack_coords(current.x, current.y);
        if (target_keys.find(current_key) != target_keys.end()) {
            selected_key = current_key;
            r_selected_target =
                Vector3i(current.x, current.y, p_start.z);
            found = true;
            break;
        }
        for (const Vector2i& direction : CARDINAL_DIRECTIONS) {
            const Vector2i next = current + direction;
            if (!area.contains_world(next.x, next.y, p_start.z)) continue;
            const uint64_t next_key =
                WorldCoords::pack_coords(next.x, next.y);
            if (parents.find(next_key) != parents.end()
                || !is_road_position(Vector3i(
                    next.x, next.y, p_start.z))) {
                continue;
            }
            parents[next_key] = current_key;
            frontier.push(next);
        }
    }
    if (!found) return false;

    std::vector<Vector3i> reversed;
    uint64_t cursor = selected_key;
    while (true) {
        const Vector2i position = WorldCoords::unpack_coords(cursor);
        reversed.push_back(Vector3i(position.x, position.y, p_start.z));
        if (cursor == start_key) break;
        auto parent = parents.find(cursor);
        if (parent == parents.end()) return false;
        cursor = parent->second;
    }
    r_route.assign(reversed.rbegin(), reversed.rend());
    return !r_route.empty();
}

bool CityPopulationDirector::build_road_route_to_venue(
    const Vector3i& p_start,
    ActiveVenue& r_venue,
    const Vector3i* p_excluded_entrance,
    const Vector3i* p_excluded_access,
    std::vector<Vector3i>& r_route,
    VenueEntranceApproach& r_approach
) {
    r_route.clear();
    if (!bubble || !ledger || !is_road_position(p_start)) return false;
    if (r_venue.entrance_approaches.empty()) return false;

    std::vector<VenueEntranceApproach> candidates;
    for (const VenueEntranceApproach& approach :
         r_venue.entrance_approaches) {
        if (approach.entrance.z != p_start.z
            || !is_road_position(approach.road_access)) {
            continue;
        }
        if (p_excluded_entrance && p_excluded_access
            && entrance_approach_matches(
                approach, *p_excluded_entrance, *p_excluded_access)) {
            continue;
        }
        candidates.push_back(approach);
    }
    if (candidates.empty()) return false;
    std::sort(
        candidates.begin(),
        candidates.end(),
        [&](const VenueEntranceApproach& a,
            const VenueEntranceApproach& b) {
            const int a_distance =
                manhattan_distance(p_start, a.road_access);
            const int b_distance =
                manhattan_distance(p_start, b.road_access);
            if (a_distance != b_distance) return a_distance < b_distance;
            const int a_entry_distance =
                manhattan_distance(a.road_access, a.entrance);
            const int b_entry_distance =
                manhattan_distance(b.road_access, b.entrance);
            if (a_entry_distance != b_entry_distance) {
                return a_entry_distance < b_entry_distance;
            }
            return entrance_approach_less(a, b);
        });
    std::vector<Vector3i> targets;
    targets.reserve(candidates.size());
    for (const VenueEntranceApproach& candidate : candidates) {
        targets.push_back(candidate.road_access);
    }
    Vector3i selected_access;
    if (!build_road_route_to_any(
            p_start, targets, r_route, selected_access)) {
        return false;
    }
    auto selected = std::find_if(
        candidates.begin(),
        candidates.end(),
        [&](const VenueEntranceApproach& approach) {
            return approach.road_access == selected_access;
        });
    if (selected == candidates.end()) return false;
    r_approach = *selected;
    return true;
}

bool CityPopulationDirector::select_departure_approach(
    ActiveVenue& r_venue,
    const Vector3i& p_from,
    const Vector3i* p_excluded_entrance,
    const Vector3i* p_excluded_access,
    VenueEntranceApproach& r_approach
) {
    bool found = false;
    int best_score = std::numeric_limits<int>::max();
    for (const VenueEntranceApproach& approach :
         r_venue.entrance_approaches) {
        if (!is_road_position(approach.road_access)
            || (p_excluded_entrance && p_excluded_access
                && entrance_approach_matches(
                    approach, *p_excluded_entrance, *p_excluded_access))) {
            continue;
        }
        const int score = manhattan_distance(p_from, approach.entrance) * 4
            + manhattan_distance(
                approach.entrance, approach.road_access);
        if (!found || score < best_score
            || (score == best_score
                && entrance_approach_less(approach, r_approach))) {
            r_approach = approach;
            best_score = score;
            found = true;
        }
    }
    return found;
}

bool CityPopulationDirector::assign_to_venue(
    uint32_t p_entity_id,
    uint64_t p_venue_key
) {
    auto venue_it = active_venues.find(p_venue_key);
    AmbientJourneyData* journey = get_journey(p_entity_id);
    const Entity* entity = ledger
        ? ledger->get_entity_pool().get_entity(p_entity_id)
        : nullptr;
    if (venue_it == active_venues.end() || !journey || !entity
        || journey->venue_key != NO_AMBIENT_VENUE
        || journey->phase != AmbientJourneyPhase::FOLLOWING_ROUTE
        || journey->departing
        || !is_road_position(Vector3i(entity->x, entity->y, entity->z))) {
        return false;
    }
    ActiveVenue& venue = venue_it->second;
    refresh_venue_entrance_approaches(venue);
    if (!venue_has_vacancy(venue)) {
        return false;
    }

    Vector3i activity;
    if (!select_venue_activity(
            p_entity_id, venue, Vector3i(), false, activity)) {
        return false;
    }
    VenueEntranceApproach approach;
    std::vector<Vector3i> approach_route;
    if (!build_road_route_to_venue(
            Vector3i(entity->x, entity->y, entity->z),
            venue,
            nullptr,
            nullptr,
            approach_route,
            approach)) {
        poi_registry->release_for_entity(p_entity_id);
        return false;
    }

    release_road_reservation(p_entity_id);
    journey->venue_key = p_venue_key;
    journey->venue_entrance = approach.entrance;
    journey->venue_road_access = approach.road_access;
    journey->venue_entry_activity = activity;
    journey->venue_leave_at_turn = 0;
    journey->venue_route_failures = 0;
    journey->venue_entrance_reached = false;
    journey->phase = AmbientJourneyPhase::APPROACHING_VENUE;
    journey->detour_attempted = true;
    journey->wants_detour = false;
    journey->blocked_turns = 0;
    journey->route = std::move(approach_route);
    journey->route_index = 1;
    venue.visitors.insert(p_entity_id);
    if (LocomotionData* loco = ledger->try_get_locomotion(p_entity_id)) {
        Locomotion::clear_path(*loco);
    }
    return true;
}

bool CityPopulationDirector::try_assign_existing_road_visitor() {
    if (!has_venue_vacancy()) return false;
    std::vector<uint32_t> ids;
    for (const auto& pair : journeys) {
        const AmbientJourneyData& journey = pair.second;
        if (journey.venue_key == NO_AMBIENT_VENUE
            && journey.phase == AmbientJourneyPhase::FOLLOWING_ROUTE
            && !journey.detour_attempted
            && !journey.departing) {
            ids.push_back(pair.first);
        }
    }
    std::sort(ids.begin(), ids.end());

    int remaining_combinations = 8;
    for (const uint32_t id : ids) {
        if (try_assign_to_vacant_venue(id, remaining_combinations)) {
            return true;
        }
        if (remaining_combinations <= 0) return false;
    }
    return false;
}

bool CityPopulationDirector::try_assign_to_vacant_venue(
    uint32_t p_entity_id,
    int& r_remaining_combinations
) {
    if (r_remaining_combinations <= 0) return false;
    std::vector<uint64_t> venue_keys;
    for (const auto& pair : active_venues) {
        if (venue_has_vacancy(pair.second)) {
            venue_keys.push_back(pair.first);
        }
    }
    std::sort(venue_keys.begin(), venue_keys.end());
    for (const uint64_t venue_key : venue_keys) {
        if (r_remaining_combinations <= 0) return false;
        --r_remaining_combinations;
        if (assign_to_venue(p_entity_id, venue_key)) return true;
    }
    return false;
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
    try_assign_existing_road_visitor();

    if (initialize_ingress_timer
        || p_calendar_turn < next_replenish_turn) {
        return;
    }

    std::vector<Vector3i> batch_entries;
    const int street_population = street_population_count();
    if (street_population > target) return;
    if (street_population < target) {
        spawn_ingress(p_player_position, p_calendar_turn, batch_entries);
    } else if (has_venue_vacancy()) {
        uint32_t entrant_id = EntityPool::INVALID_ID;
        if (spawn_ingress(
                p_player_position,
                p_calendar_turn,
                batch_entries,
                &entrant_id)) {
            int remaining_combinations = 8;
            if (!try_assign_to_vacant_venue(
                    entrant_id, remaining_combinations)) {
                despawn_ambient(entrant_id);
            }
        }
    }
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

bool CityPopulationDirector::should_retain_ambient(
    uint32_t p_entity_id,
    const Vector3i& p_position,
    const Vector2i& p_center
) const {
    const AmbientJourneyData* journey = get_journey(p_entity_id);
    if (journey && journey->venue_key != NO_AMBIENT_VENUE
        && active_venues.find(journey->venue_key) != active_venues.end()) {
        const Vector3i venue_chunk =
            WorldCoords::unpack_coords_3d(journey->venue_key);
        if (WorldCoords::chunk_coord(p_position.x) == venue_chunk.x
            && WorldCoords::chunk_coord(p_position.y) == venue_chunk.y
            && p_position.z == venue_chunk.z) {
            return true;
        }
    }
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
    std::vector<Vector3i> targets;
    for (int oy = -2; oy <= 2; ++oy) {
        for (int ox = -2; ox <= 2; ++ox) {
            const Vector3i candidate(
                p_target.x + ox, p_target.y + oy, p_target.z);
            if (chebyshev_distance(candidate, p_target) > 2
                || !is_road_position(candidate)) {
                continue;
            }
            targets.push_back(candidate);
        }
    }
    return build_road_route_to_any(
        p_start, targets, r_road_route, r_approach);
}

bool CityPopulationDirector::find_alternate_road_step(
    uint32_t p_entity_id,
    const Vector3i& p_current,
    const Vector3i& p_goal,
    Vector3i& r_step
) const {
    const int current_distance = manhattan_distance(p_current, p_goal);
    bool found = false;
    int best_score = std::numeric_limits<int>::max();
    for (const Vector2i& direction : CARDINAL_DIRECTIONS) {
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
    if (journey->phase == AmbientJourneyPhase::APPROACHING_VENUE
        && journey->venue_key != NO_AMBIENT_VENUE) {
        std::vector<Vector3i> venue_route;
        Vector3i selected_access;
        if (!build_road_route_to_any(
                p_start,
                std::vector<Vector3i>{journey->venue_road_access},
                venue_route,
                selected_access)) {
            return false;
        }
        release_road_reservation(p_entity_id);
        journey->route = std::move(venue_route);
        journey->route_index = 1;
        journey->blocked_turns = 0;
        return true;
    }
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
    if (journey->venue_key != NO_AMBIENT_VENUE) {
        const Entity* entity = ledger
            ? ledger->get_entity_pool().get_entity(p_entity_id)
            : nullptr;
        clear_venue_assignment(
            p_entity_id,
            entity && is_road_position(
                Vector3i(entity->x, entity->y, entity->z)));
        return;
    }
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

bool CityPopulationDirector::is_venue_assignment_valid(
    uint32_t p_entity_id
) {
    const AmbientJourneyData* journey = get_journey(p_entity_id);
    if (!journey || journey->venue_key == NO_AMBIENT_VENUE) return false;
    auto venue = active_venues.find(journey->venue_key);
    if (venue == active_venues.end()
        || venue->second.visitors.find(p_entity_id)
            == venue->second.visitors.end()) {
        return false;
    }
    const auto has_approach = [&](const Vector3i& entrance,
                                  const Vector3i& road_access) {
        return std::any_of(
            venue->second.entrance_approaches.begin(),
            venue->second.entrance_approaches.end(),
            [&](const VenueEntranceApproach& approach) {
                return entrance_approach_matches(
                    approach, entrance, road_access);
            });
    };
    if (journey->phase == AmbientJourneyPhase::APPROACHING_VENUE) {
        const PointOfInterestInfo* activity = poi_registry
            ? poi_registry->get(journey->venue_entry_activity)
            : nullptr;
        return activity
            && WorldCoords::chunk_coord(
                journey->venue_entry_activity.x) == venue->second.chunk.x
            && WorldCoords::chunk_coord(
                journey->venue_entry_activity.y) == venue->second.chunk.y
            && journey->venue_entry_activity.z == venue->second.chunk.z
            && has_approach(
                journey->venue_entrance, journey->venue_road_access)
            && is_road_position(journey->venue_road_access)
            && (poi_registry->is_reserved_by(
                    journey->venue_entry_activity, p_entity_id)
                || poi_registry->try_reserve(
                    journey->venue_entry_activity, p_entity_id));
    }
    if (journey->phase == AmbientJourneyPhase::LEAVING_VENUE) {
        return has_approach(
                journey->venue_entrance, journey->venue_road_access)
            && is_road_position(journey->venue_road_access);
    }
    return journey->phase == AmbientJourneyPhase::VENUE_ACTIVITY;
}

bool CityPopulationDirector::begin_venue_activity(uint32_t p_entity_id) {
    AmbientJourneyData* journey = get_journey(p_entity_id);
    AIData* ai = ledger ? ledger->try_get_ai(p_entity_id) : nullptr;
    const Entity* entity = ledger
        ? ledger->get_entity_pool().get_entity(p_entity_id)
        : nullptr;
    auto venue = journey
        ? active_venues.find(journey->venue_key)
        : active_venues.end();
    if (!journey || !ai || !entity || venue == active_venues.end()
        || !world_seed) {
        return false;
    }
    const Vector3i first_activity = journey->venue_entry_activity;
    if (!poi_registry
        || !poi_registry->is_reserved_by(first_activity, p_entity_id)) {
        return false;
    }
    if (journey->venue_leave_at_turn <= 0) {
        Rng::Seeded visit_rng = Rng::at(
            static_cast<uint32_t>(*world_seed),
            Vector2i(entity->x, entity->y),
            Rng::SPAWN,
            Rng::mix64(
                journey->venue_key
                ^ (static_cast<uint64_t>(p_entity_id) << 21)
                ^ static_cast<uint64_t>(latest_calendar_turn)
                ^ UINT64_C(0x71517)));
        journey->venue_leave_at_turn = latest_calendar_turn
            + visit_rng.range(
                venue->second.visit_min, venue->second.visit_max);
    }
    AIController::reset_routine(*ai, true);
    ai->routine_target = first_activity;
    ai->routine_has_target = true;
    ai->routine_phase = RoutinePhase::TRAVELLING;
    journey->venue_entry_activity = Vector3i();
    journey->venue_route_failures = 0;
    journey->venue_entrance_reached = false;
    journey->phase = AmbientJourneyPhase::VENUE_ACTIVITY;
    if (LocomotionData* loco = ledger->try_get_locomotion(p_entity_id)) {
        Locomotion::clear_path(*loco);
    }
    return true;
}

void CityPopulationDirector::clear_venue_routine(uint32_t p_entity_id) {
    if (poi_registry) poi_registry->release_for_entity(p_entity_id);
    AIData* ai = ledger ? ledger->try_get_ai(p_entity_id) : nullptr;
    if (!ai) return;
    AIController::reset_routine(*ai, true);
}

bool CityPopulationDirector::begin_venue_departure(uint32_t p_entity_id) {
    AmbientJourneyData* journey = get_journey(p_entity_id);
    const Entity* entity = ledger
        ? ledger->get_entity_pool().get_entity(p_entity_id)
        : nullptr;
    auto venue = journey
        ? active_venues.find(journey->venue_key)
        : active_venues.end();
    if (!journey || !entity || venue == active_venues.end()) return false;

    refresh_venue_entrance_approaches(venue->second);
    VenueEntranceApproach approach;
    if (!select_departure_approach(
            venue->second,
            Vector3i(entity->x, entity->y, entity->z),
            nullptr,
            nullptr,
            approach)) {
        return false;
    }
    clear_venue_routine(p_entity_id);
    journey->venue_entrance = approach.entrance;
    journey->venue_road_access = approach.road_access;
    journey->venue_route_failures = 0;
    journey->venue_entrance_reached = false;
    journey->phase = AmbientJourneyPhase::LEAVING_VENUE;
    return true;
}

bool CityPopulationDirector::finish_venue_departure(uint32_t p_entity_id) {
    AmbientJourneyData* journey = get_journey(p_entity_id);
    const Entity* entity = ledger
        ? ledger->get_entity_pool().get_entity(p_entity_id)
        : nullptr;
    if (!journey || !entity
        || journey->venue_key == NO_AMBIENT_VENUE) return false;
    reset_venue_assignment_state(p_entity_id, *journey, true);
    return reroute_from(
        p_entity_id, Vector3i(entity->x, entity->y, entity->z));
}

bool CityPopulationDirector::retry_or_abandon_venue(uint32_t p_entity_id) {
    AmbientJourneyData* journey = get_journey(p_entity_id);
    if (!journey || journey->venue_key == NO_AMBIENT_VENUE) return false;
    ++journey->venue_route_failures;
    if (journey->venue_route_failures < 3) {
        auto venue = active_venues.find(journey->venue_key);
        const Entity* entity = ledger
            ? ledger->get_entity_pool().get_entity(p_entity_id)
            : nullptr;
        if (venue != active_venues.end() && entity) {
            refresh_venue_entrance_approaches(venue->second);
            const Vector3i current(entity->x, entity->y, entity->z);
            const Vector3i previous_entrance = journey->venue_entrance;
            const Vector3i previous_access = journey->venue_road_access;
            VenueEntranceApproach approach;
            if (journey->phase
                == AmbientJourneyPhase::APPROACHING_VENUE) {
                if (!is_road_position(current)) return true;
                std::vector<Vector3i> route;
                bool prepared = build_road_route_to_venue(
                    current,
                    venue->second,
                    &previous_entrance,
                    &previous_access,
                    route,
                    approach);
                if (!prepared) {
                    prepared = build_road_route_to_venue(
                        current,
                        venue->second,
                        nullptr,
                        nullptr,
                        route,
                        approach);
                }
                if (prepared) {
                    journey->route = std::move(route);
                    journey->route_index = 1;
                    journey->venue_entrance = approach.entrance;
                    journey->venue_road_access = approach.road_access;
                    journey->venue_entrance_reached = false;
                    journey->blocked_turns = 0;
                    return true;
                }
            } else if (journey->phase
                == AmbientJourneyPhase::LEAVING_VENUE) {
                bool prepared = select_departure_approach(
                    venue->second,
                    current,
                    &previous_entrance,
                    &previous_access,
                    approach);
                if (!prepared) {
                    prepared = select_departure_approach(
                        venue->second,
                        current,
                        nullptr,
                        nullptr,
                        approach);
                }
                if (prepared) {
                    journey->venue_entrance = approach.entrance;
                    journey->venue_road_access = approach.road_access;
                    journey->venue_entrance_reached = false;
                    journey->blocked_turns = 0;
                    return true;
                }
            } else {
                return true;
            }
        }
        return false;
    }
    clear_venue_assignment(p_entity_id, false);
    return false;
}

void CityPopulationDirector::reset_venue_assignment_state(
    uint32_t p_entity_id,
    AmbientJourneyData& r_journey,
    bool p_departing
) {
    auto venue = active_venues.find(r_journey.venue_key);
    if (venue != active_venues.end()) {
        venue->second.visitors.erase(p_entity_id);
    }
    clear_venue_routine(p_entity_id);
    r_journey.venue_key = NO_AMBIENT_VENUE;
    r_journey.venue_entrance = Vector3i();
    r_journey.venue_road_access = Vector3i();
    r_journey.venue_entry_activity = Vector3i();
    r_journey.venue_leave_at_turn = 0;
    r_journey.venue_route_failures = 0;
    r_journey.venue_entrance_reached = false;
    r_journey.wants_detour = false;
    r_journey.detour_attempted = true;
    r_journey.blocked_turns = 0;
    r_journey.phase = AmbientJourneyPhase::FOLLOWING_ROUTE;
    r_journey.departing = p_departing;
}

void CityPopulationDirector::clear_venue_assignment(
    uint32_t p_entity_id,
    bool p_resume_road
) {
    AmbientJourneyData* journey = get_journey(p_entity_id);
    if (!journey) return;
    reset_venue_assignment_state(p_entity_id, *journey, false);
    if (p_resume_road) {
        reroute(p_entity_id);
    } else {
        journey->route.clear();
        journey->route_index = 1;
    }
}

void CityPopulationDirector::release_for_entity(uint32_t p_entity_id) {
    AmbientJourneyData* journey = get_journey(p_entity_id);
    if (journey && journey->venue_key != NO_AMBIENT_VENUE) {
        auto venue = active_venues.find(journey->venue_key);
        if (venue != active_venues.end()) {
            venue->second.visitors.erase(p_entity_id);
        }
    }
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
        AIController::reset_routine(*ai, true);
        ai->home_state = AIState::WANDER;
        if (entity) ai->home_position = Vector2i(entity->x, entity->y);
    }
    if (LocomotionData* loco = ledger->try_get_locomotion(p_entity_id)) {
        Locomotion::clear_path(*loco);
    }
    return true;
}
